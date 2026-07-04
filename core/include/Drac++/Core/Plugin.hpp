/**
 * @file Plugin.hpp
 * @brief Core plugin system interfaces for Draconis++
 * @author Draconis++ Team
 * @version 2.0.0
 *
 * @details This plugin system is designed for maximum performance:
 * - Zero-cost abstractions when plugins are disabled
 * - Lazy loading with efficient caching
 * - Minimal memory allocations
 * - RAII-based resource management
 * - Lock-free plugin access after initialization
 *
 * Plugin Types:
 * - InfoProvider: Provides structured data (weather, media, docker stats, etc.)
 * - OutputFormat: Formats output (JSON, YAML, Markdown, etc.)
 */

#pragma once

#include <chrono>
#include <concepts>
#include <filesystem>
#include <fstream>
#include <format>
#include <glaze/glaze.hpp>

// Required for DRAC_PLUGIN macro which uses draconis::utils::logging::LogLevel and SetLogLevelPtr
#include "../Utils/Logging.hpp" // IWYU pragma: keep
#include "../Utils/Types.hpp"

// Forward declaration to avoid including CacheManager.hpp
namespace draconis::utils::cache {
  class CacheManager;
}

/**
 * @class PluginCache
 * @brief Cache interface for plugins - provides efficient BEVE-based caching for any serializable type
 *
 * @details This class wraps a cache directory and provides template-based caching that stores
 * data in BEVE format (glaze's binary encoding) for maximum efficiency. Plugins can cache
 * any type that has glaze metadata defined.
 *
 * Cache entries include an expiry timestamp, and expired entries are automatically ignored.
 */
class PluginCache {
  using String = draconis::utils::types::String;
  template <typename T>
  using Option = draconis::utils::types::Option<T>;
  using u32    = draconis::utils::types::u32;
  using u64    = draconis::utils::types::u64;
  template <typename K, typename V>
  using UnorderedMap = draconis::utils::types::UnorderedMap<K, V>;
  template <typename A, typename B>
  using Pair                 = draconis::utils::types::Pair<A, B>;
  static constexpr auto None = draconis::utils::types::None;

 public:
  template <typename T>
  struct CacheEntry {
    T           data;
    Option<u64> expires; // UNIX timestamp, None = no expiry
  };

  explicit PluginCache(const std::filesystem::path& cacheDir) : m_cacheDir(cacheDir) {
    std::error_code errc;
    std::filesystem::create_directories(m_cacheDir, errc);
  }

  /**
   * @brief Get a cached value
   * @tparam T The type to retrieve (must have glaze metadata)
   * @param key Cache key
   * @return The cached value if found and not expired, None otherwise
   */
  template <typename T>
  [[nodiscard]] auto get(const String& key) const -> Option<T> {
    // Check in-memory cache first
    if (auto iter = m_cache.find(key); iter != m_cache.end()) {
      const auto& [data, expiryTp] = iter->second;
      if (std::chrono::system_clock::now() < expiryTp) {
        CacheEntry<T> entry;
        if (glz::read_beve(entry, data) == glz::error_code::none)
          return entry.data;
      }
    }

    // Check filesystem
    const std::filesystem::path filePath = m_cacheDir / key;
    if (!std::filesystem::exists(filePath))
      return None;

    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs)
      return None;

    String        fileContents((std::istreambuf_iterator<char>(ifs)), {});
    CacheEntry<T> entry;

    if (glz::read_beve(entry, fileContents) != glz::error_code::none)
      return None;

    // Check expiry
    if (entry.expires.has_value()) {
      auto expiryTp = std::chrono::system_clock::time_point(std::chrono::seconds(*entry.expires));
      if (std::chrono::system_clock::now() >= expiryTp)
        return None;
    }

    // Store in memory cache for faster subsequent access
    auto expiryTp = entry.expires.has_value()
      ? std::chrono::system_clock::time_point(std::chrono::seconds(*entry.expires))
      : std::chrono::system_clock::time_point::max();
    m_cache[key]  = { fileContents, expiryTp };

