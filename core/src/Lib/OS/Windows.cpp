/**
 * @file   Windows.cpp
 * @author pupbrained (mars@pupbrained.dev)
 * @brief  Provides the Windows-specific implementation of the System class for system information retrieval.
 *
 * @details This file contains the concrete implementation of the System class interface for the
 * Microsoft Windows platform. It retrieves a wide range of system information by
 * leveraging various Windows APIs, including:
 * - Standard Win32 API for memory, disk, and process information.
 * - Windows Registry for OS version, host model, CPU details, and package enumeration.
 * - DirectX Graphics Infrastructure (DXGI) for enumerating graphics adapters.
 * - NPSM COM API for media session information (now playing).
 *
 * To optimize performance, the implementation caches process snapshots and registry
 * handles. Wide strings are used for all string operations to avoid the overhead of
 * converting between UTF-8 and UTF-16 until the final result is needed.
 *
 * @see draconis::core::system
 */

#ifdef _WIN32

  #if defined(_MSC_VER) || defined(__clang__)
    #include <intrin.h> // __cpuid (MSVC/Clang-cl intrinsic)
  #endif

  #include <dxgi.h>       // IDXGIFactory, IDXGIAdapter, DXGI_ADAPTER_DESC
  #include <limits>       // std::numeric_limits
  #include <ranges>       // std::ranges::find_if, std::ranges::views::transform
  #include <sysinfoapi.h> // GetLogicalProcessorInformationEx, RelationProcessorCore, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, KAFFINITY
  #include <tlhelp32.h>   // CreateToolhelp32Snapshot, PROCESSENTRY32W, Process32FirstW, Process32NextW, TH32CS_SNAPPROCESS
  #include <winerror.h>   // DXGI_ERROR_NOT_FOUND, ERROR_FILE_NOT_FOUND, FAILED
  #include <winuser.h>    // EnumDisplayMonitors, GetMonitorInfoW, MonitorFromWindow, EnumDisplaySettingsW

  // Core Winsock headers
  #include <winsock2.h> // AF_INET, AF_UNSPEC, sockaddr_in
  #include <ws2tcpip.h> // inet_ntop, inet_pton

  // IP Helper API headers
  #include <iphlpapi.h> // GetAdaptersAddresses, GetBestRoute
  #include <iptypes.h>  // GAA_FLAG_INCLUDE_PREFIX, IP_ADAPTER_ADDRESSES, IP_ADAPTER_UNICAST_ADDRESS

  // COM smart pointer support
  #include <wrl/client.h> // Microsoft::WRL::ComPtr

  #include "Drac++/Core/System.hpp"

  #if DRAC_ENABLE_PACKAGECOUNT
    #include "Drac++/Services/Packages.hpp"
  #endif

  #include "Drac++/Utils/CacheManager.hpp"
  #include "Drac++/Utils/Env.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Types.hpp"
  #include "Drac++/Utils/WindowsTopology.hpp"

namespace {
  using draconis::utils::error::DracError;
  using enum draconis::utils::error::DracErrorCode;
  using namespace draconis::utils::types;
  using draconis::utils::cache::CacheManager;

  namespace constants {
    // Registry keys for Windows version information
    constexpr PWCStr PRODUCT_NAME        = L"ProductName";
    constexpr PWCStr DISPLAY_VERSION     = L"DisplayVersion";
    constexpr PWCStr SYSTEM_FAMILY       = L"SystemFamily";
    constexpr PWCStr SYSTEM_PRODUCT_NAME = L"SystemProductName";

    // clang-format off
    constexpr Array<Pair<StringView, StringView>, 5> windowsShellMap = {{
      {      "cmd",     "Command Prompt" },
      { "powershell",       "PowerShell" },
      {       "pwsh",  "PowerShell Core" },
      {         "wt", "Windows Terminal" },
      {   "explorer", "Windows Explorer" },
    }};

    constexpr Array<Pair<StringView, StringView>, 7> msysShellMap = {{
      { "bash",      "Bash" },
      {  "zsh",       "Zsh" },
      { "fish",      "Fish" },
      {   "sh",        "sh" },
      {  "ksh", "KornShell" },
      { "tcsh",      "tcsh" },
      { "dash",      "dash" },
    }};

    constexpr Array<Pair<StringView, StringView>, 4> windowManagerMap = {{
      {     "glazewm",   "GlazeWM" },
      {    "komorebi",  "Komorebi" },
      {   "seelen-ui", "Seelen UI" },
      { "slu-service", "Seelen UI" },
    }};
    // clang-format on
  } // namespace constants

  namespace helpers {
    /**
     * @brief Converts a wide string (UTF-16) to a UTF-8 encoded string.
     * @details Uses thread-local buffers to minimize allocations during repeated conversions.
     *          The buffer grows as needed but is reused across calls within the same thread.
     * @param wstr The wide string to convert.
     * @return Result containing the UTF-8 string on success, or an error on failure.
     */
    auto ConvertWStringToUTF8(const WString& wstr) -> Result<String> {
      // Return early for empty strings to avoid unnecessary work.
      if (wstr.empty())
        return String {};

      // Thread-local buffer for reuse across repeated calls.
      // This avoids repeated heap allocations in hot paths.
      thread_local String TlsBuffer;

      // First call WideCharToMultiByte to get the required buffer size.
      const i32 sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<i32>(wstr.length()), nullptr, 0, nullptr, nullptr);

      if (sizeNeeded == 0)
        ERR_FMT(InternalError, "Failed to get buffer size for UTF-8 conversion. Error code: {}", GetLastError());

      // Resize the thread-local buffer only if it's too small.
      // This grows the buffer over time to accommodate the largest string seen,
      // minimizing reallocations across multiple calls.
      if (TlsBuffer.size() < static_cast<usize>(sizeNeeded))
        TlsBuffer.resize(static_cast<usize>(sizeNeeded));

      // Convert the wide string to UTF-8 using the thread-local buffer.
      const i32 bytesConverted =
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<i32>(wstr.length()), TlsBuffer.data(), sizeNeeded, nullptr, nullptr);

      if (bytesConverted == 0)
        ERR_FMT(InternalError, "Failed to convert wide string to UTF-8. Error code: {}", GetLastError());

