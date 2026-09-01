#ifndef OUTSHINE_RENDER_STAGES_SHADERPRELUDE_H
#define OUTSHINE_RENDER_STAGES_SHADERPRELUDE_H

#include <cstdio>
#include <numbers>
#include <string>

#include "ShaderFile.h"

namespace outshine::Render {

[[nodiscard]] inline std::string MslPrelude(std::string &error) {
  std::string opening;
  if (!LoadShaderText("src/render/shaders/prelude.msl", opening, error)) { return {}; }
  char pi[48];
  std::snprintf(pi, sizeof pi, "#define OUTSHINE_PI %.17g\n", std::numbers::pi);
  return opening + pi;
}

} // namespace outshine::Render
#endif
