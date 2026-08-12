/* WHAT A DISPLAY FRAME IS, in one place and generated from the compiled plan. Two stages emit it --
 * the tonemap on its own, and the fused temporal resolve that carries it out of the same fragment --
 * so it is written once here and neither of them owns a constant of the display chain.
 *
 * THE PLAN DECIDES WHICH TERMS EXIST, and the terms it leaves out are not branches: a plan with no
 * occlusion stage emits no occlusion tap and binds no occlusion texture, and a plan with no meter
 * emits its own declared scalar. That is why "the plan carries no dead path" is a property of the
 * generated shader rather than a claim about it.
 *
 * `Transfer::Linear` IS NOT A LOOK. It writes scene-referred radiance into an sRGB-encoding
 * attachment and does nothing else, which is what a numeric comparison against a path tracer needs:
 * a curve there would be measuring the curve (doc/requirements.md I.26.13). */
#ifndef RESOLVE_H
#define RESOLVE_H

#include <string>

#include "Filmic.h"
#include "RenderPlan.h"

namespace outshine::Render {

struct DisplayOptions {
  bool HasOcclusion = false;   /* `aoTex` is bound and the direct fraction weights it */
  bool HasMeter = false;       /* `meter` is bound; otherwise `Exposure` below is the scale */
  float Exposure = 1.0f;
  Transfer Curve = Transfer::Filmic;
};

/* Emits `fn displayed(scene : vec4f, fragXY : vec2f) -> vec4f`. The caller declares `aoTex` and
 * `meter` at its own binding indices where the options say they exist. */
inline std::string DisplayWGSL(const DisplayOptions &options) {
  std::string source;
  if (options.HasOcclusion) {
    /* alpha = the DIRECT fraction of this pixel's radiance; occlusion darkens only the rest. The
     * occlusion target is half resolution and read at the matching texel. */
    source +=
        "fn occluded(scene : vec4f, fragXY : vec2f) -> vec3f {\n"
        "  let ao = textureLoad(aoTex, vec2i(fragXY * 0.5), 0).r;\n"
        "  return scene.rgb * mix(ao, 1.0, clamp(scene.a, 0.0, 1.0));\n"
        "}\n";
  } else {
    source += "fn occluded(scene : vec4f, fragXY : vec2f) -> vec3f { return scene.rgb; }\n";
  }
  source += "fn displayed(scene : vec4f, fragXY : vec2f) -> vec4f {\n"
            "  let lit = occluded(scene, fragXY);\n";
  source += options.HasMeter ? "  let scale = meter.expScale;\n"
                             : "  let scale = " + std::to_string(options.Exposure) + ";\n";
  source += options.Curve == Transfer::Filmic ? "  return vec4f(filmic(lit * scale), 1.0);\n"
                                              : "  return vec4f(lit * scale, 1.0);\n";
  source += "}\n";
  return source;
}

} // namespace outshine::Render
#endif
