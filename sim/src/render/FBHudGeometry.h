/* FlightBox — FBHudGeometry: the HUD's per-frame 2D geometry buffer (pixel coords), the WebGPU
 * backend's INPUT contract. A display system (systems/FBDisplaySystem) fills one of these each frame
 * via BuildHud(); FBHudStage uploads the vertex streams verbatim and draws them. Replaces the
 * old GL-shim globals (w3_hud/w3_hudT/mx_v) with an owned, reusable buffer -- no fake GL, no statics.
 *
 * Two primitive kinds, matching FBHudStage's two pipelines:
 *   Strokes (x,y,d,hw,r,g,b) x6/segment -- ONE analytic-coverage AA quad path for every straight
 *           segment (rails, ticks, carets, boxes, the conformal horizon) -- Line() and QLine() both
 *           emit this, only their default half-width differs (a hairline vs. an explicit width); no
 *           more hard LineList aliasing. d = the vertex's signed perpendicular distance from the
 *           segment's centreline in pixels, hw = the segment's nominal half-width; FBHudStage's
 *           fragment shader turns (d, hw) into a box-filter coverage value (see its banner). Caps are
 *           plain butt caps (the quad's own length, not extended/feathered) -- see FBHudGeometry.cpp's
 *           AppendStroke for why that trade-off is fine here.
 *   Glyphs  (x,y,u,v,r,g,b) x6/char -- generic bitmap-font tile-atlas blits (FBHudFont.h)
 * Vectors, not fixed arrays: Reset() clears without releasing capacity, so after the first frame's
 * geometry settles (bounded, deterministic symbology) nothing reallocates. */
#ifndef FBHUDGEOMETRY_H
#define FBHUDGEOMETRY_H

#include <vector>

namespace FlightBox {

/* Upper bounds FBHudStage sizes its GPU buffers to (generous headroom over one frame's symbology;
 * kHudMaxStrokeFloats absorbs what used to be two separate line/tri budgets). */
inline constexpr size_t kHudMaxStrokeFloats = 147456;
inline constexpr size_t kHudMaxTextFloats = 32768;

class FBHudGeometry {
public:
  FBHudGeometry();

  void Reset();

  /* Scissor state for conformal symbology (the HUD combiner aperture): while active, every stroke is
   * cut to the rect (Liang-Barsky segment clip -- partial strokes emit only their visible portion, a
   * fully-outside stroke emits nothing) and every glyph is dropped whole unless its content box
   * overlaps the rect (glyphs are opaque quads, not further sub-divisible without a shader change --
   * see the class banner and FBF16Hud's aperture clip). SetClip/ClearClip bracket a BuildHud section;
   * geometry emitted outside any SetClip/ClearClip pair (tapes, text blocks -- already laid out inside
   * the aperture) is unaffected. */
  void SetClip(float x0, float y0, float x1, float y1);
  void ClearClip();

  /* A hairline (half-width 0.5px -- the AA band alone is the line's visible width, matching the old
   * 1px hard LineList's weight) straight segment. */
  void Line(float x0, float y0, float x1, float y1, float r, float g, float b);
  /* A stroke of explicit half-width hw -- the conformal horizon's thicker bar. */
  void QLine(float x0, float y0, float x1, float y1, float halfWidth, float r, float g, float b);
  /* Approximate a circle as `segments` chords, each an AA stroke. */
  void Circle(float cx, float cy, float radius, int segments, float r, float g, float b);
  /* A rectangle outline -- the boxed current value on a tape. */
  void Box(float x0, float y0, float x1, float y1, float r, float g, float b);
  void Text(float x, float y, float s, float r, float g, float b, const char *text);
  void Printf(float x, float y, float s, float r, float g, float b, const char *fmt, ...);

  const std::vector<float> &Strokes() const { return StrokeV; }
  const std::vector<float> &Glyphs() const { return TextV; }

private:
  std::vector<float> StrokeV, TextV;
  bool ClipOn = false;
  float ClipX0 = 0, ClipY0 = 0, ClipX1 = 0, ClipY1 = 0;
};

} // namespace FlightBox
#endif
