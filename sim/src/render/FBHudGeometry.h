/* FlightBox — FBHudGeometry: the HUD's per-frame 2D geometry buffer (pixel coords), the WebGPU
 * backend's INPUT contract. A display system (systems/FBDisplaySystem) fills one of these each frame
 * via BuildHud(); FBHudStage uploads the three vertex streams verbatim and draws them. Replaces the
 * old GL-shim globals (w3_hud/w3_hudT/mx_v) with an owned, reusable buffer -- no fake GL, no statics.
 *
 * Three primitive kinds, matching FBHudStage's three pipelines:
 *   Lines  (x,y,r,g,b) x2/segment  -- rails, ticks, carets, boxes (GL_LINE-equivalent LineList)
 *   Tris   (x,y,r,g,b) x6/segment  -- thin AA quads for lines that must stay smooth at any angle
 *          (the conformal horizon), built from two triangles per QLine call
 *   Glyphs (x,y,u,v,r,g,b) x6/char -- generic bitmap-font tile-atlas blits (FBHudFont.h)
 * Vectors, not fixed arrays: Reset() clears without releasing capacity, so after the first frame's
 * geometry settles (bounded, deterministic symbology) nothing reallocates. */
#ifndef FBHUDGEOMETRY_H
#define FBHUDGEOMETRY_H

#include <vector>

namespace FlightBox {

/* Upper bounds FBHudStage sizes its GPU buffers to (generous headroom over one frame's symbology). */
inline constexpr size_t kHudMaxLineFloats = 131072;
inline constexpr size_t kHudMaxTriFloats = 16384;
inline constexpr size_t kHudMaxTextFloats = 32768;

class FBHudGeometry {
public:
  FBHudGeometry();

  void Reset();

  void Line(float x0, float y0, float x1, float y1, float r, float g, float b);
  /* A thin filled quad along (x0,y0)-(x1,y1), half-width hw -> a smooth AA line at any angle. */
  void QLine(float x0, float y0, float x1, float y1, float halfWidth, float r, float g, float b);
  /* Approximate a circle as `segments` chords. */
  void Circle(float cx, float cy, float radius, int segments, float r, float g, float b);
  /* A rectangle outline -- the boxed current value on a tape. */
  void Box(float x0, float y0, float x1, float y1, float r, float g, float b);
  void Text(float x, float y, float s, float r, float g, float b, const char *text);
  void Printf(float x, float y, float s, float r, float g, float b, const char *fmt, ...);

  const std::vector<float> &Lines() const { return LineV; }
  const std::vector<float> &Tris() const { return TriV; }
  const std::vector<float> &Glyphs() const { return TextV; }

private:
  std::vector<float> LineV, TriV, TextV;
};

} // namespace FlightBox
#endif
