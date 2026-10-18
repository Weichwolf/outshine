#ifndef OUTSHINE_TEST_GROUNDCLASSINVENTORY_H
#define OUTSHINE_TEST_GROUNDCLASSINVENTORY_H

#include "Readback.h"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <span>
#include <string>

namespace GroundClassInventory {
inline bool Capture(outshine::Render::Native::Device *device,
                    outshine::Render::Native::Buffer *buffer,
                    std::span<const uint32_t> words,
                    const std::string &path) {
  if (words.size() < 4 || words.size_bytes() > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  outshine::Render::Readback gpu;
  if (gpu.FromBuffer(device, buffer, static_cast<uint32_t>(words.size_bytes())) !=
          outshine::Render::ReadState::Ready ||
      std::memcmp(gpu.Rows(), words.data(), words.size_bytes()) != 0) {
    return false;
  }
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(words.data()),
               static_cast<std::streamsize>(words.size_bytes()));
  output.close();
  return static_cast<bool>(output);
}
}
#endif