    return entry.data;
  }

  /**
   * @brief Set a cached value
   * @tparam T The type to store (must have glaze metadata)
   * @param key Cache key
   * @param value The value to cache
   * @param ttlSeconds Time-to-live in seconds (0 = no expiry)
   */
  template <typename T>
  auto set(const String& key, const T& value, u32 ttlSeconds = 0) -> void {
    using namespace std::chrono;

    Option<u64>              expiryTs = None;
    system_clock::time_point expiryTp = system_clock::time_point::max();

    if (ttlSeconds > 0) {
      expiryTp = system_clock::now() + seconds(ttlSeconds);
      expiryTs = duration_cast<seconds>(expiryTp.time_since_epoch()).count();
    }

    CacheEntry<T> entry {
      .data    = value,
      .expires = expiryTs
    };

    String binaryBuffer;
    glz::write_beve(entry, binaryBuffer);

    // Store in memory
    m_cache[key] = { binaryBuffer, expiryTp };

    // Store to filesystem
    const std::filesystem::path filePath = m_cacheDir / key;
    std::error_code             errc;
    std::filesystem::create_directories(filePath.parent_path(), errc);

    if (std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc); ofs.is_open())
      ofs.write(binaryBuffer.data(), static_cast<std::streamsize>(binaryBuffer.size()));
  }

  /**
   * @brief Invalidate a cached entry
   * @param key Cache key to invalidate
   */
  auto invalidate(const String& key) -> void {
    m_cache.erase(key);
    std::error_code errc;
    std::filesystem::remove(m_cacheDir / key, errc);
  }

 private:
  std::filesystem::path                                                             m_cacheDir;
  mutable UnorderedMap<String, Pair<String, std::chrono::system_clock::time_point>> m_cache;
};

// Glaze metadata for CacheEntry (in global glz namespace)
namespace glz {
  template <typename T>
  struct meta<PluginCache::CacheEntry<T>> {
    using Entry                 = PluginCache::CacheEntry<T>;
    static constexpr auto value = object("data", &Entry::data, "expires", &Entry::expires);
  };
} // namespace glz

namespace draconis::core::plugin {
  /**
   * @enum PluginType
   * @brief Categorizes plugins for efficient lookup and filtering
   */
  enum class PluginType : utils::types::u8 {
    InfoProvider, ///< Provides structured information (weather, media, etc.)
    OutputFormat, ///< Adds new output formats (JSON, YAML, Markdown, etc.)
  };

  struct PluginDependencies {
    bool requiresNetwork    = false;
    bool requiresFilesystem = false;
    bool requiresAdmin      = false;
    bool requiresCaching    = false;
  };

  struct PluginMetadata {
    utils::types::String name;
    utils::types::String version;
    utils::types::String author;
    utils::types::String description;
    PluginType           type;
    PluginDependencies   dependencies;
  };

  struct PluginFieldValue;

  using PluginFieldArray = utils::types::Vec<PluginFieldValue>;

  using PluginFieldValueBase = utils::types::Variant<
    bool,
    utils::types::i64,
    utils::types::u64,
    utils::types::f64,
    utils::types::String,
    PluginFieldArray>;

  struct PluginFieldValue : PluginFieldValueBase {
    using PluginFieldValueBase::PluginFieldValueBase;

    PluginFieldValue(const char* value) : PluginFieldValueBase(utils::types::String { value }) {}
    PluginFieldValue(utils::types::StringView value) : PluginFieldValueBase(utils::types::String { value }) {}
    PluginFieldValue(const utils::types::Vec<utils::types::String>& values) : PluginFieldValueBase(ToArray(values)) {}
    PluginFieldValue(utils::types::Vec<utils::types::String>&& values) : PluginFieldValueBase(ToArray(std::move(values))) {}

   private:
    [[nodiscard]] static auto ToArray(const utils::types::Vec<utils::types::String>& values) -> PluginFieldArray {
      PluginFieldArray result;
      result.reserve(values.size());
      for (const auto& value : values)
        result.emplace_back(value);
      return result;
    }

    [[nodiscard]] static auto ToArray(utils::types::Vec<utils::types::String>&& values) -> PluginFieldArray {
      PluginFieldArray result;
      result.reserve(values.size());
      for (auto&& value : values)
        result.emplace_back(std::move(value));
      return result;
    }
  };

