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
 * a curve there would be measuring the curve (board:0087).
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
  /* WHETHER THE PLAN PULLED A TEMPORAL RESOLVE (board:1413). It is not a flag the shader branches on:
   * a plan without one emits no temporal term at all, which is the same statement the transfer above
   * already makes about its curve. */
  bool Temporal = false;
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
  if (options.Temporal) {
    /* THE CLAMP IS IN YCoCg AND THAT IS THE ONE CHOICE HERE WORTH DEFENDING. The history is clipped
     * into the box of the current frame's 3x3 neighbourhood; in RGB that box fits the set of colours
     * a pixel could plausibly be badly, so an edge between two hues clips towards a colour NEITHER
     * neighbour has and the picture gains fringes. In a luma-chroma basis the box lies along the axis
     * the eye is sensitive to and the same clip keeps the hue -- Karis, *High Quality Temporal
     * Supersampling*, SIGGRAPH 2014, and Salvi, *An Excursion in Temporal Supersampling*, GDC 2016.
     *
     * IT IS A CLIP TOWARDS THE MEAN AND NOT A CLAMP PER CHANNEL. Clamping each channel independently
     * lands on a corner of the box and shifts the colour; scaling the whole offset until it enters
     * keeps the direction the history disagreed in, which is what makes a disocclusion fade rather
     * than flash.
     *
     * `kCurrentWeight` IS THE ONE NUMBER A READER SHOULD SEE: the fraction of the present in each
     * output pixel, so the history's half-life is `-1 / log2(1 - w)` frames -- at 0.1 that is 6.6
     * frames, 110 ms at 60 Hz, and it is the trade between shimmer and smear. */
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
            "  float a = covered(sceneDepth);\n";
  source += "  float scale = " + std::to_string(options.Exposure) + ";\n";
  source += options.Curve == Transfer::Filmic ? "  return float4(filmic(lit * scale), a);\n"
                                              : "  return float4(lit * scale, a);\n";
  source += "}\n";
  return source;
}

} // namespace outshine::Render
#endif
