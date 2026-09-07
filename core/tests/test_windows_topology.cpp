#include <boost/ut.hpp>

#include <Drac++/Utils/WindowsTopology.hpp>

auto main() -> int {
  using namespace boost::ut;
  using draconis::utils::types::Vec;
  using draconis::utils::windows::CountProcessorCores;
  "Topology larger than the old stack buffer"_test = [] {
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX record {};
    record.Relationship = RelationProcessorCore;
    record.Size         = sizeof(record);
    Vec<BYTE> bytes(sizeof(record) * 64);
    for (size_t i = 0; i < 64; ++i)
      std::memcpy(bytes.data() + i * sizeof(record), &record, sizeof(record));
    auto result = CountProcessorCores(bytes);
    expect(result.has_value());
    if (result)
      expect(*result == 64_u);
    record.Size = 0;
    std::memcpy(bytes.data(), &record, sizeof(record));
    expect(!CountProcessorCores(bytes));
    record.Size = static_cast<DWORD>(bytes.size() + 1);
    std::memcpy(bytes.data(), &record, sizeof(record));
    expect(!CountProcessorCores(bytes));
    record.Size         = sizeof(record);
    record.Relationship = RelationNumaNode;
    std::memcpy(bytes.data(), &record, sizeof(record));
    expect(!CountProcessorCores(bytes));
    expect(!CountProcessorCores(draconis::utils::types::Span<const BYTE>(bytes).first(3)));
  };
}