  using PluginFields = utils::types::Map<utils::types::String, PluginFieldValue>;
  using PluginData   = utils::types::Map<utils::types::String, PluginFields>;

  [[nodiscard]] inline auto PluginFieldToString(const PluginFieldValue& value) -> utils::types::String {
    using namespace utils::types;

    return std::visit(
      [](const auto& inner) -> String {
        using T = std::decay_t<decltype(inner)>;

        if constexpr (std::same_as<T, bool>)
          return inner ? "true" : "false";
        else if constexpr (std::same_as<T, i64> || std::same_as<T, u64>)
          return std::to_string(inner);
        else if constexpr (std::same_as<T, f64>)
          return std::format("{}", inner);
        else if constexpr (std::same_as<T, String>)
          return inner;
        else {
          String result;
          usize  size = 0;
          for (const PluginFieldValue& item : inner)
            size += PluginFieldToString(item).size() + 2;
          result.reserve(size);

          for (usize i = 0; i < inner.size(); ++i) {
            if (i > 0)
              result += ", ";
            result += PluginFieldToString(inner[i]);
          }

          return result;
        }
      },
      static_cast<const PluginFieldValueBase&>(value)
    );
  }

  inline auto AppendJsonEscaped(utils::types::String& output, utils::types::StringView input) -> void {
    for (const char ch : input) {
      switch (ch) {
        case '"':
          output += "\\\"";
          break;
        case '\\':
          output += "\\\\";
          break;
        case '\n':
          output += "\\n";
          break;
        case '\r':
          output += "\\r";
          break;
        case '\t':
          output += "\\t";
          break;
        default:
          output += ch;
          break;
      }
    }
  }

  inline auto AppendPluginFieldJson(utils::types::String& output, const PluginFieldValue& value) -> void {
    using namespace utils::types;

    std::visit(
      [&output](const auto& inner) -> void {
        using T = std::decay_t<decltype(inner)>;

        if constexpr (std::same_as<T, bool>) {
          output += inner ? "true" : "false";
        } else if constexpr (std::same_as<T, i64> || std::same_as<T, u64>) {
          output += std::to_string(inner);
        } else if constexpr (std::same_as<T, f64>) {
          output += std::format("{}", inner);
        } else if constexpr (std::same_as<T, String>) {
          output += '"';
          AppendJsonEscaped(output, inner);
          output += '"';
        } else {
          output += '[';
          for (usize i = 0; i < inner.size(); ++i) {
            if (i > 0)
              output += ',';
            AppendPluginFieldJson(output, inner[i]);
          }
          output += ']';
        }
      },
      static_cast<const PluginFieldValueBase&>(value)
    );
  }

  [[nodiscard]] inline auto PluginDataToJson(const PluginData& pluginData) -> utils::types::String {
    using namespace utils::types;

    String output;
    output += '{';

    usize pluginIndex = 0;
    for (const auto& [pluginId, fields] : pluginData) {
      if (pluginIndex++ > 0)
        output += ',';

      output += '"';
      AppendJsonEscaped(output, pluginId);
      output += "\":{";

      usize fieldIndex = 0;
      for (const auto& [fieldName, value] : fields) {
        if (fieldIndex++ > 0)
          output += ',';

        output += '"';
        AppendJsonEscaped(output, fieldName);
        output += "\":";
        AppendPluginFieldJson(output, value);
      }

      output += '}';
    }

    output += '}';
    return output;
  }

  inline auto AppendPluginDataJsonProperty(utils::types::String& jsonObject, const PluginData& pluginData) -> void {
    if (pluginData.empty())
      return;

    const auto closingBrace = jsonObject.find_last_of('}');
    if (closingBrace == utils::types::String::npos)
      return;

    const bool hasExistingProperties = jsonObject.find_first_not_of(" \n\r\t{") < closingBrace;
    jsonObject.insert(closingBrace, std::format("{}\"pluginFields\":{}", hasExistingProperties ? "," : "", PluginDataToJson(pluginData)));
  }

  /**
   * @struct PluginContext
   * @brief Context passed to plugins during initialization
   * @details Contains paths and configuration needed by plugins
   */
  struct PluginContext {
    std::filesystem::path configDir; ///< Directory where plugin configs live (e.g., ~/.config/draconis++/plugins/)
    std::filesystem::path cacheDir;  ///< Directory for plugin cache files
    std::filesystem::path dataDir;   ///< Directory for plugin data files
  };

