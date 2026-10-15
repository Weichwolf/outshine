#include "SpirvFile.h"
#include <array>
#include <cstdio>
#include <cstring>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Render {

namespace {

constexpr size_t kReadBlockBytes = 1u << 14u;
constexpr uint32_t kSpirvMagic = 0x07230203u;

std::expected<std::string, std::string> ReadShaderFile(std::string_view treePath) {
  const std::string path(treePath);
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    return std::unexpected("the shader artifact " + path +
                           " is not readable from here -- the engine reads its shaders from the "
                           "tree, so the process must start at the repository root");
  }
  std::string held;
  std::array<char, kReadBlockBytes> block;
  for (size_t read = std::fread(block.data(), 1, block.size(), file); read > 0;
       read = std::fread(block.data(), 1, block.size(), file)) {
    held.append(block.data(), read);
  }
  const bool failed = std::ferror(file) != 0;
  std::fclose(file);
  if (failed) { return std::unexpected("reading shader artifact " + path + " failed"); }
  if (held.empty()) {
    return std::unexpected("the shader artifact " + path +
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
