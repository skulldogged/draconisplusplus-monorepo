#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_PLUGINS
  #include <Drac++/Core/PluginManager.hpp>
#endif

#include <algorithm>
#include <cctype>
#include <magic_enum/magic_enum.hpp>
#include <typeinfo>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "CLI.hpp"
#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::utils::localization;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;
using namespace draconis::cli;
using draconis::utils::error::DracError;

struct CliOptions {
  // Modes
  bool   doctorMode    = false;
  bool   benchmarkMode = false;
  bool   listPlugins   = false;
  String pluginInfo;

  // Cache control
  bool clearCache     = false;
  bool ignoreCacheRun = false;

  // Output options
  bool   noAscii = false;
  String outputFormat;
  String compactFormat;

  // Localization
  String language;

  // Logo options
  String logoPath;
  String logoProtocol;
  u32    logoWidth  = 0;
  u32    logoHeight = 0;

  // Misc
  bool   showConfigPath = false;
  String generateCompletions;
};

auto main(const i32 argc, CStr* argv[]) -> i32 try {
  CliOptions opts;

  {
    using draconis::utils::argparse::Argument;
    using draconis::utils::argparse::ArgumentParser;

    // Build enhanced version string with build date and git hash
#if defined(DRAC_BUILD_DATE) && defined(DRAC_GIT_HASH)
    String versionString = std::format("draconis++ {} ({}) [{}]", DRAC_VERSION, DRAC_BUILD_DATE, DRAC_GIT_HASH);
#elif defined(DRAC_BUILD_DATE)
    String versionString = std::format("draconis++ {} ({})", DRAC_VERSION, DRAC_BUILD_DATE);
#else
    String versionString = std::format("draconis++ {}", DRAC_VERSION);
#endif

    ArgumentParser parser(versionString);

    parser
      .addArguments("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .addArguments("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag()
      .bindTo(opts.doctorMode);

    parser
      .addArguments("-l", "--log-level")
      .help("Set the minimum log level.")
      .defaultValue(LogLevel::Info);

    parser
      .addArguments("--clear-cache")
      .help("Clears the cache. This will remove all cached data, including in-memory and on-disk copies.")
      .flag()
      .bindTo(opts.clearCache);

    parser
      .addArguments("--lang")
      .help("Set the language for localization (e.g., 'en', 'es', 'fr', 'de').")
      .defaultValue(String(""))
      .bindTo(opts.language);

    parser
      .addArguments("--ignore-cache")
      .help("Ignore cache for this run (fetch fresh data without reading/writing on-disk cache).")
      .flag()
      .bindTo(opts.ignoreCacheRun);

    parser
      .addArguments("--no-ascii")
      .help("Disable ASCII art display.")
      .flag()
      .bindTo(opts.noAscii);

    parser
      .addArguments("--format")
      .help("Output system information using an output format plugin (e.g., 'markdown', 'json', 'json-pretty', 'yaml').")
      .defaultValue(String(""))
      .bindTo(opts.outputFormat);

    parser
      .addArguments("--compact")
      .help(
        "Output a single line using a template string (e.g., '{host} | {cpu} | {ram}'). "
        "Available placeholders: {date}, {host}, {os}, {kernel}, {cpu}, {gpu}, {ram}, {disk}, "
        "{uptime}, {shell}, {de}, {wm}, {packages}, {weather}, {playing}."
      )
      .defaultValue(String(""))
      .bindTo(opts.compactFormat);

    parser
      .addArguments("--logo-path")
      .help("Path to an image to render in the logo area (kitty / kitty-direct / iterm2).")
      .defaultValue(String(""))
      .bindTo(opts.logoPath);

    parser
      .addArguments("--logo-protocol")
      .help("Logo image protocol: 'kitty', 'kitty-direct', or 'iterm2'.")
      .defaultValue(String(""))
      .bindTo(opts.logoProtocol);

    parser
      .addArguments("--logo-width")
      .help("Logo image width in pixels.")
      .defaultValue(i32(0))
      .bindTo(opts.logoWidth, [](const Argument& arg) -> u32 { return static_cast<u32>(std::max(0, arg.get<i32>())); });

    parser
      .addArguments("--logo-height")
      .help("Logo image height in pixels.")
      .defaultValue(i32(0))
      .bindTo(opts.logoHeight, [](const Argument& arg) -> u32 { return static_cast<u32>(std::max(0, arg.get<i32>())); });

#if DRAC_ENABLE_PLUGINS
    parser
      .addArguments("--list-plugins")
      .help("List all available and loaded plugins.")
      .flag()
      .bindTo(opts.listPlugins);

    parser
      .addArguments("--plugin-info")
      .help("Show detailed information about a specific plugin.")
      .defaultValue(String(""))
      .bindTo(opts.pluginInfo);
#endif

    parser
      .addArguments("--benchmark")
      .help("Print timing information for each data source.")
      .flag()
      .bindTo(opts.benchmarkMode);

    parser
      .addArguments("--show-config-path")
      .help("Display the active configuration file location.")
      .flag()
      .bindTo(opts.showConfigPath);

    parser
      .addArguments("--generate-completions")
      .help("Generate shell completion script. Supported shells: bash, zsh, fish, powershell.")
      .defaultValue(String(""))
      .bindTo(opts.generateCompletions);

    if (Result<> result = parser.parseInto({ argv, static_cast<usize>(argc) }); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    SetRuntimeLogLevel(
      parser.get<bool>("-V") || parser.get<bool>("--verbose")
        ? LogLevel::Debug
        : parser.getEnum<LogLevel>("--log-level")
    );
  }

  using draconis::utils::cache::CacheManager, draconis::utils::cache::CachePolicy;

  CacheManager cache;

  if (opts.ignoreCacheRun)
    CacheManager::ignoreCache = true;

  cache.setGlobalPolicy(CachePolicy::tempDirectory());

  if (opts.clearCache) {
    const usize removedCount = cache.invalidateAll(true);

    if (removedCount > 0)
      Println("Removed {} files.", removedCount);
    else
      Println("No cache files were found to clear.");

    return EXIT_SUCCESS;
  }

  // Handle --generate-completions (early exit, no config needed)
  if (!opts.generateCompletions.empty()) {
    GenerateCompletions(opts.generateCompletions);
    return EXIT_SUCCESS;
  }

  // Handle --show-config-path (early exit)
  if (opts.showConfigPath) {
#if DRAC_PRECOMPILED_CONFIG
    Println("Using precompiled configuration (no external config file).");
#else
    Println("{}", Config::getConfigPath().string());
#endif
    return EXIT_SUCCESS;
  }

  {
    Config config = Config::getInstance();

    // Initialize translation manager with language from command line or config
    if (opts.language.empty() && config.general.language)
      opts.language = *config.general.language;

    // Initialize translation manager (this will auto-detect system language)
    TranslationManager& translationManager = GetTranslationManager();

    if (!opts.language.empty())
      translationManager.setLanguage(opts.language);

    if (!opts.logoPath.empty())
      config.logo.imagePath = opts.logoPath;

    if (!opts.logoProtocol.empty()) {
      String protoLower = opts.logoProtocol;
      std::ranges::transform(
        protoLower,
        protoLower.begin(),
        [](unsigned char chr) -> char {
          return static_cast<char>(std::tolower(chr));
        }
      );

      config.logo.protocol = protoLower;
    }

    if (opts.logoWidth > 0)
      config.logo.width = opts.logoWidth;

    if (opts.logoHeight > 0)
      config.logo.height = opts.logoHeight;

#if DRAC_ENABLE_PLUGINS
    // Initialize plugin system early for maximum performance
    auto& pluginManager = draconis::core::plugin::GetPluginManager();

    const auto pluginInitStart = std::chrono::steady_clock::now();
    if (auto initResult = pluginManager.initialize(config.plugins); !initResult)
      warn_log("Plugin system initialization failed: {}", initResult.error().message);
    else
      debug_log("Plugin system initialized successfully");
    const f64 pluginInitializationMs = std::chrono::duration<f64, std::milli>(
                                         std::chrono::steady_clock::now() - pluginInitStart
    )
                                         .count();
    // Handle plugin-specific commands with early exit for performance
    if (opts.listPlugins || !opts.pluginInfo.empty()) {
      pluginManager.loadPluginsOfType(draconis::core::plugin::PluginType::InfoProvider, cache);
      pluginManager.loadPluginsOfType(draconis::core::plugin::PluginType::OutputFormat, cache);
    }

    if (opts.listPlugins)
      return HandleListPluginsCommand(pluginManager);

    if (!opts.pluginInfo.empty())
      return HandlePluginInfoCommand(pluginManager, opts.pluginInfo);
#else
    constexpr f64 pluginInitializationMs = 0.0;
#endif

    // Handle benchmark mode (runs timing for each data source)
    if (opts.benchmarkMode) {
      Vec<BenchmarkResult> results = RunBenchmark(cache, config, pluginInitializationMs);
      PrintBenchmarkReport(results);
      return EXIT_SUCCESS;
    }

    SystemInfo data(cache, config, opts.compactFormat);

    if (opts.doctorMode) {
      PrintDoctorReport(data);

      return EXIT_SUCCESS;
    }

    if (!opts.outputFormat.empty()) {
#if DRAC_ENABLE_PLUGINS
      FormatOutputViaPlugin(opts.outputFormat, data);
#else
      Print("Plugin output formats require plugin support to be enabled.\n");
#endif
    } else if (!opts.compactFormat.empty())
      PrintCompactOutput(opts.compactFormat, data);
    else
      Print(CreateUI(config, data, opts.noAscii));
  }

  return EXIT_SUCCESS;
} catch (const DracError& e) {
  error_log(
    "Unhandled DracError [{}] at {}:{} ({}): {}",
    magic_enum::enum_name(e.code),
    e.location.file_name(),
    e.location.line(),
    e.location.function_name(),
    e.message
  );
  return EXIT_FAILURE;
} catch (const Exception& e) {
  error_log("Unhandled standard exception [{}]: {}", typeid(e).name(), e.what());
  return EXIT_FAILURE;
} catch (...) {
  error_log("Unhandled non-standard C++ exception (type and message unavailable)");
  return EXIT_FAILURE;
}