      // Return a copy of the converted portion. The copy is necessary since
      // the caller owns the returned string and we must preserve the TLS buffer.
      return String(TlsBuffer.data(), static_cast<usize>(bytesConverted));
    }

    auto GetDirCount(const WString& path) -> Result<u64> {
      // Create mutable copy and append wildcard.
      WString searchPath(path);
      searchPath.append(L"\\*");

      // Used to receive information about the found file or directory.
      WIN32_FIND_DATAW findData;

      // Begin searching for files and directories.
      HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

      if (hFind == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
          return 0;

        ERR(IoError, "FindFirstFileW failed");
      }

      u64 count = 0;

      while (hFind != INVALID_HANDLE_VALUE) {
        // Only increment if the found item is:
        // 1. a directory
        // 2. not a special directory (".", "..")
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
          count++;

        // Continue searching for more files and directories.
        if (!FindNextFileW(hFind, &findData))
          break;
      }

      // Ensure that the handle is closed to avoid leaks.
      FindClose(hFind);

      return count;
    }

    // Reads a registry value into a WString.
    auto GetRegistryValue(const HKEY& hKey, const WString& valueName) -> Result<WString> {
      // Buffer for storing the registry value. Should be large enough to hold most values.
      Array<WCStr, 1024> registryBuffer {};

      // Size of the buffer in bytes.
      DWORD dataSizeInBytes = registryBuffer.size() * sizeof(WCStr);

      // Stores the type of the registry value.
      DWORD type = 0;

      // Query the registry value.
      if (const LSTATUS status = RegQueryValueExW(hKey, valueName.c_str(), nullptr, &type,
                                                  // NOLINTNEXTLINE(*-pro-type-reinterpret-cast) - reinterpret_cast is required to convert the buffer to a byte array.
                                                  reinterpret_cast<LPBYTE>(registryBuffer.data()),
                                                  &dataSizeInBytes);
          status != ERROR_SUCCESS) {
        if (status == ERROR_FILE_NOT_FOUND)
          ERR(NotFound, "Registry value not found");

        if (status == ERROR_ACCESS_DENIED)
          ERR(PermissionDenied, "Permission denied while reading registry value");

        ERR_FMT(PlatformSpecific, "RegQueryValueExW failed with error code: {}", status);
      }

      // Ensure the retrieved value is a string.
      if (type == REG_SZ || type == REG_EXPAND_SZ) {
        const usize length = dataSizeInBytes / sizeof(WCStr);
        if (dataSizeInBytes % sizeof(WCStr) != 0 || length > registryBuffer.size())
          ERR(ParseError, "Invalid registry string size");
        const auto end = std::find(registryBuffer.begin(), registryBuffer.begin() + length, L'\0');
        return WString(registryBuffer.begin(), end);
      }

      ERR_FMT(ParseError, "Registry value exists but is not a string type. Type is: {}", type);
    }

  } // namespace helpers

  namespace cache {
    // RAII wrapper for registry keys
    class RegistryKey {
     public:
      explicit RegistryKey(HKEY key) : m_key(key) {}

      ~RegistryKey() {
        if (m_key)
          RegCloseKey(m_key);
      }

      auto operator=(RegistryKey&& other) noexcept -> RegistryKey& {
        if (this != &other) {
          if (m_key)
            RegCloseKey(m_key);
          m_key       = other.m_key;
          other.m_key = nullptr;
        }
        return *this;
      }

      [[nodiscard]] auto get() const -> HKEY {
        return m_key;
      }

      [[nodiscard]] explicit operator bool() const {
        return m_key != nullptr;
      }

      RegistryKey(const RegistryKey&)                    = delete;
      RegistryKey(RegistryKey&&)                         = delete;
      auto operator=(const RegistryKey&) -> RegistryKey& = delete;

     private:
      HKEY m_key = nullptr;
    };

    // Caches registry values, allowing them to only be retrieved once.
    class RegistryCache {
     public:
      RegistryCache() : m_currentVersionKey(nullptr), m_hardwareConfigKey(nullptr) {
        // Attempt to open the registry key for Windows version information.
        HKEY currentVersionKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &currentVersionKey) == ERROR_SUCCESS)
          m_currentVersionKey = RegistryKey(currentVersionKey);

        // Attempt to open the registry key for hardware configuration.
        HKEY hardwareConfigKey = nullptr;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\CrashControl\\MachineCrash", 0, KEY_READ, &hardwareConfigKey) == ERROR_SUCCESS)
          m_hardwareConfigKey = RegistryKey(hardwareConfigKey);
      }

      static auto getInstance() -> const RegistryCache& {
        static const RegistryCache INSTANCE;
        return INSTANCE;
      }

      [[nodiscard]] auto getCurrentVersionKey() const -> HKEY {
        return m_currentVersionKey.get();
      }
      [[nodiscard]] auto getHardwareConfigKey() const -> HKEY {
        return m_hardwareConfigKey.get();
      }

      ~RegistryCache()                                       = default;
      RegistryCache(const RegistryCache&)                    = delete;
      RegistryCache(RegistryCache&&)                         = delete;
      auto operator=(const RegistryCache&) -> RegistryCache& = delete;
      auto operator=(RegistryCache&&) -> RegistryCache&      = delete;

     private:
      RegistryKey m_currentVersionKey;
      RegistryKey m_hardwareConfigKey;
    };

    // Caches OS version data for use in other functions.
    class OsVersionCache {
     public:
      struct VersionData {
        u32 majorVersion;
        u32 minorVersion;
        u32 buildNumber;
      };

      static auto getInstance() -> const OsVersionCache& {
        static const OsVersionCache INSTANCE;
        return INSTANCE;
      }

      [[nodiscard]] auto getVersionData() const -> const Result<VersionData>&;

      [[nodiscard]] auto getBuildNumber() const -> Result<u64> {
        if (!m_versionData)
          ERR_FROM(m_versionData.error());

        return static_cast<u64>(m_versionData->buildNumber);
      }

      OsVersionCache(const OsVersionCache&)                    = delete;
      OsVersionCache(OsVersionCache&&)                         = delete;
      auto operator=(const OsVersionCache&) -> OsVersionCache& = delete;
      auto operator=(OsVersionCache&&) -> OsVersionCache&      = delete;

     private:
      Result<VersionData> m_versionData;

      // Helper struct for SEH-safe version reading (POD only - no C++ objects requiring unwinding)
      struct VersionReadResult {
        u32  majorVersion;
        u32  minorVersion;
        u32  buildNumber;
        bool success;
      };

      // SEH helper function - must not use any C++ objects that require unwinding
      // This is separated because MSVC's __try/__except cannot be used in functions
      // with objects that have destructors.
      static auto readVersionDataSEH() -> VersionReadResult {
        // KUSER_SHARED_DATA is a block of memory shared between the kernel and user-mode
        // processes. This address has not changed since its inception. It SHOULD always
        // contain data for the running Windows version.
        constexpr ULONG_PTR kuserSharedData = 0x7FFE0000;

        // These offsets should also be static/consistent across different versions of Windows.
        constexpr u32 kuserSharedNtMajorVersion = kuserSharedData + 0x26C;
        constexpr u32 kuserSharedNtMinorVersion = kuserSharedData + 0x270;
        constexpr u32 kuserSharedNtBuildNumber  = kuserSharedData + 0x260;

        VersionReadResult result = {
          .majorVersion = 0,
          .minorVersion = 0,
          .buildNumber  = 0,
          .success      = false,
        };

        // Considering this file is windows-specific, it's fine to use windows-specific extensions.
  #if defined(_MSC_VER) && defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wlanguage-extension-token"
  #endif
        // Use Structured Exception Handling (SEH) to safely read the version data. In case of invalid
        // pointers, this will catch the access violation and return an Error, instead of crashing.
  #ifdef _MSC_VER
        __try {
  #endif
          // Read the version data directly from the calculated memory addresses.
          // - reinterpret_cast is required to cast the memory addresses to volatile pointers.
          // - `volatile` tells the compiler that these memory reads should not be optimized away.
          // NOLINTBEGIN(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
          result.majorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMajorVersion);
          result.minorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMinorVersion);
          result.buildNumber  = *reinterpret_cast<const volatile u32*>(kuserSharedNtBuildNumber);
          // NOLINTEND(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
          result.success = true;
  #ifdef _MSC_VER
        } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
          // If an access violation occurs, then the shared memory couldn't be properly read.
          result.success = false;
        }
  #endif
  #if defined(_MSC_VER) && defined(__clang__)
    #pragma clang diagnostic pop
  #endif

        return result;
      }

      // Fetching version data from KUSER_SHARED_DATA is the fastest way to get the version information.
      // It also avoids the need for a system call or registry access. The biggest downside, though, is
      // that it's inherently risky/unsafe, and could break in future updates. To mitigate this risk,
      // the SEH helper function handles potential exceptions and returns a POD struct.
      OsVersionCache() {
        const VersionReadResult readResult = readVersionDataSEH();

        if (readResult.success) {
          m_versionData = VersionData {
            .majorVersion = readResult.majorVersion,
            .minorVersion = readResult.minorVersion,
            .buildNumber  = readResult.buildNumber
          };
        } else {
          // If an access violation occurs, then the shared memory couldn't be properly read.
          // Set the version data to an error instead of crashing.
          m_versionData = Err(DracError(InternalError, "Failed to read kernel version from KUSER_SHARED_DATA"));
        }
      }

      ~OsVersionCache() = default;
    };
    auto OsVersionCache::getVersionData() const -> const Result<VersionData>& {
      return m_versionData;
    }

    // RAII wrapper for Windows handles
    template <typename HandleType>
    class HandleWrapper {
     public:
      explicit HandleWrapper(HandleType handle) : m_handle(handle) {}

      ~HandleWrapper() {
        if (m_handle && m_handle != INVALID_HANDLE_VALUE)
          CloseHandle(m_handle);
      }

      auto operator=(HandleWrapper&& other) noexcept -> HandleWrapper& {
        if (this != &other) {
          if (m_handle && m_handle != INVALID_HANDLE_VALUE)
            CloseHandle(m_handle);

          m_handle       = other.m_handle;
          other.m_handle = nullptr;
        }

        return *this;
      }

      [[nodiscard]] auto get() const -> HandleType {
        return m_handle;
      }

      [[nodiscard]] explicit operator bool() const {
        return m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE;
      }

      HandleWrapper(const HandleWrapper&)                    = delete;
      HandleWrapper(HandleWrapper&&)                         = delete;
      auto operator=(const HandleWrapper&) -> HandleWrapper& = delete;

     private:
      HandleType m_handle = nullptr;
    };

    // Caches process snapshots and process tree information.
    class ProcessTreeCache {
     public:
      struct Data {
        DWORD  parentPid = 0;
        String baseExeNameLower;
      };

      static auto getInstance() -> ProcessTreeCache& {
        static ProcessTreeCache Instance;
        return Instance;
      }

      auto initialize() -> Result<> {
        bool initSuccess = false;

        // Use std::call_once for thread-safe initialization
        std::call_once(m_initFlag, [this, &initSuccess]() -> void {
          debug_log("ProcessTreeCache: Starting initialization...");

          // Use the Toolhelp32Snapshot API to get a snapshot of all running processes.
          const HandleWrapper<HANDLE> hSnap(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));

          if (!hSnap) {
            debug_log("ProcessTreeCache: CreateToolhelp32Snapshot failed, error: {}", GetLastError());
            return;
          }

          // This structure must be initialized with its own size before use; it's a WinAPI requirement.
          PROCESSENTRY32W pe32 {};
          pe32.dwSize = sizeof(PROCESSENTRY32W);

          // Get the first process from the snapshot.
          if (!Process32FirstW(hSnap.get(), &pe32)) {
            debug_log("ProcessTreeCache: Process32FirstW failed, error: {}", GetLastError());
            return;
          }

          UnorderedMap<DWORD, Data> processMap;

          while (true) {
            // Extract the executable name from the full path.
            const WStringView exeFileView(pe32.szExeFile);

            // Find the last backslash to get just the executable name.
            const usize start = exeFileView.find_last_of(L'\\') != WStringView::npos ? exeFileView.find_last_of(L'\\') + 1 : 0;
            const usize end   = exeFileView.length();

            WStringView stemView = exeFileView.substr(start, end - start);

            // Remove .exe extension if present
            if (stemView.size() > 4 && stemView.substr(stemView.size() - 4) == L".exe")
              stemView = stemView.substr(0, stemView.size() - 4);

            WString baseName(stemView);
            std::ranges::transform(baseName, baseName.begin(), [](const WCStr character) -> wint_t { return towlower(character); });

            if (const Result<String> baseNameUTF8 = helpers::ConvertWStringToUTF8(baseName))
              processMap.insert_or_assign(pe32.th32ProcessID, Data { .parentPid = pe32.th32ParentProcessID, .baseExeNameLower = *baseNameUTF8 });

            if (!Process32NextW(hSnap.get(), &pe32))
              break;
          }

          // Atomic update of the process map
          {
            const std::scoped_lock lock(m_processMutex);
            m_processMap = std::move(processMap);
          }

          initSuccess = true;
          debug_log("ProcessTreeCache: Initialized with {} processes", m_processMap.size());
        });

        if (!initSuccess && m_processMap.empty()) {
          debug_log("ProcessTreeCache: Initialization failed or map is empty");
          ERR(IoError, "Failed to initialize process tree cache");
        }

        return {};
      }

      auto getProcessMap() const -> const UnorderedMap<DWORD, Data>& {
        const std::scoped_lock lock(m_processMutex);
        return m_processMap;
      }

      ProcessTreeCache(const ProcessTreeCache&)                    = delete;
      ProcessTreeCache(ProcessTreeCache&&)                         = delete;
      auto operator=(const ProcessTreeCache&) -> ProcessTreeCache& = delete;
      auto operator=(ProcessTreeCache&&) -> ProcessTreeCache&      = delete;

     private:
      UnorderedMap<DWORD, Data> m_processMap;
      mutable Mutex             m_processMutex;
      std::once_flag            m_initFlag;

      ProcessTreeCache()  = default;
      ~ProcessTreeCache() = default;
    };
  } // namespace cache

  namespace shell {
    template <usize sz>
    auto FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap) -> Result<String> {
      using cache::ProcessTreeCache;

      debug_log("FindShellInProcessTree: Starting with PID {}", startPid);

      // PID 0 (System Idle Process) is always the root process, and cannot have a parent.
      if (startPid == 0)
        ERR(InvalidArgument, "Start PID cannot be 0");

      TRY_VOID(ProcessTreeCache::getInstance().initialize());

      const UnorderedMap<DWORD, ProcessTreeCache::Data>& processMap = ProcessTreeCache::getInstance().getProcessMap();
      debug_log("FindShellInProcessTree: Process map has {} entries", processMap.size());

      DWORD currentPid = startPid;

      // This is a pretty reasonable depth and should cover most cases without excessive recursion.
      constexpr i32 maxDepth = 16;

      i32 depth = 0;

      while (currentPid != 0 && depth < maxDepth) {
        auto procIt = processMap.find(currentPid);
        if (procIt == processMap.end()) {
          debug_log("FindShellInProcessTree: PID {} not found in map, stopping traversal", currentPid);
          break;
        }

        // Get the lowercase name of the process.
        const String& processName = procIt->second.baseExeNameLower;
        debug_log("FindShellInProcessTree: depth={}, PID={}, name={}", depth, currentPid, processName);

        // Check if the process name matches any shell in the map,
        // and return its friendly-name counterpart if it is.
        if (
          const auto mapIter =
            std::ranges::find_if(shellMap, [&](const Pair<StringView, StringView>& pair) -> auto { return StringView { processName } == pair.first; });
          mapIter != std::ranges::end(shellMap)
        ) {
          debug_log("FindShellInProcessTree: Found shell: {}", mapIter->second);
          return String(mapIter->second);
        }

        // Move up the tree to the parent process.
        currentPid = procIt->second.parentPid;
        depth++;
      }

      debug_log("FindShellInProcessTree: Shell not found after {} iterations", depth);
      ERR(NotFound, "Shell not found");
    }
  } // namespace shell

  auto GetDiskInfoForDrive(const String& driveRoot, CacheManager& /*cache*/) -> Result<DiskInfo> {
    DiskInfo disk;

    // Set name and mount point (same for Windows drives)
    disk.name       = driveRoot;
    disk.mountPoint = driveRoot;

    // Get drive type
    UINT const driveType = GetDriveTypeA(driveRoot.c_str());
    switch (driveType) {
      case DRIVE_FIXED:
        disk.driveType = "Fixed";
        break;
      case DRIVE_REMOVABLE:
        disk.driveType = "Removable";
        break;
      case DRIVE_CDROM:
        disk.driveType = "CD-ROM";
        break;
      case DRIVE_REMOTE:
        disk.driveType = "Network";
        break;
      case DRIVE_RAMDISK:
        disk.driveType = "RAM Disk";
        break;
      default:
        disk.driveType = "Unknown";
        break;
    }

    // Get filesystem type
    Array<char, MAX_PATH> filesystem = {};
    if (GetVolumeInformationA(driveRoot.c_str(), nullptr, 0, nullptr, nullptr, nullptr, filesystem.data(), MAX_PATH))
      disk.filesystem = filesystem.data();
    else
      disk.filesystem = "Unknown";

    // Get disk space information
    ULARGE_INTEGER freeBytes, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceExA(driveRoot.c_str(), &freeBytes, &totalBytes, &totalFreeBytes)) {
      disk.totalBytes = totalBytes.QuadPart;
      disk.usedBytes  = totalBytes.QuadPart - freeBytes.QuadPart;
    } else {
      // If we can't get space info, set to 0
      disk.totalBytes = 0;
      disk.usedBytes  = 0;
    }

    // Get system drive for comparison
    Array<char, MAX_PATH> systemDir = {};
    GetSystemDirectoryA(systemDir.data(), MAX_PATH);
    const char systemDrive = systemDir.front();

    // Check if this is the system drive
    disk.isSystemDrive = (driveRoot.front() == systemDrive);

    return disk;
  }
} // namespace

