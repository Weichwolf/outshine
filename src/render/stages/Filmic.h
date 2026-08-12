/* THE DISPLAY CURVE, as WGSL, in one place — the exposure places middle grey and this shapes what is
 * above and below it. Two consumers: ExposureStage needs the input the curve calls middle grey,
 * TaaStage applies the curve. Requires nothing else in scope. */
#ifndef FILMIC_H
#define FILMIC_H

namespace outshine::Render {

static const char *kFilmicWGSL = R"(
/* Narkowicz' rational fit of the ACES RRT + sRGB ODT (SIGGRAPH 2015 course notes). Scene-referred in,
 * display-LINEAR out; the sRGB surface format does the encode. PER CHANNEL and not on luminance,
 * because the shoulder desaturating a channel that runs past white IS what a photograph of a clear
 * sky does — its blue saturates while red and green climb, and a curve applied to luminance alone
 * keeps the ratio and clips the channel square instead. */
fn filmic(x : vec3f) -> vec3f {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), vec3f(0.0), vec3f(1.0));
}
/* The input this curve answers 0.18 to: the positive root of 2.0726 x^2 - 0.0762 x - 0.0252 = 0,
 * i.e. filmic(x) = 0.18 solved exactly. The exposure carries the whole anchor, so the curve has no
 * free parameter left and no scene can bend it. */
const kFilmicMid : f32 = 0.13017527;
/* Where the curve reaches display white, from the same fit: filmic(x) = 1 at x^2 - 7x - 1.75 = 0,
 * x = 7.2417 = 5.798 stops over middle grey. The frame's own latitude above the key, and the number
 * that decides whether anything in a picture can clip. */
const kFilmicWhite : f32 = 7.24166;
)";

} // namespace outshine::Render
#endif
