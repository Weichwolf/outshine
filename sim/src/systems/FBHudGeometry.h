/* The HUD's reused per-frame 2D geometry buffer in PIXEL coordinates — the WebGPU backend's input
 * contract. A display system fills one per frame via BuildHud(); FBHudStage uploads it verbatim.
 * Two primitive kinds matching the two pipelines: Strokes (x,y,d,hw,r,g,b) and Glyphs (x,y,u,v,r,g,b),
 * six vertices each. Vectors, not fixed arrays: Reset() keeps the capacity, so after the first frame
 * the bounded, deterministic symbology reallocates nothing.
 * Layouts, Deckungsformel und Clipping: doc/flightbox/rendering.md, Abschnitt 7.1. */
#ifndef FBHUDGEOMETRY_H
#define FBHUDGEOMETRY_H

#include <vector>

namespace FlightBox::Systems {

/* The bounds FBHudStage sizes its GPU buffers to. */
inline constexpr size_t kHudMaxStrokeFloats = 147456;
inline constexpr size_t kHudMaxTextFloats = 32768;

class FBHudGeometry {
public:
  FBHudGeometry();

  void Reset();

  /* The combiner aperture: strokes are CUT to the rect (Liang-Barsky), glyphs dropped WHOLE — an
   * opaque quad is not sub-divisible without a shader change. Geometry outside a SetClip/ClearClip
   * pair is unaffected. */
  void SetClip(float x0, float y0, float x1, float y1);
  void ClearClip();

  /* A hairline: half-width 0.5 px, so the AA band alone is the visible width. */
  void Line(float x0, float y0, float x1, float y1, float r, float g, float b);
  /* A stroke of explicit half-width. */
  void QLine(float x0, float y0, float x1, float y1, float halfWidth, float r, float g, float b);
  /* `segments` chords, each an AA stroke. */
  void Circle(float cx, float cy, float radius, int segments, float r, float g, float b);
  /* A rectangle outline. */
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

} // namespace FlightBox::Systems
#endif
