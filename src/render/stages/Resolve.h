#ifndef OUTSHINE_RENDER_STAGES_RESOLVE_H
#define OUTSHINE_RENDER_STAGES_RESOLVE_H

#include <string>

#include "Compiled.h"

namespace outshine::Render {

struct DisplayOptions {
  float Exposure = 1.0f;
  Transfer Curve = Transfer::Filmic;

  bool Temporal = false;
};

inline std::string DisplayMsl(const DisplayOptions &options) {
  std::string source;
  if (options.Curve == Transfer::Filmic) {
    source += "static inline float3 filmic(float3 x) {\n"
              "  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),\n"
              "               float3(0.0), float3(1.0));\n"
              "}\n";
  }

  source += "static inline float covered(float sceneDepth) {\n"
            "  return select(0.0, 1.0, sceneDepth > 0.0);\n"
            "}\n";
  if (options.Temporal) {
    source += "constant float kCurrentWeight = 0.1;\n"
              "static inline float3 rgbToYCoCg(float3 c) {\n"
              "  return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b,\n"
              "                -0.25 * c.r + 0.5 * c.g - 0.25 * c.b);\n"
              "}\n"
              "static inline float3 yCoCgToRgb(float3 c) {\n"
              "  float t = c.x - c.z;\n"
              "  return float3(t + c.y, c.x + c.z, t - c.y);\n"
              "}\n"
              "static inline float3 clipTowards(float3 history, float3 centre, float3 extent) {\n"
              "  float3 offset = history - centre;\n"
              "  float3 unit = abs(offset) / max(extent, float3(1.0e-5));\n"
              "  float largest = max(max(unit.x, unit.y), unit.z);\n"
              "  return largest > 1.0 ? centre + offset / largest : history;\n"
              "}\n";
  }

  source += "static inline float4 displayed(float4 scene, float sceneDepth) {\n"
            "  float3 lit = scene.rgb;\n"
            "  float a = max(covered(sceneDepth), saturate(scene.a));\n";
  source += "  float scale = " + std::to_string(options.Exposure) + ";\n";
  source += options.Curve == Transfer::Filmic ? "  return float4(filmic(lit * scale), a);\n"
                                              : "  return float4(lit * scale, a);\n";
  source += "}\n";
  return source;
}

} // namespace outshine::Render
#endif
