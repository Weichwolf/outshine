#include "ShaderFile.h"

#include <array>
#include <cstdio>
#include <numbers>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Render {

namespace {

constexpr size_t kReadBlockBytes = 1u << 14u;
constexpr size_t kPreludeDefineBytes = 48;

bool LoadShaderText(std::string_view treePath, std::string &into, std::string &error) {
  const std::string path(treePath);
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    error = "the shader source " + path +
            " is not readable from here -- the engine reads its shaders from the tree, so the "
            "process must start at the repository root";
    return false;
  }
  into.clear();
  std::array<char, kReadBlockBytes> block;
  for (size_t read = std::fread(block.data(), 1, block.size(), file); read > 0;
       read = std::fread(block.data(), 1, block.size(), file)) {
    into.append(block.data(), read);
  }
  std::fclose(file);
  if (into.empty()) {
    error = "the shader source " + path + " is empty, and an empty kernel is a picture refusal";
    return false;
  }
  return true;
}

} // namespace

ShaderText &ShaderText::Reads(std::string_view treePath) {
  if (!Why_.empty()) { return *this; }
  std::string read;
  if (!LoadShaderText(treePath, read, Why_)) { return *this; }
  Held_ += read;
  return *this;
}

ShaderText &ShaderText::Adds(std::string_view text) {
  if (Why_.empty()) { Held_ += text; }
  return *this;
}

ShaderText &ShaderText::Begins() {
  if (!Reads("src/render/shaders/prelude.msl")) { return *this; }
  std::array<char, kPreludeDefineBytes> pi{};
  std::snprintf(pi.data(), pi.size(), "#define OUTSHINE_PI %.17g\n", std::numbers::pi);
  Held_ += pi.data();
  return *this;
}

std::string ShaderText::Take() {
  return Why_.empty() ? std::move(Held_) : std::string{};
}

std::string ShaderText::Take(std::string &error) {
  if (!Why_.empty()) {
    error = Why_;
    return {};
  }
  return std::move(Held_);
}

} // namespace outshine::Render
