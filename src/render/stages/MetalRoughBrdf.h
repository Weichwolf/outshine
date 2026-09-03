#ifndef OUTSHINE_RENDER_STAGES_METALROUGHBRDF_H
#define OUTSHINE_RENDER_STAGES_METALROUGHBRDF_H

#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include "math/Units.h"

namespace outshine::Render {

[[nodiscard]] inline double BrdfVisibility(double nl, double nv, double a2) {
  return 0.5 /
         (nl * std::sqrt(nv * nv * (1.0 - a2) + a2) + nv * std::sqrt(nl * nl * (1.0 - a2) + a2));
}

[[nodiscard]] inline std::string MetalRoughBrdfMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/metalRoughBrdf.msl", body, error)) { return {}; }
  std::array<char, 256> constants{};
  std::snprintf(constants.data(), constants.size(), "constant float kPi = %.17g;\n", kPi);
  return std::string(constants.data()) + body;
}

[[nodiscard]] inline std::string MetalRoughBrdfMsl() {
  std::string ignored;
  return MetalRoughBrdfMsl(ignored);
}

} // namespace outshine::Render
#endif