namespace draconis::core::system {
  using namespace draconis::utils::types;
  using draconis::utils::cache::CacheManager;
  using namespace cache;
  using namespace constants;
  using namespace helpers;

  auto GetMemInfo(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    // Passed to GlobalMemoryStatusEx to retrieve memory information.
    // dwLength is required to be set as per WinAPI.
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo))
      return ResourceUsage(memInfo.ullTotalPhys - memInfo.ullAvailPhys, memInfo.ullTotalPhys);

    ERR_FMT(ApiUnavailable, "GlobalMemoryStatusEx failed with error code {}", GetLastError());
  }

  auto GetOperatingSystem(CacheManager& cache) -> Result<OSInfo> {
    return cache.getOrSet<OSInfo>("windows_os_version", []() -> Result<OSInfo> {
      // Windows is weird about its versioning scheme, and Windows 11 is still
      // considered Windows 10 in the registry. We have to manually check if
      // the actual version is Windows 11 by checking the build number.
      constexpr PWCStr windows10 = L"Windows 10";
      constexpr PWCStr windows11 = L"Windows 11";

      constexpr usize windowsLen = std::char_traits<WCStr>::length(windows10);

      const RegistryCache& registry = RegistryCache::getInstance();

      HKEY currentVersionKey = registry.getCurrentVersionKey();

      if (!currentVersionKey)
        ERR(NotFound, "Failed to open registry key");

      WString productName = TRY(GetRegistryValue(currentVersionKey, PRODUCT_NAME));

      if (productName.empty())
        ERR(NotFound, "ProductName not found in registry");

      // Build 22000+ of Windows are all considered Windows 11, so we can safely replace the product name
      // if it's currently "Windows 10" and the build number is greater than or equal to 22000.
      if (const Result<u64> buildNumberOpt = OsVersionCache::getInstance().getBuildNumber())
        if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000)
          if (const size_t pos = productName.find(windows10); pos != WString::npos) {
            // Make sure we're not replacing a substring of a larger string. Should never happen,
            // but if it ever does, we'll just leave the product name unchanged.
            const bool startBoundary = (pos == 0 || !iswalnum(productName.at(pos - 1)));
            const bool endBoundary   = (pos + windowsLen == productName.length() || !iswalnum(productName.at(pos + windowsLen)));

            if (startBoundary && endBoundary)
              productName.replace(pos, windowsLen, windows11);
          }

      // Append the display version if it exists.
      WString displayVersion = TRY(GetRegistryValue(currentVersionKey, DISPLAY_VERSION));

      const String productNameUTF8 = TRY(ConvertWStringToUTF8(productName));

      const String displayVersionUTF8 = TRY(ConvertWStringToUTF8(displayVersion));

      return OSInfo(productNameUTF8, displayVersionUTF8, "windows");
    });
  }

  auto GetHost(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_host", draconis::utils::cache::CachePolicy::inMemory(), []() -> Result<String> {
      // Read from BIOS registry key which contains system product information
      HKEY biosKey = nullptr;

      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &biosKey) != ERROR_SUCCESS)
        ERR(NotFound, "Failed to open BIOS registry key");

      const RegistryKey biosKeyGuard(biosKey);

      // Try SystemFamily first (e.g., "ASUS TUF Gaming F15"), then fall back to SystemProductName
      if (Result<WString> systemFamily = GetRegistryValue(biosKey, SYSTEM_FAMILY); systemFamily)
        return ConvertWStringToUTF8(*systemFamily);

      if (Result<WString> productName = GetRegistryValue(biosKey, SYSTEM_PRODUCT_NAME); productName)
        return ConvertWStringToUTF8(*productName);

      ERR(NotFound, "Failed to get system family or product name from BIOS registry");
    });
  }

  auto GetKernelVersion(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_kernel_version", draconis::utils::cache::CachePolicy::tempDirectory(), []() -> Result<String> {
      // See the OsVersionCache class for how the version data is retrieved.
      const auto& [majorVersion, minorVersion, buildNumber] = TRY(OsVersionCache::getInstance().getVersionData());

      return std::format("{}.{}.{}", majorVersion, minorVersion, buildNumber);
    });
  }

  auto GetWindowManager(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_wm", draconis::utils::cache::CachePolicy::neverExpire(), []() -> Result<String> {
      if (!cache::ProcessTreeCache::getInstance().initialize())
        ERR(PlatformSpecific, "Failed to initialize process tree cache");

      for (const auto& [parentPid, baseExeNameLower] : cache::ProcessTreeCache::getInstance().getProcessMap() | std::views::values) {
        const StringView processName = baseExeNameLower;

        if (
          const auto mapIter =
            std::ranges::find_if(windowManagerMap, [&](const Pair<StringView, StringView>& pair) -> bool {
              return processName == pair.first;
            });
          mapIter != std::ranges::end(windowManagerMap)
        )
          return String(mapIter->second);
      }

      return "DWM";
    });
  }

  auto GetDesktopEnvironment(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_desktop_environment", draconis::utils::cache::CachePolicy::neverExpire(), []() -> Result<String> {
      // Windows doesn't really have the concept of a desktop environment,
      // so our next best bet is just displaying the UI design based on the build number.

      const u64 build = TRY(OsVersionCache::getInstance().getBuildNumber());

      if (build >= 15063)
        return "Fluent";

      if (build >= 9200)
        return "Metro";

      if (build >= 6000)
        return "Aero";

      return "Classic";
    });
  }

  auto GetShell(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_shell", draconis::utils::cache::CachePolicy::tempDirectory(), []() -> Result<String> {
      using draconis::utils::env::GetEnv;
      using shell::FindShellInProcessTree;

      // MSYS2 environments automatically set the MSYSTEM environment variable.
      if (const Result<String> msystemResult = GetEnv("MSYSTEM"); msystemResult && !msystemResult->empty()) {
        String shellPath;

        // The SHELL environment variable should basically always be set.
        if (const Result<String> shellResult = GetEnv("SHELL"); shellResult && !shellResult->empty())
          shellPath = *shellResult;

        if (!shellPath.empty()) {
          // Get the executable name from the path.
          const usize lastSlash = shellPath.find_last_of("\\/");
          String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;

          std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) -> int { return std::tolower(character); });

          // Remove the .exe extension if it exists.
          if (shellExe.ends_with(".exe"))
            shellExe.resize(shellExe.length() - 4);

          // Check if the executable name matches any shell in the map.
          if (
            const auto iter =
              std::ranges::find_if(msysShellMap, [&](const Pair<StringView, StringView>& pair) -> bool {
                return StringView { shellExe } == pair.first;
              });
            iter != std::ranges::end(msysShellMap)
          )
            return String(iter->second);

          // If the executable name doesn't match any shell in the map, we might as well just return it as is.
          return shellExe;
        }

        // If the SHELL environment variable is not set, we can fall back to checking the process tree.
        // This is slower, but if we don't have the SHELL variable there's not much else we can do.
        if (Result<String> shellResult = FindShellInProcessTree(GetCurrentProcessId(), msysShellMap))
          return *std::move(shellResult);

        // If we still can't find it, return "Unknown"
        return String("Unknown");
      }

      // Normal windows shell environments don't set any environment variables we can check,
      // so we have to check the process tree instead.
      if (Result<String> shellResult = FindShellInProcessTree(GetCurrentProcessId(), windowsShellMap))
        return *std::move(shellResult);

      // If we can't find a shell, return "Unknown" instead of failing
      return String("Unknown");
    });
  }

  auto GetDiskUsage(CacheManager& /*cache*/) -> Result<ResourceUsage> {
    // GetDiskFreeSpaceExW is a pretty old function and doesn't use native 64-bit integers,
    // so we have to use ULARGE_INTEGER instead. It's basically a union that holds either a
    // 64-bit integer or two 32-bit integers.
    ULARGE_INTEGER freeBytes {}, totalBytes {};

    // Get the disk usage for the C: drive.
    Array<wchar_t, MAX_PATH> windowsDirectory {};
    const UINT               length = GetWindowsDirectoryW(windowsDirectory.data(), windowsDirectory.size());
    if (length == 0 || length >= windowsDirectory.size())
      ERR_FMT(IoError, "Failed to locate Windows directory: {}", GetLastError());
    if (!GetDiskFreeSpaceExW(windowsDirectory.data(), nullptr, &totalBytes, &freeBytes))
      ERR_FMT(IoError, "Failed to get disk usage: {}", GetLastError());

    // Calculate the used bytes by subtracting the free bytes from the total bytes.
    // QuadPart corresponds to the 64-bit integer in the union. (LowPart/HighPart are for the 32-bit integers.)
    return ResourceUsage(totalBytes.QuadPart - freeBytes.QuadPart, totalBytes.QuadPart);
  }

  auto GetDisks(CacheManager& cache) -> Result<Vec<DiskInfo>> {
    Array<char, MAX_PATH> drives = {};

    DWORD const size = GetLogicalDriveStringsA(MAX_PATH, drives.data());

    if (size == 0)
      ERR(IoError, "Failed to get logical drive strings");

    Vec<DiskInfo> disks;

    usize index = 0;
    while (index < MAX_PATH && drives.at(index) != '\0') {
      const char* drive = std::addressof(drives.at(index));

      const Result<DiskInfo> diskInfo = GetDiskInfoForDrive(drive, cache);
      if (diskInfo)
        disks.push_back(*diskInfo);

      // Move to next drive string (skip null terminator)
      index += static_cast<usize>(strlen(drive)) + 1;
    }

    return disks;
  }

  auto GetSystemDisk(CacheManager& cache) -> Result<DiskInfo> {
    // Get the system drive letter directly
    Array<char, MAX_PATH> systemDir = {};
    if (GetSystemDirectoryA(systemDir.data(), MAX_PATH) == 0)
      ERR(IoError, "Failed to get system directory");

    const char   systemDrive = systemDir.front();
    const String driveStr    = String(1, systemDrive) + ":\\";

    return GetDiskInfoForDrive(driveStr, cache);
  }

  auto GetDiskByPath(const String& path, CacheManager& cache) -> Result<DiskInfo> {
    if (path.empty())
      ERR(InvalidArgument, "Path cannot be empty");

    char driveLetter = '\0';
    if (path.length() >= 2 && path.at(1) == ':') {
      driveLetter = path.front();
    } else {
      Array<char, MAX_PATH> cwd = {};
      if (GetCurrentDirectoryA(MAX_PATH, cwd.data()) == 0)
        ERR(IoError, "Failed to get current directory");

      driveLetter = cwd.front();
    }

    if (driveLetter >= 'a' && driveLetter <= 'z') {
      const int temp = static_cast<int>(driveLetter) - 'a' + 'A';
      driveLetter    = static_cast<char>(temp);
    }

    const String driveRoot = String(1, driveLetter) + ":\\";

    return GetDiskInfoForDrive(driveRoot, cache);
  }

  auto GetCPUModel(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_cpu_model", draconis::utils::cache::CachePolicy::inMemory(), []() -> Result<String> {
      /*
       * This function attempts to get the CPU model name on Windows in two ways:
       * 1. Using __cpuid on x86/x86_64 platforms (much more direct and efficient).
       * 2. Reading from the registry on all platforms (slower, but more reliable).
       */
      if constexpr (DRAC_ARCH_X86_64 || DRAC_ARCH_X86) {
        /*
         * The CPUID instruction is used to get the CPU model name on x86/x86_64 platforms.
         * 1. First, we call CPUID with leaf 0x80000000 to ask the CPU if it supports the
         *    extended functions needed to retrieve the brand string.
         * 2. If it does, we then make three more calls to retrieve the 48-byte brand
         *    string, which the CPU provides in three 16-byte chunks.
         *
         * (In this context, a "leaf" is basically just an action we ask the CPU to perform.)
         */

        // Array to hold the raw 32-bit values from the EAX, EBX, ECX, and EDX registers.
        Array<i32, 4> cpuInfo = {};

        // Buffer to hold the raw 48-byte brand string + null terminator.
        Array<char, 49> brandString = {};

        // Step 1: Check for brand string support. The result is returned in EAX (cpuInfo[0]).
  #if defined(_MSC_VER) || (defined(__clang__) && defined(_WIN32))
        // Use __cpuidex to avoid conflict with Clang's __cpuid macro from cpuid.h
        __cpuidex(cpuInfo.data(), static_cast<i32>(0x80000000), 0);
  #else
        __cpuid(0x80000000, cpuInfo.at(0), cpuInfo.at(1), cpuInfo.at(2), cpuInfo.at(3));
  #endif

        // We must have extended functions support (functions up to 0x80000004).
        if (const u32 maxFunction = cpuInfo.at(0); maxFunction >= 0x80000004) {
          // Retrieve the brand string in three 16-byte parts.
          for (u32 i = 0; i < 3; i++) {
            // Call leaves 0x80000002, 0x80000003, and 0x80000004. Each call
            // returns a 16-byte chunk of the brand string.
  #if defined(_MSC_VER) || (defined(__clang__) && defined(_WIN32))
            // Use __cpuidex to avoid conflict with Clang's __cpuid macro from cpuid.h
            __cpuidex(cpuInfo.data(), static_cast<i32>(0x80000002 + i), 0);
  #else
            __cpuid(0x80000002 + i, cpuInfo.at(0), cpuInfo.at(1), cpuInfo.at(2), cpuInfo.at(3));
  #endif

            // Copy the chunk into the brand string buffer.
            std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
          }

          String result(brandString.data());

          // Clean up any possible trailing whitespace.
          while (!result.empty() && std::isspace(result.back()))
            result.pop_back();

          // We're done if we got a valid, non-empty string.
          // Otherwise, fallback to querying the registry.
          if (!result.empty())
            return result;
        }
      }
      {
        /*
         * If the CPUID instruction fails/is unsupported on the target architecture,
         * we fallback to querying the registry. This is a lot more reliable than
         * querying the CPU itself and supports all architectures, but it's also slower.
         */

        HKEY hKey = nullptr;

        // This key contains information about the processor.
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
          // Get the processor name value from the registry key.
          Result<WString> processorNameW = GetRegistryValue(hKey, L"ProcessorNameString");

          // Ensure the key is closed to prevent leaks.
          RegCloseKey(hKey);

          // The registry returns wide strings so we have to convert to UTF-8 before returning.
          if (processorNameW)
            return ConvertWStringToUTF8(*processorNameW);
        }
      }

      // At this point, there's no other good method to get the CPU model on Windows.
      // Using WMI is useless because it just calls the same registry key we're already using.
      ERR(NotFound, "All methods to get CPU model failed on this platform");
    });
  }

  auto GetCPUCores(CacheManager& cache) -> Result<CPUCores> {
    return cache.getOrSet<CPUCores>("windows_cpu_cores", draconis::utils::cache::CachePolicy::neverExpire(), []() -> Result<CPUCores> {
      const DWORD logicalProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
      if (logicalProcessors == 0)
        ERR_FMT(ApiUnavailable, "GetActiveProcessorCount failed with error code {}", GetLastError());

      DWORD bufferSize = 0;

      if (GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bufferSize) == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        ERR_FMT(ApiUnavailable, "GetLogicalProcessorInformationEx (size query) failed with error code {}", GetLastError());

      for (int attempt = 0; attempt < 4; ++attempt) {
        if (bufferSize == 0)
          ERR(ParseError, "Empty processor topology buffer");
        Vec<BYTE> buffer(bufferSize);
        DWORD     capacity = static_cast<DWORD>(buffer.size());
        if (GetLogicalProcessorInformationEx(RelationProcessorCore, reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()), &capacity)) {
          if (capacity > buffer.size() || logicalProcessors > std::numeric_limits<u16>::max())
            ERR(ResourceExhausted, "Processor topology exceeds the public API range");
          const auto physicalCores = TRY(draconis::utils::windows::CountProcessorCores(Span<const BYTE>(buffer).first(capacity)));
          return CPUCores(physicalCores, static_cast<u16>(logicalProcessors));
        }
        const DWORD error = GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER)
          ERR_FMT(ApiUnavailable, "GetLogicalProcessorInformationEx failed: {}", error);
        bufferSize = capacity;
      }
      ERR(ApiUnavailable, "Processor topology kept changing during enumeration");
    });
  }

  auto GetGPUModel(CacheManager& cache) -> Result<String> {
    return cache.getOrSet<String>("windows_gpu_model", draconis::utils::cache::CachePolicy::neverExpire(), []() -> Result<String> {
      Microsoft::WRL::ComPtr<IDXGIFactory> factory;

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
      HRESULT result = CreateDXGIFactory(IID_PPV_ARGS(&factory));
  #pragma clang diagnostic pop

      if (FAILED(result))
        ERR_FMT(ApiUnavailable, "Failed to create DXGI factory: {}", result);

      Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
      result = factory->EnumAdapters(0, &adapter);

      if (result == DXGI_ERROR_NOT_FOUND)
        ERR(NotFound, "No GPU adapter found");

      if (FAILED(result))
        ERR(ApiUnavailable, "Failed to enumerate adapters");

      DXGI_ADAPTER_DESC desc {};
      result = adapter->GetDesc(&desc);

      if (FAILED(result))
        ERR(ApiUnavailable, "Failed to get adapter description");

      const i32 length = WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nullptr, 0, nullptr, nullptr);

      if (length == 0)
        ERR(InternalError, "Failed to convert GPU name to UTF-8");

      String gpuName(length, '\0');
      WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName.data(), length, nullptr, nullptr);
      gpuName.resize(length - 1);

      return gpuName;
    });
  }

  auto GetUptime() -> Result<std::chrono::seconds> {
    return std::chrono::seconds(GetTickCount64() / 1000);
  }

  auto GetOutputs(CacheManager& /*cache*/) -> Result<Vec<DisplayInfo>> {
    for (int attempt = 0; attempt < 4; ++attempt) {
      UINT32 pathCount = 0, modeCount = 0;
      LONG   status = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
      if (status != ERROR_SUCCESS)
        ERR_FMT(ApiUnavailable, "GetDisplayConfigBufferSizes failed: {}", status);
      Vec<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
      Vec<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
      status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(), nullptr);
      if (status == ERROR_INSUFFICIENT_BUFFER)
        continue;
      if (status != ERROR_SUCCESS)
        ERR_FMT(ApiUnavailable, "QueryDisplayConfig failed: {}", status);
      if (pathCount > paths.size() || modeCount > modes.size())
        ERR(ParseError, "Invalid display configuration size");
      paths.resize(pathCount);
      modes.resize(modeCount);
      Vec<DisplayInfo> outputs;
      outputs.reserve(pathCount);
      for (const auto& path : paths) {
        if (!(path.flags & DISPLAYCONFIG_PATH_ACTIVE))
          continue;
        if (path.targetInfo.modeInfoIdx >= modes.size() || path.sourceInfo.modeInfoIdx >= modes.size())
          ERR(ParseError, "Invalid display mode index");
        const auto& target = modes[path.targetInfo.modeInfoIdx];
        const auto& source = modes[path.sourceInfo.modeInfoIdx];
        if (target.infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET || source.infoType != DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE)
          ERR(ParseError, "Unexpected display mode type");
        const auto& signal = target.targetMode.targetVideoSignalInfo;
        const f64   pixels = static_cast<f64>(signal.totalSize.cx) * signal.totalSize.cy;
        outputs.emplace_back(path.targetInfo.id, DisplayInfo::Resolution { .width = signal.activeSize.cx, .height = signal.activeSize.cy }, pixels > 0 ? signal.pixelRate / pixels : 0, source.sourceMode.position.x == 0 && source.sourceMode.position.y == 0);
      }
      if (outputs.empty())
        ERR(NotFound, "No active displays found");
      return outputs;
    }
    ERR(ApiUnavailable, "Display configuration kept changing during enumeration");
  }

  auto GetPrimaryOutput(CacheManager& cache) -> Result<DisplayInfo> {
    const auto outputs = TRY(GetOutputs(cache));
    for (const auto& output : outputs)
      if (output.isPrimary)
        return output;
    ERR(NotFound, "No primary display found");
  }

  static auto CollectNetworkInterfaces(Option<NetworkInterface>* primary) -> Result<Vec<NetworkInterface>> {
    Option<DWORD> primaryIndex;
    if (primary) {
      MIB_IPFORWARDROW route {};
      sockaddr_in      destination {};
      destination.sin_family = AF_INET;
      inet_pton(AF_INET, "8.8.8.8", &destination.sin_addr);
      if (GetBestRoute(destination.sin_addr.s_addr, 0, &route) == NO_ERROR)
        primaryIndex = route.dwForwardIfIndex;
    }
    Vec<NetworkInterface> interfaces;
    ULONG                 bufferSize = 15000; // A reasonable starting buffer size
    Vec<BYTE>             buffer(bufferSize);

    // GetAdaptersAddresses is a two-call function. First call gets the required buffer size.
    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
    auto* pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    DWORD result     = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);

    if (result == ERROR_BUFFER_OVERFLOW) {
      buffer.resize(bufferSize);
      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
      pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
      result     = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
    }

    if (result != NO_ERROR)
      ERR_FMT(NetworkError, "GetAdaptersAddresses failed with error: {}", result);

    // Iterate through the linked list of adapters
    for (IP_ADAPTER_ADDRESSES* pCurrAddresses = pAddresses; pCurrAddresses != nullptr; pCurrAddresses = pCurrAddresses->Next) {
      NetworkInterface iface;
      iface.name = pCurrAddresses->AdapterName;

      iface.isUp       = (pCurrAddresses->OperStatus == IfOperStatusUp);
      iface.isLoopback = (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK);

      // Format the MAC address
      if (pCurrAddresses->PhysicalAddressLength == 6)
        iface.macAddress = std::format(
          "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
          pCurrAddresses->PhysicalAddress[0],
          pCurrAddresses->PhysicalAddress[1],
          pCurrAddresses->PhysicalAddress[2],
          pCurrAddresses->PhysicalAddress[3],
          pCurrAddresses->PhysicalAddress[4],
          pCurrAddresses->PhysicalAddress[5]
        );

      // Iterate through the IP addresses for this adapter
      for (const IP_ADAPTER_UNICAST_ADDRESS* pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next)
        if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
          // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
          const auto* saIn = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);

          Array<char, INET_ADDRSTRLEN> strBuffer {};

          if (inet_ntop(AF_INET, &(saIn->sin_addr), strBuffer.data(), INET_ADDRSTRLEN))
            iface.ipv4Address = strBuffer.data();
        }

      if (primary && primaryIndex && pCurrAddresses->IfIndex == *primaryIndex)
        *primary = iface;
      interfaces.emplace_back(std::move(iface));
    }

    return interfaces;
  }

  auto GetNetworkInterfaces(CacheManager& /*cache*/) -> Result<Vec<NetworkInterface>> {
    return CollectNetworkInterfaces(nullptr);
  }

  auto GetNetworkSnapshot(CacheManager& /*cache*/) -> Result<NetworkSnapshot> {
    NetworkSnapshot snapshot;
    auto            interfaces = CollectNetworkInterfaces(&snapshot.primaryInterface);
    if (!interfaces)
      return Err(interfaces.error());
    snapshot.interfaces = std::move(*interfaces);
    return snapshot;
  }

  auto GetPrimaryNetworkInterface(CacheManager& cache) -> Result<NetworkInterface> {
    return cache.getOrSet<NetworkInterface>("windows_primary_network_interface", []() -> Result<NetworkInterface> {
      MIB_IPFORWARDROW routeRow;
      sockaddr_in      destAddr {};
      destAddr.sin_family = AF_INET;
      inet_pton(AF_INET, "8.8.8.8", &destAddr.sin_addr);

      if (DWORD status = GetBestRoute(destAddr.sin_addr.s_addr, 0, &routeRow); status != NO_ERROR)
        ERR_FMT(NetworkError, "GetBestRoute failed with error: {}", status);

      // The interface index for the best route
      const DWORD primaryInterfaceIndex = routeRow.dwForwardIfIndex;

      ULONG     bufferSize = 15000;
      Vec<BYTE> buffer(bufferSize);

      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
      auto* pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

      DWORD result = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
      if (result == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufferSize);
        // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
        pAddresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        result     = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufferSize);
      }

      if (result != NO_ERROR)
        ERR_FMT(NetworkError, "GetAdaptersAddresses failed with error: {}", result);

      for (IP_ADAPTER_ADDRESSES* pCurrAddresses = pAddresses; pCurrAddresses != nullptr; pCurrAddresses = pCurrAddresses->Next) {
        // NOLINTNEXTLINE(*-union-access)
        if (pCurrAddresses->IfIndex == primaryInterfaceIndex) {
          NetworkInterface iface;
          iface.name = pCurrAddresses->AdapterName;

          iface.isUp       = (pCurrAddresses->OperStatus == IfOperStatusUp);
          iface.isLoopback = (pCurrAddresses->IfType == IF_TYPE_SOFTWARE_LOOPBACK);

          if (pCurrAddresses->PhysicalAddressLength == 6)
            iface.macAddress = std::format(
              "{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
              pCurrAddresses->PhysicalAddress[0],
              pCurrAddresses->PhysicalAddress[1],
              pCurrAddresses->PhysicalAddress[2],
              pCurrAddresses->PhysicalAddress[3],
              pCurrAddresses->PhysicalAddress[4],
              pCurrAddresses->PhysicalAddress[5]
            );

          for (IP_ADAPTER_UNICAST_ADDRESS const* pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != nullptr; pUnicast = pUnicast->Next)
            if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
              // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
              auto* saIn = reinterpret_cast<sockaddr_in*>(pUnicast->Address.lpSockaddr);

              Array<char, INET_ADDRSTRLEN> strBuffer {};

              if (inet_ntop(AF_INET, &(saIn->sin_addr), strBuffer.data(), INET_ADDRSTRLEN))
                iface.ipv4Address = strBuffer.data();
            }

          return iface;
        }
      }

      ERR(NotFound, "Could not find details for the primary network interface.");
    });
  }

  auto GetBatteryInfo(CacheManager& /*cache*/) -> Result<Battery> {
    using matchit::match, matchit::is, matchit::_;
    using enum Battery::Status;

    SYSTEM_POWER_STATUS powerStatus;

    if (!GetSystemPowerStatus(&powerStatus))
      ERR_FMT(ApiUnavailable, "GetSystemPowerStatus failed with error code {}", GetLastError());

    // The BATTERY_FLAG_NO_SYSTEM_BATTERY flag (0x80) indicates no battery is present.
    if (powerStatus.BatteryFlag & BATTERY_FLAG_NO_BATTERY)
      ERR(NotFound, "No battery found");

    // The BATTERY_FLAG_UNKNOWN flag (0xFF) indicates the status can't be determined.
    if (powerStatus.BatteryFlag == BATTERY_FLAG_UNKNOWN)
      ERR(NotFound, "Battery status unknown");

    // 255 means unknown, so we'll map it to None.
    const Option<u8> percentage = (powerStatus.BatteryLifePercent == 255) ? None : Some(powerStatus.BatteryLifePercent);

    // The catch-all should only ever need to cover 255 but using it regardless is safer.
    const Battery::Status status = match(powerStatus.ACLineStatus)(
      is | (_ == 1 && percentage == 100) = Full,
      is | 1                             = Charging,
      is | 0                             = Discharging,
      is | _                             = Unknown
    );

    return Battery(
      status,
      percentage,
      powerStatus.BatteryLifeTime == std::numeric_limits<DWORD>::max()
        ? None
        : Some(std::chrono::seconds(powerStatus.BatteryLifeTime))
    );
  }
} // namespace draconis::core::system

  #if DRAC_ENABLE_PACKAGECOUNT
