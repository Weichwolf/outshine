#include "FBHudGeometry.h"
#include "FBHudFont.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace FlightBox {

namespace {
/* Half of the 1 px band the fragment shader ramps coverage over, so alpha hits 0 exactly at the
 * quad edge. */
constexpr float kLineFeather = 0.5f;

/* Liang-Barsky: p<0 entering, p>0 leaving, p==0 parallel (reject iff already outside that boundary).
 * false = wholly outside, and the caller emits nothing. */
bool ClipSegment(float &x0, float &y0, float &x1, float &y1, float cx0, float cy0, float cx1, float cy1) {
  float dx = x1 - x0, dy = y1 - y0, t0 = 0.f, t1 = 1.f;
  float p[4] = {-dx, dx, -dy, dy};
  float q[4] = {x0 - cx0, cx1 - x0, y0 - cy0, cy1 - y0};
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0.f) {
      if (q[i] < 0.f) return false;
      continue;
    }
    float r = q[i] / p[i];
    if (p[i] < 0.f) { if (r > t1) return false; if (r > t0) t0 = r; }
    else { if (r < t0) return false; if (r < t1) t1 = r; }
  }
  float nx0 = x0 + t0 * dx, ny0 = y0 + t0 * dy, nx1 = x0 + t1 * dx, ny1 = y0 + t1 * dy;
  x0 = nx0; y0 = ny0; x1 = nx1; y1 = ny1;
  return true;
}
} // namespace

void FBHudGeometry::SetClip(float x0, float y0, float x1, float y1) {
  ClipOn = true;
  ClipX0 = x0; ClipY0 = y0; ClipX1 = x1; ClipY1 = y1;
}

void FBHudGeometry::ClearClip() { ClipOn = false; }

FBHudGeometry::FBHudGeometry() {
  StrokeV.reserve(kHudMaxStrokeFloats);
  TextV.reserve(kHudMaxTextFloats);
}

void FBHudGeometry::Reset() {
  StrokeV.clear();
  TextV.clear();
}

/* d interpolates exactly across the quad because it is an affine function of position on a rectangle.
 * Caps are BUTT, deliberately: the aliasing worth removing is the SIDE edge running along a tilted
 * stroke's LENGTH (the horizon bar, the waterline chevron), not the 1 px end cut across it — and
 * feathering that too would need a second distance channel through shader and vertex format. */
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
  if (ClipOn && !ClipSegment(x0, y0, x1, y1, ClipX0, ClipY0, ClipX1, ClipY1)) return;
  AppendStroke(StrokeV, x0, y0, x1, y1, 0.5f, r, g, b);
}

void FBHudGeometry::QLine(float x0, float y0, float x1, float y1, float hw, float r, float g, float b) {
  if (ClipOn && !ClipSegment(x0, y0, x1, y1, ClipX0, ClipY0, ClipX1, ClipY1)) return;
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

/* All-or-nothing under an aperture clip: a glyph is an opaque quad with no sub-character geometry.
 * x still advances, so a string straddling the aperture edge stays correctly spaced. */
void FBHudGeometry::Text(float x, float y, float s, float r, float g, float b, const char *text) {
  if (!ClipOn) { FBHudFontAppendText(TextV, x, y, s, r, g, b, text); return; }
  float adv = kFontAdvance * s, qs = kFontQuadSize * s;
  for (; *text; text++) {
    char u = *text;
    if (u >= 'a' && u <= 'z') u -= 32;
    const char *p = strchr(kFontCharset, u);
    int gi = p ? (int)(p - kFontCharset) : 0;
    if (gi > 0 && x + qs >= ClipX0 && x <= ClipX1 && y + qs >= ClipY0 && y <= ClipY1)
      FBHudFontAppendGlyph(TextV, x, y, qs, gi, r, g, b);
    x += adv;
  }
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
