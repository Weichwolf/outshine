#include "FBHudGeometry.h"
#include "FBHudFont.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace FlightBox {

FBHudGeometry::FBHudGeometry() {
  LineV.reserve(kHudMaxLineFloats);
  TriV.reserve(kHudMaxTriFloats);
  TextV.reserve(kHudMaxTextFloats);
}

void FBHudGeometry::Reset() {
  LineV.clear();
  TriV.clear();
  TextV.clear();
}

void FBHudGeometry::Line(float x0, float y0, float x1, float y1, float r, float g, float b) {
  LineV.insert(LineV.end(), {x0, y0, r, g, b, x1, y1, r, g, b});
}

void FBHudGeometry::QLine(float x0, float y0, float x1, float y1, float hw, float r, float g, float b) {
  float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy);
  if (L < 1e-3f) return;
  float px = -dy / L * hw, py = dx / L * hw;
  float ax = x0 + px, ay = y0 + py, bx = x1 + px, by = y1 + py, cx2 = x1 - px, cy2 = y1 - py,
        dx2 = x0 - px, dy2 = y0 - py;
  TriV.insert(TriV.end(), {ax, ay, r, g, b, bx, by, r, g, b, cx2, cy2, r, g, b,
                           ax, ay, r, g, b, cx2, cy2, r, g, b, dx2, dy2, r, g, b});
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
