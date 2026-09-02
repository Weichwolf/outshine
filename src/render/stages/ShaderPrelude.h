#ifndef OUTSHINE_RENDER_STAGES_SHADERPRELUDE_H
#define OUTSHINE_RENDER_STAGES_SHADERPRELUDE_H

#include <array>
#include <cstdio>
#include <numbers>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

constexpr size_t kPreludeDefineBytes = 48;

[[nodiscard]] inline std::string MslPrelude(std::string &error) {
  std::string opening;
  if (!LoadShaderText("src/render/shaders/prelude.msl", opening, error)) { return {}; }
  std::array<char, kPreludeDefineBytes> pi{};
  std::snprintf(pi.data(), pi.size(), "#define OUTSHINE_PI %.17g\n", std::numbers::pi);
  return opening + pi.data();
}

} // namespace outshine::Render
#endif
