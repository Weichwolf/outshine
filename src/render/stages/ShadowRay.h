#ifndef OUTSHINE_RENDER_STAGES_SHADOWRAY_H
#define OUTSHINE_RENDER_STAGES_SHADOWRAY_H

#include <cstdio>
#include <string>

#include "ShaderFile.h"

#include "TriangleBvh.h"

namespace outshine::Render {

constexpr float kShadowRayNearFraction = 1.0f / 16384.0f;

[[nodiscard]] inline std::string ShadowRayMsl(std::string &error) {
  std::string body;
  if (!LoadShaderText("src/render/shaders/shadowRay.msl", body, error)) { return std::string(); }
  char constants[512];
  std::snprintf(constants, sizeof constants,
                "constant uint kBvhNoEscape = %uu;\n"
                "constant uint kBvhLeafFirstBits = %uu;\n"
                "constant uint kBvhLeafFirstMask = %uu;\n"
                "constant uint kBvhInterior = %uu;\n",
                kBvhNoEscape, kBvhLeafFirstBits, kBvhLeafFirstMask, kBvhInterior);
  return std::string(constants) + body;
}

[[nodiscard]] inline std::string ShadowRayMsl() {
  std::string ignored;
  return ShadowRayMsl(ignored);
}

}
#endif
