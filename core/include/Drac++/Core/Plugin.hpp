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
#include <format>
#include <fstream>
#include <glaze/glaze.hpp>
#include <iterator>
#include <type_traits>

// Required for DRAC_PLUGIN macro which uses draconis::utils::logging::LogLevel and SetLogLevelPtr
#include "../Utils/CacheManager.hpp"
#include "../Utils/Logging.hpp" // IWYU pragma: keep
#include "../Utils/Types.hpp"

// Plugins use the same typed, atomic, bounded cache implementation as core.
class PluginCache {
  using CacheManager = draconis::utils::cache::CacheManager;
  using String       = draconis::utils::types::String;
  std::shared_ptr<CacheManager> m_manager;
  std::shared_ptr<void>         m_module;
  String                        m_prefix;

 public:
  template <typename T>
  using CacheEntry = CacheManager::CacheEntry<T>;
  explicit PluginCache(const std::filesystem::path& directory)
    : m_manager(std::make_shared<CacheManager>(directory)) {}

  // The shared service remains alive when a C consumer destroys its cache handle.
  auto bind(std::shared_ptr<CacheManager> manager, String prefix) -> void {
    manager->retainModule(m_module);
    m_manager = std::move(manager);
    m_prefix  = std::move(prefix);
  }
  template <typename T>
  auto get(const String& key) const -> draconis::utils::types::Option<T> {
    if (!CacheManager::isValidKey(key))
      return {};
    return m_manager->get<T>(m_prefix + key, draconis::utils::cache::CachePolicy::neverExpire());
  }
  template <typename T>
  auto set(const String& key, const T& value, draconis::utils::types::u32 ttlSeconds = 0) -> void {
    if (!CacheManager::isValidKey(key))
      return;
    using namespace draconis::utils::cache;
    const CachePolicy policy { .location = CacheLocation::Persistent,
                               .ttl      = ttlSeconds ? draconis::utils::types::Option<std::chrono::seconds>(std::chrono::seconds(ttlSeconds)) : draconis::utils::types::None };
    m_manager->set(m_prefix + key, value, policy);
  }
  auto invalidate(const String& key) -> void {
    if (CacheManager::isValidKey(key))
      m_manager->invalidate(m_prefix + key);
  }
  auto retainModule(std::shared_ptr<void> module) -> void {
    m_manager->retainModule(module);
    m_module = std::move(module);
  }
  template <typename T, typename Fetcher>
  auto getOrSet(const String& key, draconis::utils::types::u32 ttlSeconds, Fetcher&& fetcher) -> draconis::utils::types::Result<T> {
    if (!CacheManager::isValidKey(key))
      return fetcher();
    const draconis::utils::cache::CachePolicy policy {
      .location = draconis::utils::cache::CacheLocation::Persistent,
      .ttl      = std::chrono::seconds(ttlSeconds),
    };
    return m_manager->getOrSet<T>(m_prefix + key, policy, std::forward<Fetcher>(fetcher));
  }
};

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

  using PluginFieldArray  = utils::types::Vec<PluginFieldValue>;
  using PluginFieldObject = utils::types::Map<utils::types::String, PluginFieldValue>;

  using PluginFieldValueBase = utils::types::Variant<
    bool,
    utils::types::i64,
    utils::types::u64,
    utils::types::f64,
    utils::types::String,
    PluginFieldArray,
    PluginFieldObject>;

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

  using PluginFields = PluginFieldObject;
  using PluginData   = utils::types::Map<utils::types::String, PluginFields>;

  inline auto AppendPluginFieldValue(utils::types::String& result, const PluginFieldValue& value) -> utils::types::Unit {
    using namespace utils::types;

    std::visit(
      [&result](const auto& inner) -> Unit {
        using T = std::decay_t<decltype(inner)>;

        if constexpr (std::same_as<T, bool>)
          result += inner ? "true" : "false";
        else if constexpr (std::same_as<T, i64> || std::same_as<T, u64>)
          std::format_to(std::back_inserter(result), "{}", inner);
        else if constexpr (std::same_as<T, f64>)
          std::format_to(std::back_inserter(result), "{}", inner);
        else if constexpr (std::same_as<T, String>)
          result += inner;
        else if constexpr (std::same_as<T, PluginFieldArray>) {
          for (usize i = 0; i < inner.size(); ++i) {
            if (i > 0)
              result += ", ";
            AppendPluginFieldValue(result, inner[i]);
          }
        } else {
          bool first = true;
          for (const auto& [key, item] : inner) {
            if (!first)
              result += ", ";
            first = false;
            result += key;
            result += ": ";
            AppendPluginFieldValue(result, item);
          }
        }
      },
      static_cast<const PluginFieldValueBase&>(value)
    );
  }

  [[nodiscard]] inline auto PluginFieldToString(const PluginFieldValue& value) -> utils::types::String {
    utils::types::String result;
    result.reserve(64);
    AppendPluginFieldValue(result, value);
    return result;
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
      const utils::types::String&                                          formatName,
      const utils::types::Map<utils::types::String, utils::types::String>& data,
      const PluginData&                                                    pluginData
    ) const -> utils::types::Result<utils::types::String> = 0;

    /**
     * @brief Get all format names this plugin supports
     * @return View of supported format names (e.g., {"json", "json-pretty"})
     */
    [[nodiscard]] virtual auto getFormatNames() const -> utils::types::Span<const utils::types::String> = 0;

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

namespace glz {
  template <>
  struct to<JSON, draconis::core::plugin::PluginFieldValue> {
    template <auto Opts>
    static auto op(
      const draconis::core::plugin::PluginFieldValue& value,
      auto&&                                          ctx,
      auto&&                                          buffer,
      auto&&                                          index
    ) -> void {
      std::visit(
        [&](const auto& inner) -> void {
          using Value = std::decay_t<decltype(inner)>;
          to<JSON, Value>::template op<Opts>(inner, ctx, buffer, index);
        },
        static_cast<const draconis::core::plugin::PluginFieldValueBase&>(value)
      );
    }
  };
} // namespace glz

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
  #define DRAC_PLUGIN(PluginClass)                                                      \
    namespace draconis::plugins {                                                       \
      static auto Create_##PluginClass() -> ::draconis::core::plugin::IPlugin* {        \
        return new PluginClass();                                                       \
      }                                                                                 \
      static auto Destroy_##PluginClass(::draconis::core::plugin::IPlugin* p) -> void { \
        delete p;                                                                       \
      }                                                                                 \
    }                                                                                   \
    extern "C" DRAC_PLUGIN_API void DracRegisterPlugin_##PluginClass() {                \
      ::draconis::core::plugin::RegisterStaticPlugin(                                   \
        #PluginClass,                                                                   \
        { ::draconis::plugins::Create_##PluginClass,                                    \
          ::draconis::plugins::Destroy_##PluginClass }                                  \
      );                                                                                \
    }

#else
// NOLINTBEGIN(bugprone-macro-parentheses) - false positive
  #define DRAC_PLUGIN(PluginClass)                                                                            \
    extern "C" DRAC_PLUGIN_API auto DracPluginAbiVersion() -> unsigned int {                                  \
      return 2;                                                                                               \
    }                                                                                                         \
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
