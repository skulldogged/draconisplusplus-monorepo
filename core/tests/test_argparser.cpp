#include <boost/ut.hpp>

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/Types.hpp>

auto main() -> int {
  using namespace boost::ut;
  using namespace draconis::utils::argparse;
  using namespace draconis::utils::types;

  "ArgumentParser flag"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    bool           verbose = false;
    parser.addArguments("-v", "--verbose").flag().bindTo(verbose);

    const Vec<String>         args   = { "testprog", "--verbose" };
    const Result<ParseAction> result = parser.parseInto(args);

    expect(result.has_value());
    expect(verbose);
    expect(parser.isUsed("-v"));
  };

  "ArgumentParser value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    String         output;
    parser.addArguments("-o", "--output").bindTo(output);

    const Vec<String>         args   = { "testprog", "-o", "out.txt" };
    const Result<ParseAction> result = parser.parseInto(args);

    expect(result.has_value());
    expect(output == String("out.txt"));
  };

  "ArgumentParser integer conversion"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    i32            count = 0;
    parser.addArguments("-c", "--count").defaultValue(i32(1)).bindTo(count);

    const Vec<String>         args   = { "testprog", "--count", "42" };
    const Result<ParseAction> result = parser.parseInto(args);

    expect(result.has_value());
    expect(count == 42);
  };

  "ArgumentParser default value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    i32            count = 0;
    parser.addArguments("-c", "--count").defaultValue(i32(10)).bindTo(count);

    const Vec<String>         args   = { "testprog" };
    const Result<ParseAction> result = parser.parseInto(args);

    expect(result.has_value());
    expect(count == 10);
  };

  "ArgumentParser missing value"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    parser.addArguments("-o");

    const Vec<String>         args   = { "testprog", "-o" };
    const Result<ParseAction> result = parser.parseArgs(args);

    expect(!result.has_value());
  };

  "ArgumentParser unknown argument"_test = [] -> void {
    ArgumentParser            parser("0.1.0");
    const Vec<String>         args   = { "testprog", "--unknown" };
    const Result<ParseAction> result = parser.parseArgs(args);

    expect(!result.has_value());
  };

  "ArgumentParser returns help action"_test = [] -> void {
    ArgumentParser    parser("0.1.0");
    const Vec<String> args   = { "testprog", "--help" };
    const auto        result = parser.parseArgs(args);

    expect(result.has_value());
    expect(*result == ParseAction::ShowHelp);
  };

  "ArgumentParser returns version action"_test = [] -> void {
    ArgumentParser    parser("0.1.0");
    const Vec<String> args   = { "testprog", "--version" };
    const auto        result = parser.parseArgs(args);

    expect(result.has_value());
    expect(*result == ParseAction::ShowVersion);
  };

  "ArgumentParser duplicate aliases use latest registration"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    bool           first  = false;
    bool           second = false;
    parser.addArguments("--same").flag().bindTo(first);
    parser.addArguments("--same").flag().bindTo(second);

    const Vec<String> args   = { "testprog", "--same" };
    const auto        result = parser.parseInto(args);

    expect(result.has_value());
    expect(!first);
    expect(second);
  };

  "ArgumentParser retains argument references across growth"_test = [] -> void {
    ArgumentParser parser("0.1.0");
    Argument&      retained = parser.addArguments("--retained");
    for (usize index = 0; index < 256; ++index)
      parser.addArguments("--extra-" + std::to_string(index)).flag();
    parser.reserveArguments(1024);

    bool selected = false;
    retained.flag().bindTo(selected);
    const Vec<String> args   = { "testprog", "--retained" };
    const auto        result = parser.parseInto(args);

    expect(result.has_value());
    expect(selected);
  };

  "ArgumentParser enum defaults replace previous choices"_test = [] -> void {
    enum class Mode : u8 { First,
                      Second };
    ArgumentParser parser("0.1.0");
    Argument&      argument = parser.addArguments("--mode").choices({ "old" });
    expect(argument.setValue(String("old")).has_value());
    argument.defaultValue(Mode::First);

    const Vec<String> args   = { "testprog", "--mode", "second" };
    const auto        result = parser.parseArgs(args);
    expect(result.has_value());
    expect(parser.getEnum<Mode>("--mode") == Mode::Second);
    expect(!argument.setValue(String("old")).has_value());
  };

  return 0;
}
