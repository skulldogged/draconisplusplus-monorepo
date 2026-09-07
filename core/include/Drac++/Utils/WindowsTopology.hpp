#pragma once

#ifdef _WIN32
  #include <cstddef>
  #include <cstring>
  #include <limits>
  #include <windows.h>

  #include "Error.hpp"

namespace draconis::utils::windows {
  // The OS returns variable-length records; never traverse an unchecked Size.
  inline auto CountProcessorCores(types::Span<const BYTE> buffer) -> types::Result<types::u16> {
    using enum error::DracErrorCode;
    constexpr auto headerSize = offsetof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, Processor);
    types::usize   count      = 0;
    while (!buffer.empty()) {
      if (buffer.size() < headerSize)
        ERR(ParseError, "Truncated processor topology header");
      SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX header {};
      std::memcpy(&header, buffer.data(), headerSize);
      if (header.Relationship != RelationProcessorCore || header.Size < headerSize || header.Size > buffer.size())
        ERR(ParseError, "Invalid processor topology record");
      if (++count > std::numeric_limits<types::u16>::max())
        ERR(ResourceExhausted, "Processor count exceeds the public API range");
      buffer = buffer.subspan(header.Size);
    }
    if (count == 0)
      ERR(NotFound, "No processor cores returned");
    return static_cast<types::u16>(count);
  }
} // namespace draconis::utils::windows
#endif