  class IPlugin {
   public:
    IPlugin()                                                               = default;
    IPlugin(const IPlugin&)                                                 = default;
    IPlugin(IPlugin&&)                                                      = delete;
    auto operator=(const IPlugin&) -> IPlugin&                              = default;
    auto operator=(IPlugin&&) -> IPlugin&                                   = delete;
    virtual ~IPlugin()                                                      = default;
    [[nodiscard]] virtual auto getMetadata() const -> const PluginMetadata& = 0;

    /**
     * @brief Initialize the plugin with context and cache
     * @param ctx Plugin context with paths for config, cache, and data
     * @param cache Cache interface for persistent storage
     * @return Success or error
     */
    virtual auto initialize(const PluginContext& ctx, ::PluginCache& cache) -> utils::types::Result<utils::types::Unit> = 0;

    /**
     * @brief Set plugin configuration from TOML string
     * @param tomlConfig TOML configuration string
     * @return Success or error
     * @details Allows runtime configuration without filesystem access.
     *          Plugins should parse this during initialize() or before collectData().
     */
    virtual auto setConfig(utils::types::StringView tomlConfig) -> utils::types::Result<utils::types::Unit> {
      (void)tomlConfig;
      return {};
    }

    virtual auto shutdown() -> utils::types::Unit = 0;

    [[nodiscard]] virtual auto isReady() const -> bool = 0;
  };

  /**
   * @class IInfoProviderPlugin
   * @brief Plugin interface for providing structured information
   *
   * @details Info providers supply structured fields that can be:
   * - Consumed by output format plugins
   * - Used in compact format templates
   * - Displayed in the UI
   *
   * Each plugin manages its own configuration file in the plugins config directory.
   * The plugin is responsible for reading its config during initialize().
   *
   * Example providers: weather, now playing, docker stats, system monitors
   */
  class IInfoProviderPlugin : public IPlugin {
   public:
    /**
     * @brief Unique identifier for this provider
     * @return Provider ID (e.g., "weather", "media", "docker")
     * @details Used as the key in output data and for compact format placeholders
     */
    [[nodiscard]] virtual auto getProviderId() const -> utils::types::String = 0;

    /**
     * @brief Collect/refresh data from this provider
     * @param cache Cache interface for data persistence
     * @return Success or error (errors are passed up for doctor mode)
     * @details Called each time the app runs. Plugin should use cache for efficiency.
     */
    virtual auto collectData(::PluginCache& cache) -> utils::types::Result<utils::types::Unit> = 0;

    /**
     * @brief Get data as typed key-value pairs
     * @return Map of field names to typed values
     * @details Used by output format plugins and compact format templates.
     *          Keys should be local field names (e.g., "temperature", "description");
     *          Draconis++ nests them under the provider ID.
     */
    [[nodiscard]] virtual auto getFields() const -> PluginFields = 0;

    /**
     * @brief Get a single-line display string for UI
     * @return Formatted string for display (e.g., "72°F, Clear sky")
     * @details Used in the main UI output
     */
    [[nodiscard]] virtual auto getDisplayValue() const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get the icon for UI display (Nerd Font icon with spacing)
     * @return Icon string (e.g., "   " for weather)
     * @details Used in the main UI output. Should include trailing space for alignment.
     */
    [[nodiscard]] virtual auto getDisplayIcon() const -> utils::types::String = 0;

    /**
     * @brief Get the label for UI display
     * @return Label string (e.g., "Weather")
     * @details Used in the main UI output
     */
    [[nodiscard]] virtual auto getDisplayLabel() const -> utils::types::String = 0;

    /**
     * @brief Get the last error from data collection, if any
     * @return Error if collectData() failed, None otherwise
     * @details Used for doctor mode to report failures
     */
    [[nodiscard]] virtual auto getLastError() const -> utils::types::Option<utils::types::String> = 0;

    /**
     * @brief Check if this provider is enabled in its config
     * @return True if enabled, false if disabled
     * @details Plugins can be installed but disabled in config
     */
    [[nodiscard]] virtual auto isEnabled() const -> bool = 0;
  };

