#include "FBHudGeometry.h"
#include "FBHudFont.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace FlightBox {

namespace {
/* Half of the 1px box-filter band FBHudStage's fragment shader ramps coverage over -- see its banner.
 * A stroke's rasterised half-extent is hw + kLineFeather, so alpha == 0 exactly at the quad edge. */
constexpr float kLineFeather = 0.5f;
} // namespace

FBHudGeometry::FBHudGeometry() {
  StrokeV.reserve(kHudMaxStrokeFloats);
  TextV.reserve(kHudMaxTextFloats);
}

void FBHudGeometry::Reset() {
  StrokeV.clear();
  TextV.clear();
}

/* Emits one stroke's 2 triangles (6 verts) as a quad expanded hw+kLineFeather to each side of the
 * centreline; d is that vertex's signed perpendicular distance, interpolated exactly across the quad
 * since it's an affine function of position on a rectangle. Caps are BUTT, at the segment's own
 * endpoints (not feathered/extended) -- deliberately: HUD strokes are short (ticks/rails/box edges/
 * horizon-bar halves), the visible aliasing this whole change targets is the SIDE edge running along a
 * tilted stroke's length (the horizon bar, the waterline chevron, tick diagonals at extreme roll), not
 * the short end cut across it; adding a second longitudinal distance channel to feather that cut too
 * would double the shader/vertex-format complexity for an edge that's already just 1px of hard cut
 * regardless of angle. */
static void AppendStroke(std::vector<float> &out, float x0, float y0, float x1, float y1, float hw,
                          float r, float g, float b) {
  float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
  if (len < 1e-4f) return;
  float ext = hw + kLineFeather;
  float nx = -dy / len * ext, ny = dx / len * ext;
  auto vert = [&](float px, float py, float d) { out.insert(out.end(), {px, py, d, hw, r, g, b}); };
  vert(x0 + nx, y0 + ny, ext);
  vert(x1 + nx, y1 + ny, ext);
  vert(x1 - nx, y1 - ny, -ext);
  vert(x0 + nx, y0 + ny, ext);
  vert(x1 - nx, y1 - ny, -ext);
  vert(x0 - nx, y0 - ny, -ext);
}

void FBHudGeometry::Line(float x0, float y0, float x1, float y1, float r, float g, float b) {
  AppendStroke(StrokeV, x0, y0, x1, y1, 0.5f, r, g, b);
}

void FBHudGeometry::QLine(float x0, float y0, float x1, float y1, float hw, float r, float g, float b) {
  AppendStroke(StrokeV, x0, y0, x1, y1, hw, r, g, b);
}

void FBHudGeometry::Circle(float cx, float cy, float radius, int segments, float r, float g, float b) {
  float px = cx + radius, py = cy;
  for (int i = 1; i <= segments; i++) {
    float a = (float)i / (float)segments * 6.2831853f;
    float x = cx + radius * cosf(a), y = cy + radius * sinf(a);
    Line(px, py, x, y, r, g, b);
    px = x;
    py = y;
  }
}

void FBHudGeometry::Box(float x0, float y0, float x1, float y1, float r, float g, float b) {
  Line(x0, y0, x1, y0, r, g, b);
  Line(x1, y0, x1, y1, r, g, b);
  Line(x1, y1, x0, y1, r, g, b);
  Line(x0, y1, x0, y0, r, g, b);
}

void FBHudGeometry::Text(float x, float y, float s, float r, float g, float b, const char *text) {
  FBHudFontAppendText(TextV, x, y, s, r, g, b, text);
}

void FBHudGeometry::Printf(float x, float y, float s, float r, float g, float b, const char *fmt, ...) {
  char buf[96];
  va_list a;
  va_start(a, fmt);
  vsnprintf(buf, sizeof buf, fmt, a);
  va_end(a);
  Text(x, y, s, r, g, b, buf);
}

} // namespace FlightBox
