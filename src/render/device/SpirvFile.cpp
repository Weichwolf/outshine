#include "SpirvFile.h"
#include "ReadTextFile.h"
#include <cstring>

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {

namespace {

constexpr uint32_t kSpirvMagic = 0x07230203u;

std::expected<std::string, std::string> ReadShaderFile(std::string_view treePath) {
  auto held = ReadTextFile(treePath, std::numeric_limits<size_t>::max());
  if (!held) { return std::unexpected("shader artifact " + held.error()); }
  if (held->empty()) {
    return std::unexpected("the shader artifact " + std::string(treePath) +
                           " is empty, and an empty kernel is a picture refusal");
  }
  return held;
}

}

std::expected<std::vector<uint32_t>, std::string> ReadSpirv(std::string_view path) {
  const auto bytes = ReadShaderFile(path);
  if (!bytes) { return std::unexpected(bytes.error()); }
  constexpr size_t headerWords = 5;
  if (bytes->size() < headerWords * sizeof(uint32_t) || bytes->size() % sizeof(uint32_t) != 0) {
    return std::unexpected(std::string(path) + " is not a complete SPIR-V word stream");
  }
  std::vector<uint32_t> words(bytes->size() / sizeof(uint32_t));
  std::memcpy(words.data(), bytes->data(), bytes->size());
  if (words.front() != kSpirvMagic) {
    return std::unexpected(std::string(path) + " has no SPIR-V header");
  }
  return words;
}

}