  /**
   * @class IOutputFormatPlugin
   * @brief Plugin interface for output formatting
   */
  class IOutputFormatPlugin : public IPlugin {
   public:
    /**
     * @brief Format the data using the specified format variant
     * @param formatName The format name to use (must be one returned by getFormatNames())
     * @param data The core system data to format
     * @param pluginData Plugin-contributed data organized by plugin ID
     * @return Formatted output string or error
     */
    virtual auto formatOutput(
      const utils::types::String&                                                                                   formatName,
      const utils::types::Map<utils::types::String, utils::types::String>&                                          data,
      const PluginData&                                                                                             pluginData
    ) const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get all format names this plugin supports
     * @return Vector of supported format names (e.g., {"json", "json-pretty"})
     */
    [[nodiscard]] virtual auto getFormatNames() const -> utils::types::Vec<utils::types::String> = 0;

    /**
     * @brief Get file extension for a given format
     * @param formatName The format name
     * @return File extension (without dot)
     */
    [[nodiscard]] virtual auto getFileExtension(const utils::types::String& formatName) const -> utils::types::String = 0;
  };

  // Legacy alias for backward compatibility during migration
  using ISystemInfoPlugin = IInfoProviderPlugin;
} // namespace draconis::core::plugin

#if defined(DRAC_STATIC_PLUGIN_BUILD)
  // For static plugin builds, no import/export needed
  #define DRAC_PLUGIN_API
#elif defined(_WIN32)
  #if defined(DRAC_PLUGIN_BUILD)
    #define DRAC_PLUGIN_API __declspec(dllexport)
  #else
    #define DRAC_PLUGIN_API __declspec(dllimport)
  #endif
#else
  #define DRAC_PLUGIN_API __attribute__((visibility("default")))
#endif

/**
 * @def DRAC_PLUGIN
 * @brief Generates plugin factory functions with default create/destroy behavior
 *
 * @param PluginClass The plugin class to instantiate (must be default-constructible)
 *
 * For static builds, creates factory functions and self-registers the plugin at startup.
 * For dynamic builds, creates extern "C" exports for dynamic loading.
 *
 * @example
 * @code
 * DRAC_PLUGIN(WindowsInfoPlugin)
 * @endcode
 */
#ifdef DRAC_STATIC_PLUGIN_BUILD
  #include <Drac++/Core/StaticPlugins.hpp>

  // For static builds, each plugin exports a Register function
  // that is called by DracInitStaticPlugins()
  #define DRAC_PLUGIN(PluginClass)                                                       \
    namespace draconis::plugins {                                                        \
      static auto Create_##PluginClass() -> ::draconis::core::plugin::IPlugin* {         \
        return new PluginClass();                                                        \
      }                                                                                  \
      static auto Destroy_##PluginClass(::draconis::core::plugin::IPlugin* p) -> void {  \
        delete p;                                                                        \
      }                                                                                  \
    }                                                                                    \
    extern "C" DRAC_PLUGIN_API void DracRegisterPlugin_##PluginClass() {                 \
      ::draconis::core::plugin::RegisterStaticPlugin(                                    \
        #PluginClass,                                                                    \
        { ::draconis::plugins::Create_##PluginClass,                                     \
          ::draconis::plugins::Destroy_##PluginClass }                                   \
      );                                                                                 \
    }

#else
// NOLINTBEGIN(bugprone-macro-parentheses) - false positive
  #define DRAC_PLUGIN(PluginClass)                                                                            \
    extern "C" DRAC_PLUGIN_API auto CreatePlugin() -> draconis::core::plugin::IPlugin* {                      \
      return new PluginClass();                                                                               \
    }                                                                                                         \
    extern "C" DRAC_PLUGIN_API auto DestroyPlugin(draconis::core::plugin::IPlugin* plugin) -> void {          \
      delete plugin;                                                                                          \
    }                                                                                                         \
    extern "C" DRAC_PLUGIN_API auto SetPluginLogLevel(draconis::utils::logging::LogLevel* levelPtr) -> void { \
      draconis::utils::logging::SetLogLevelPtr(levelPtr);                                                     \
    }
// NOLINTEND(bugprone-macro-parentheses)
#endif
