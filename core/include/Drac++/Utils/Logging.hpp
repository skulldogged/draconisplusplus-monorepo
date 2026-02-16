#pragma once

#include <chrono>     // std::chrono::{days, floor, seconds, system_clock}
#include <ctime>      // localtime_r/s, strftime, time_t, tm
#include <filesystem> // std::filesystem::path
#include <format>     // std::format
#include <stack>      // std::stack for span tracking
#include <utility>    // std::forward

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <fcntl.h>
  #include <io.h>
  #include <windows.h>
#endif

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout, std::cerr
#endif

#include <source_location> // std::source_location (always needed for spans/targets)

#include "Error.hpp"
#include "Types.hpp"

namespace draconis::utils::logging {
  namespace types = ::draconis::utils::types;

  inline auto GetLogMutex() -> types::Mutex& {
    static types::Mutex LogMutexInstance;
    return LogMutexInstance;
  }

  /**
   * @brief Helper to write to console handling Windows specifics
   * @param text The text to write
   * @param useStderr Whether to write to stderr instead of stdout
   */
  inline auto WriteToConsole(const types::StringView text, bool useStderr = false) -> void {
#ifdef _WIN32
    HANDLE hOutput = GetStdHandle(useStderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    if (hOutput != INVALID_HANDLE_VALUE) {
      DWORD consoleMode = 0;
      if (GetConsoleMode(hOutput, &consoleMode)) {
        WriteConsoleA(hOutput, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
      } else {
        WriteFile(hOutput, text.data(), static_cast<DWORD>(text.size()), nullptr, nullptr);
      }
      return;
    }
#endif

#ifdef __cpp_lib_print
    if (useStderr)
      std::print(stderr, "{}", text);
    else
      std::print("{}", text);
#else
    if (useStderr)
      std::cerr << text;
    else
      std::cout << text;
#endif
  }

  enum class LogColor : types::u8 {
    Black         = 0,
    Red           = 1,
    Green         = 2,
    Yellow        = 3,
    Blue          = 4,
    Magenta       = 5,
    Cyan          = 6,
    White         = 7,
    Gray          = 8,
    BrightRed     = 9,
    BrightGreen   = 10,
    BrightYellow  = 11,
    BrightBlue    = 12,
    BrightMagenta = 13,
    BrightCyan    = 14,
    BrightWhite   = 15,
  };

  struct LogLevelConst {
    // clang-format off
    static constexpr types::Array<types::StringView, 16> COLOR_CODE_LITERALS = {
      "\033[38;5;0m",  "\033[38;5;1m",  "\033[38;5;2m",  "\033[38;5;3m",
      "\033[38;5;4m",  "\033[38;5;5m",  "\033[38;5;6m",  "\033[38;5;7m",
      "\033[38;5;8m",  "\033[38;5;9m",  "\033[38;5;10m", "\033[38;5;11m",
      "\033[38;5;12m", "\033[38;5;13m", "\033[38;5;14m", "\033[38;5;15m",
    };
    // clang-format on

    static constexpr const char* RESET_CODE   = "\033[0m";
    static constexpr const char* BOLD_START   = "\033[1m";
    static constexpr const char* BOLD_END     = "\033[22m";
    static constexpr const char* ITALIC_START = "\033[3m";
    static constexpr const char* ITALIC_END   = "\033[23m";
    static constexpr const char* DIM_START    = "\033[2m";
    static constexpr const char* DIM_END      = "\033[22m";

    // Pre-formatted level strings with ANSI codes (bold + color + text + reset)
    // Format: BOLD_START + COLOR + TEXT + RESET_CODE
    // Tracing-style colors: TRACE=magenta, DEBUG=blue, INFO=green, WARN=yellow, ERROR=red
    static constexpr types::StringView TRACE_STYLED = "\033[1m\033[38;5;5mTRACE\033[0m"; // Magenta
    static constexpr types::StringView DEBUG_STYLED = "\033[1m\033[38;5;4mDEBUG\033[0m"; // Blue
    static constexpr types::StringView INFO_STYLED  = "\033[1m\033[38;5;2mINFO \033[0m"; // Green
    static constexpr types::StringView WARN_STYLED  = "\033[1m\033[38;5;3mWARN \033[0m"; // Yellow
    static constexpr types::StringView ERROR_STYLED = "\033[1m\033[38;5;1mERROR\033[0m"; // Red

    static constexpr types::PCStr TIMESTAMP_FORMAT = "%Y-%m-%dT%H:%M:%S";

    // Compact format: [timestamp] LEVEL target: message
    static constexpr types::PCStr LOG_FORMAT_COMPACT = "{} {} {}: {}";
    // Pretty format uses multi-line output
    static constexpr types::PCStr LOG_FORMAT_PRETTY_MAIN = "  {} {} {}: {}";

    static constexpr types::PCStr AT_PREFIX = "    at ";
    static constexpr types::PCStr IN_PREFIX = "    in ";
  };

  /**
   * @enum LogLevel
   * @brief Represents different log levels (tracing-style).
   */
  enum class LogLevel : types::u8 {
    Trace, // Most verbose - for tracing program flow
    Debug, // Debug information
    Info,  // General information
    Warn,  // Warnings
    Error, // Errors
  };

  /**
   * @brief Gets a reference to the shared log level pointer storage.
   */
  inline auto GetLogLevelPtrStorage() -> LogLevel*& {
    static LogLevel* Ptr = nullptr;
    return Ptr;
  }

  /**
   * @brief Gets a reference to the local log level storage.
   */
  inline auto GetLocalLogLevel() -> LogLevel& {
    static LogLevel Level = LogLevel::Info;
    return Level;
  }

  /**
   * @brief Sets the log level pointer for plugin support.
   */
  inline auto SetLogLevelPtr(LogLevel* ptr) -> void {
    GetLogLevelPtrStorage() = ptr;
  }

  /**
   * @brief Gets a pointer to the log level storage owned by this module.
   */
  inline auto GetLogLevelPtr() -> LogLevel* {
    return &GetLocalLogLevel();
  }

  /**
   * @brief Gets the current runtime log level.
   */
  inline auto GetRuntimeLogLevel() -> LogLevel& {
    if (LogLevel* ptr = GetLogLevelPtrStorage())
      return *ptr;
    return GetLocalLogLevel();
  }

  /**
   * @brief Sets the runtime log level.
   */
  inline auto SetRuntimeLogLevel(const LogLevel level) {
    if (LogLevel* ptr = GetLogLevelPtrStorage())
      *ptr = level;
    else
      GetLocalLogLevel() = level;
  }

  /**
   * @struct Style
   * @brief Options for text styling with ANSI codes.
   */
  struct Style {
    LogColor color  = LogColor::White;
    bool     bold   = false;
    bool     italic = false;
    bool     dim    = false;
  };

  /**
   * @brief Applies ANSI styling to text based on the provided style options.
   */
  inline auto Stylize(const types::StringView text, const Style& style) -> types::String {
    const bool hasStyle = style.bold || style.italic || style.dim || style.color != LogColor::White;

    if (!hasStyle)
      return types::String(text);

    types::String result;
    result.reserve(text.size() + 32);

    if (style.bold)
      result += LogLevelConst::BOLD_START;
    if (style.italic)
      result += LogLevelConst::ITALIC_START;
    if (style.dim)
      result += LogLevelConst::DIM_START;
    if (style.color != LogColor::White)
      result += LogLevelConst::COLOR_CODE_LITERALS.at(static_cast<types::usize>(style.color));

    result += text;
    result += LogLevelConst::RESET_CODE;

    return result;
  }

  /**
   * @brief Returns the pre-formatted and styled log level strings.
   */
  constexpr auto GetLevelInfo() -> const types::Array<types::StringView, 5>& {
    static constexpr types::Array<types::StringView, 5> LEVEL_INFO_INSTANCE = {
      LogLevelConst::TRACE_STYLED,
      LogLevelConst::DEBUG_STYLED,
      LogLevelConst::INFO_STYLED,
      LogLevelConst::WARN_STYLED,
      LogLevelConst::ERROR_STYLED,
    };
    return LEVEL_INFO_INSTANCE;
  }

  /**
   * @brief Returns whether a log level should use stderr
   */
  constexpr auto ShouldUseStderr(const LogLevel level) -> bool {
    return level == LogLevel::Warn || level == LogLevel::Error;
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Print Helpers
  // ─────────────────────────────────────────────────────────────────────────────

  template <typename... Args>
  inline auto Print(const LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...), ShouldUseStderr(level));
  }

  inline auto Print(const LogLevel level, const types::StringView text) {
    WriteToConsole(text, ShouldUseStderr(level));
  }

  template <typename... Args>
  inline auto Println(const LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...) + '\n', ShouldUseStderr(level));
  }

  inline auto Println(const LogLevel level, const types::StringView text) {
    types::String textWithNewline(text);
    textWithNewline += '\n';
    WriteToConsole(textWithNewline, ShouldUseStderr(level));
  }

  inline auto Println(const LogLevel level) {
    WriteToConsole("\n", ShouldUseStderr(level));
  }

  // User-facing print (stdout only)
  template <typename... Args>
  inline auto Print(std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...));
  }

  inline auto Print(const types::StringView text) {
    WriteToConsole(text);
  }

  template <typename... Args>
  inline auto Println(std::format_string<Args...> fmt, Args&&... args) {
    WriteToConsole(std::format(fmt, std::forward<Args>(args)...) + '\n');
  }

  inline auto Println(const types::StringView text) {
    types::String textWithNewline(text);
    textWithNewline += '\n';
    WriteToConsole(textWithNewline);
  }

  inline auto Println() {
    WriteToConsole("\n");
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Timestamp
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Returns a ISO8601-like timestamp string (YYYY-MM-DDTHH:MM:SS).
   */
  inline auto GetCachedTimestamp(const std::time_t timeT) -> types::StringView {
    thread_local auto                   LastTt   = static_cast<std::time_t>(-1);
    thread_local types::Array<char, 20> TsBuffer = { '\0' };

    if (timeT != LastTt) {
      std::tm localTm {};

      if (
#ifdef _WIN32
        localtime_s(&localTm, &timeT) == 0
#else
        localtime_r(&timeT, &localTm) != nullptr
#endif
      ) {
        if (std::strftime(TsBuffer.data(), TsBuffer.size(), LogLevelConst::TIMESTAMP_FORMAT, &localTm) == 0)
          std::copy_n("????-??-??T??:??:??", 20, TsBuffer.data());
      } else
        std::copy_n("????-??-??T??:??:??", 20, TsBuffer.data());

      LastTt = timeT;
    }

    return { TsBuffer.data(), 19 };
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Structured Fields Support
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @struct Field
   * @brief Represents a key-value field for structured logging.
   */
  struct Field {
    types::StringView key;
    types::String     value;

    template <typename T>
    static auto create(types::StringView k, const T& v) -> Field {
      if constexpr (std::is_same_v<std::decay_t<T>, types::String> || std::is_same_v<std::decay_t<T>, types::StringView> ||
                    std::is_same_v<std::decay_t<T>, const char*> || std::is_same_v<std::decay_t<T>, char*>) {
        return Field { k, types::String(v) };
      } else if constexpr (std::is_same_v<std::decay_t<T>, bool>) {
        return Field { k, v ? "true" : "false" };
      } else {
        return Field { k, std::format("{}", v) };
      }
    }
  };

  /**
   * @brief Formats fields into a string like: key=value, key2=value2
   */
  inline auto FormatFields(const types::Vec<Field>& fields) -> types::String {
    if (fields.empty())
      return "";

    types::String result;
    result.reserve(fields.size() * 20);

    for (types::usize i = 0; i < fields.size(); ++i) {
      if (i > 0)
        result += ", ";
      result += Stylize(fields[i].key, { .bold = true });
      result += "=";
      result += fields[i].value;
    }

    return result;
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Span Support (Tracing-style execution context tracking)
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @struct SpanInfo
   * @brief Information about an active span.
   */
  struct SpanInfo {
    types::String      name;
    types::Vec<Field>  fields;
    types::String      target;
  };

  /**
   * @brief Gets the thread-local span stack.
   */
  inline auto GetSpanStack() -> std::stack<SpanInfo>& {
    thread_local std::stack<SpanInfo> SpanStackInstance;
    return SpanStackInstance;
  }

  /**
   * @class SpanGuard
   * @brief RAII guard that enters a span on construction and exits on destruction.
   */
  class SpanGuard {
  public:
    SpanGuard(types::String name, types::String target, types::Vec<Field> fields = {})
        : m_active(true) {
      GetSpanStack().push(SpanInfo { std::move(name), std::move(fields), std::move(target) });
    }

    ~SpanGuard() {
      if (m_active && !GetSpanStack().empty())
        GetSpanStack().pop();
    }

    // Non-copyable, movable
    SpanGuard(const SpanGuard&)                    = delete;
    auto operator=(const SpanGuard&) -> SpanGuard& = delete;

    SpanGuard(SpanGuard&& other) noexcept : m_active(other.m_active) {
      other.m_active = false;
    }

    auto operator=(SpanGuard&& other) noexcept -> SpanGuard& {
      if (this != &other) {
        if (m_active && !GetSpanStack().empty())
          GetSpanStack().pop();
        m_active       = other.m_active;
        other.m_active = false;
      }
      return *this;
    }

  private:
    bool m_active;
  };

  // ─────────────────────────────────────────────────────────────────────────────
  // Target Extraction
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Extracts a target string from a function name (converts to module-like path).
   * @details Converts "void draconis::core::GetCpuInfo()" to "draconis::core"
   */
  inline auto ExtractTarget(const char* funcName) -> types::String {
    types::StringView func(funcName);

    // Find the last :: before the function name (which is followed by '(')
    auto parenPos = func.rfind('(');
    if (parenPos == types::StringView::npos)
      parenPos = func.size();

    auto lastColonPos = func.rfind("::", parenPos);
    if (lastColonPos == types::StringView::npos)
      return types::String(func.substr(0, parenPos));

    // Find where the namespace/class path starts (skip return type)
    auto spacePos = func.rfind(' ', lastColonPos);
    types::usize startPos = (spacePos != types::StringView::npos) ? spacePos + 1 : 0;

    return types::String(func.substr(startPos, lastColonPos - startPos));
  }

  // ─────────────────────────────────────────────────────────────────────────────
  // Core Logging Implementation
  // ─────────────────────────────────────────────────────────────────────────────

  /**
   * @brief Core logging implementation with full tracing-style support.
   */
  template <typename... Args>
  auto LogImpl(
    const LogLevel              level,
    const std::source_location& loc,
    const types::StringView     target,
    const types::Vec<Field>&    fields,
    std::format_string<Args...> fmt,
    Args&&... args
  ) {
    using namespace std::chrono;
    using std::filesystem::path;

    if (level < GetRuntimeLogLevel())
      return;

    const auto              nowTp     = system_clock::now();
    const std::time_t       nowTt     = system_clock::to_time_t(nowTp);
    const types::StringView timestamp = GetCachedTimestamp(nowTt);
    const types::String     message   = std::format(fmt, std::forward<Args>(args)...);

    // Format location
    const types::String fileLine = std::format(
      "{}:{}",
      path(loc.file_name()).filename().string(),
      loc.line()
    );

    // Build fields string
    types::String fieldsStr = FormatFields(fields);

    {
      const types::LockGuard lock(GetLogMutex());

#ifdef DRAC_PRETTY_LOG
      // Pretty multi-line format (like tracing's Pretty formatter)
      // Line 1: timestamp LEVEL target: message, fields
      Print(level, "  ");
      Print(level, Stylize(timestamp, { .color = LogColor::Gray, .dim = true }));
      Print(level, " ");
      Print(level, GetLevelInfo().at(static_cast<types::usize>(level)));
      Print(level, " ");
      Print(level, Stylize(target, { .bold = true }));
      Print(level, ": ");
      Print(level, message);
      if (!fieldsStr.empty()) {
        Print(level, ", ");
        Print(level, fieldsStr);
      }
      Println(level);

      // Line 2: at file:line
      Print(level, Stylize(LogLevelConst::AT_PREFIX, { .color = LogColor::Gray, .italic = true }));
      Println(level, Stylize(fileLine, { .color = LogColor::Gray, .italic = true }));

      // Lines 3+: in span_name with fields (for each span in stack)
      auto& spanStack = GetSpanStack();
      if (!spanStack.empty()) {
        // We need to iterate from bottom to top, so copy to vector
        std::stack<SpanInfo> tempStack = spanStack;
        types::Vec<SpanInfo> spans;
        while (!tempStack.empty()) {
          spans.push_back(tempStack.top());
          tempStack.pop();
        }
        // Reverse to get bottom-to-top order (outermost span first)
        for (auto it = spans.rbegin(); it != spans.rend(); ++it) {
          Print(level, Stylize(LogLevelConst::IN_PREFIX, { .color = LogColor::Gray, .italic = true }));
          Print(level, Stylize(it->target, { .color = LogColor::Gray, .italic = true }));
          Print(level, Stylize("::", { .color = LogColor::Gray, .italic = true }));
          Print(level, Stylize(it->name, { .bold = true, .color = LogColor::Gray }));
          if (!it->fields.empty()) {
            Print(level, Stylize(" with ", { .color = LogColor::Gray, .italic = true }));
            Print(level, Stylize(FormatFields(it->fields), { .color = LogColor::Gray }));
          }
          Println(level);
        }
      }

      Println(level); // Empty line between events
#else
      // Compact format: timestamp LEVEL file:line target: message, fields
      Print(level, Stylize(timestamp, { .color = LogColor::Gray, .dim = true }));
      Print(level, " ");
      Print(level, GetLevelInfo().at(static_cast<types::usize>(level)));
      Print(level, " ");
  #ifndef NDEBUG
      Print(level, Stylize(fileLine, { .color = LogColor::Gray, .italic = true }));
      Print(level, " ");
  #endif
      Print(level, Stylize(target, { .bold = true }));
      Print(level, ": ");
      Print(level, message);
      if (!fieldsStr.empty()) {
        Print(level, ", ");
        Print(level, fieldsStr);
      }
      Println(level);
#endif
    }
  }

  /**
   * @brief Simplified log implementation without fields.
   */
  template <typename... Args>
  auto LogImpl(
    const LogLevel              level,
    const std::source_location& loc,
    const types::StringView     target,
    std::format_string<Args...> fmt,
    Args&&... args
  ) {
    LogImpl(level, loc, target, types::Vec<Field> {}, fmt, std::forward<Args>(args)...);
  }

  /**
   * @brief Log an error object at the specified level.
   */
  template <typename ErrorType>
  auto LogError(
    const LogLevel          level,
    const types::StringView target,
    const ErrorType&        error_obj
  ) {
    using DecayedErrorType = std::decay_t<ErrorType>;

    std::source_location logLocation;
    types::String        errorMessagePart;

    if constexpr (std::is_same_v<DecayedErrorType, error::DracError>) {
      logLocation      = error_obj.location;
      errorMessagePart = error_obj.message;
    } else {
      logLocation = std::source_location::current();
      if constexpr (std::is_base_of_v<std::exception, DecayedErrorType>)
        errorMessagePart = error_obj.what();
      else if constexpr (requires { error_obj.message; })
        errorMessagePart = error_obj.message;
      else
        errorMessagePart = "Unknown error type logged";
    }

    LogImpl(level, logLocation, target, "{}", errorMessagePart);
  }

} // namespace draconis::utils::logging

// ─────────────────────────────────────────────────────────────────────────────
// Macros
// ─────────────────────────────────────────────────────────────────────────────

// Helper to extract target from current function
#define DRAC_LOG_TARGET ::draconis::utils::logging::ExtractTarget(__FUNCTION__)

// Field creation macro for structured logging
#define field(name, value) ::draconis::utils::logging::Field::create(#name, value)

// Span macros - create an RAII span guard
#define span_enter(name, ...)                      \
  auto _drac_span_guard_##__LINE__ =               \
    ::draconis::utils::logging::SpanGuard(         \
      #name,                                       \
      DRAC_LOG_TARGET,                             \
      ::draconis::utils::types::Vec<::draconis::utils::logging::Field> { __VA_ARGS__ } \
    )

// Log macros with target and optional fields
#define trace_log(fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Trace, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define debug_log(fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Debug, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define info_log(fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Info, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define warn_log(fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Warn, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define error_log(fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Error, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

// Log with fields macros
#define trace_log_fields(fields_vec, fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Trace, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fields_vec, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define debug_log_fields(fields_vec, fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Debug, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fields_vec, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define info_log_fields(fields_vec, fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Info, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fields_vec, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define warn_log_fields(fields_vec, fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Warn, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fields_vec, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

#define error_log_fields(fields_vec, fmt, ...) \
  ::draconis::utils::logging::LogImpl( \
    ::draconis::utils::logging::LogLevel::Error, \
    std::source_location::current(), \
    DRAC_LOG_TARGET, \
    fields_vec, \
    fmt __VA_OPT__(, ) __VA_ARGS__)

// Error object logging macros
#define trace_at(error_obj) \
  ::draconis::utils::logging::LogError( \
    ::draconis::utils::logging::LogLevel::Trace, \
    DRAC_LOG_TARGET, \
    error_obj)

#define debug_at(error_obj) \
  ::draconis::utils::logging::LogError( \
    ::draconis::utils::logging::LogLevel::Debug, \
    DRAC_LOG_TARGET, \
    error_obj)

#define info_at(error_obj) \
  ::draconis::utils::logging::LogError( \
    ::draconis::utils::logging::LogLevel::Info, \
    DRAC_LOG_TARGET, \
    error_obj)

#define warn_at(error_obj) \
  ::draconis::utils::logging::LogError( \
    ::draconis::utils::logging::LogLevel::Warn, \
    DRAC_LOG_TARGET, \
    error_obj)

#define error_at(error_obj) \
  ::draconis::utils::logging::LogError( \
    ::draconis::utils::logging::LogLevel::Error, \
    DRAC_LOG_TARGET, \
    error_obj)
