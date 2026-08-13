/* WHAT A DISPLAY FRAME IS, in one place and generated from the compiled plan. One stage emits it --
 * the tonemap -- and it holds no constant of the display chain of its own.
 *
 * THE PLAN DECIDES WHICH TERMS EXIST, and the terms it leaves out are not branches: a plan that
 * declares a linear transfer emits no curve at all rather than a curve with a flag around it. That
 * is why "the plan carries no dead path" is a property of the generated shader rather than a claim
 * about it.
 *
 * `Transfer::Linear` IS NOT A LOOK. It writes scene-referred radiance into an sRGB-encoding
 * attachment and does nothing else, which is what a numeric comparison against a path tracer needs:
 * a curve there would be measuring the curve (doc/requirements.md I.26.13).
 *
 * THE FRAME'S ALPHA IS COVERAGE, STRAIGHT AND NOT PREMULTIPLIED, and it is the reason this function
 * takes a depth sample at all. Without it a black subject and no subject are the same three
 * channels: MEASURED, the oracle's sphere carries 46 101 of 46 151 covered pixels at exactly 0.0
 * RGB, and only alpha tells them from the background.
 *
 * IT IS ALSO THE CHANNEL BLENDING NEEDS. glTF makes alpha modes first-class, so a renderer that
 * cannot emit coverage cannot implement `BLEND`. */
#ifndef RESOLVE_H
#define RESOLVE_H

#include <string>

#include "RenderPlan.h"

namespace outshine::Render {

struct DisplayOptions {
  float Exposure = 1.0f;   /* the scale the transfer multiplies scene radiance by */
  Transfer Curve = Transfer::Filmic;
};

/* Emits `float4 displayed(float4 scene, float sceneDepth)`. The caller hands the scene texel and
 * the depth attachment's own sample for the same fragment. */
inline std::string DisplayMsl(const DisplayOptions &options) {
  std::string source;
  if (options.Curve == Transfer::Filmic) {
    /* Narkowicz' rational fit of the ACES RRT + sRGB ODT (SIGGRAPH 2015 course notes).
     * Scene-referred in, display-LINEAR out; the sRGB surface format does the encode. PER CHANNEL
     * and not on luminance, because the shoulder desaturating a channel that runs past white IS what
     * a photograph of a clear sky does -- its blue saturates while red and green climb, and a curve
     * applied to luminance alone keeps the ratio and clips the channel square instead. */
    source += "static inline float3 filmic(float3 x) {\n"
              "  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14),\n"
              "               float3(0.0), float3(1.0));\n"
              "}\n";
  }
  /* Reversed-Z on a target cleared to 0 at the far plane: anything that wrote depth is strictly
   * greater, and that is the whole of the coverage predicate. */
  source += "static inline float covered(float sceneDepth) {\n"
            "  return select(0.0, 1.0, sceneDepth > 0.0);\n"
            "}\n";
  source += "static inline float4 displayed(float4 scene, float sceneDepth) {\n"
            "  float3 lit = scene.rgb;\n"
            "  float a = covered(sceneDepth);\n";
  source += "  float scale = " + std::to_string(options.Exposure) + ";\n";
  source += options.Curve == Transfer::Filmic ? "  return float4(filmic(lit * scale), a);\n"
                                              : "  return float4(lit * scale, a);\n";
  source += "}\n";
  return source;
}

} // namespace outshine::Render
#endif
