#ifndef OUTSHINE_CORE_IO_PNG_H
#define OUTSHINE_CORE_IO_PNG_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Io {

struct Png {
  bool Read = false;
  uint32_t Wide = 0;
  uint32_t High = 0;
  uint32_t Channels = 0;
  std::vector<uint8_t> Bytes;
  std::string Error;
};

[[nodiscard]] Png ReadPng(const uint8_t *bytes, size_t length);

} // namespace outshine::Io

#endif