namespace draconis::services::packages {
  using draconis::utils::env::GetEnv;
  using helpers::GetDirCount;

  auto CountChocolatey(CacheManager& cache) -> Result<u64> {
    return cache.getOrSet<u64>("windows_chocolatey_count", []() -> Result<u64> {
      // C:\ProgramData\chocolatey is the default installation directory.
      WString chocoPath = L"C:\\ProgramData\\chocolatey";

      // If the ChocolateyInstall environment variable is set, use that instead.
      // Most of the time it's set to C:\ProgramData\chocolatey, but it can be overridden.
      if (const Result<WString> chocoEnv = GetEnv(L"ChocolateyInstall"); chocoEnv)
        chocoPath = *chocoEnv;

      // The lib directory contains the package metadata.
      chocoPath.append(L"\\lib");

      // Get the number of directories in the lib directory.
      // This corresponds to the number of packages installed.
      return GetDirCount(chocoPath);
    });
  }

  auto CountScoop(CacheManager& cache) -> Result<u64> {
    return cache.getOrSet<u64>("windows_scoop_count", []() -> Result<u64> {
      WString scoopAppsPath;

      // The SCOOP environment variable should be used first if it's set.
      if (const Result<WString> scoopEnv = GetEnv(L"SCOOP"); scoopEnv) {
        scoopAppsPath = *scoopEnv;
        scoopAppsPath.append(L"\\apps");
      } else if (const Result<WString> userProfile = GetEnv(L"USERPROFILE"); userProfile) {
        // Otherwise, we can try finding the scoop folder in the user's home directory.
        scoopAppsPath = *userProfile;
        scoopAppsPath.append(L"\\scoop\\apps");
      } else {
        // The user likely doesn't have scoop installed if neither of those other methods work.
        ERR(ConfigurationError, "Could not determine Scoop installation directory (SCOOP and USERPROFILE environment variables not found)");
      }

      // Get the number of directories in the apps directory.
      // This corresponds to the number of packages installed.
      return GetDirCount(scoopAppsPath);
    });
  }

  auto CountWinGet(CacheManager& /*cache*/) -> Result<u64> {
    ERR(NotSupported, "WinGet inventory is not exposed by the current native package backend");
  }

} // namespace draconis::services::packages
  #endif // DRAC_ENABLE_PACKAGECOUNT

#endif // _WIN32
