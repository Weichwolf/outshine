#include "FBDeclaredHud.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FBCamera.h"
#include "FBHudFont.h"
#include "FBUnits.h"

namespace FlightBox::Systems {
namespace {

constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kR2D = 57.29577951308232f;

struct Proj { float sx, sy, zc; };   /* zc = cos(angle off boresight); <= 0 = behind */

/* EVERYTHING A ROW NEEDS THAT IS NOT IN THE ROW: the window, the two projectors and the two scales.
 * Built once per frame, so a deck of forty rows still resolves one camera basis. */
struct Frame {
  const FBState &S;
  const FBHudEnv &E;
  const FBHudDeck &D;
  float W = 0, H = 0;            /* the DRAWN window: full width, the windscreen's height */
  float cx = 0, cy = 0;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;   /* it, inset */
  float mg = 1;   /* the deck's pixel scale — symbols do not grow with the screen */
  float Kc = 0;   /* pixels per radian: the SCENE's projector, never a second one */
  float mr = 0;   /* pixels per milliradian, magnified to the combiner's apparent size */
  FBCameraBasis Cam{};

  /* A WORLD angle: through the same basis as the scene, so the symbol sits on the thing it means. */
  Proj World(float azDeg, float elDeg) const {
    float az = azDeg * kRad, el = elDeg * kRad;
    float d[3] = {cosf(el) * sinf(az), sinf(el), -cosf(el) * cosf(az)};
    float xc = d[0] * Cam.sr[0] + d[1] * Cam.sr[1] + d[2] * Cam.sr[2];
    float yc = d[0] * Cam.up[0] + d[1] * Cam.up[1] + d[2] * Cam.up[2];
    float zc = d[0] * Cam.f[0] + d[1] * Cam.f[1] + d[2] * Cam.f[2];
    float zcs = zc > 0.05f ? zc : 0.05f;
    return {cx + Kc * xc / zcs, cy - Kc * yc / zcs, zc};
  }
  /* A BODY angle: straight onto the glass. The screen IS the aircraft's system, so rolling it would
   * be wrong — a lead solution does not bank with the horizon. */
  Proj Body(float azDeg, float elDeg) const {
    return {cx + Kc * tanf(azDeg * kRad), cy - Kc * tanf(elDeg * kRad), 1.f};
  }
  bool Inside(const Proj &p) const {
    return p.zc >= 0.05f && p.sx >= x0 && p.sx <= x1 && p.sy >= y0 && p.sy <= y1;
  }
};

void DashedLine(FBHudGeometry &out, float x0, float y0, float x1, float y1, int n,
                float r, float g, float b) {
  if (n < 1) n = 1;
  for (int i = 0; i < n; i++) {
    float t0 = (float)i / (float)n, t1 = ((float)i + 0.5f) / (float)n;
    out.Line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, r, g, b);
  }
}

float TextWidth(const char *s, float scale) { return (float)std::strlen(s) * kFontAdvance * scale; }

void PutText(FBHudGeometry &out, const FBHudElement &e, float x, float y, float scale, const char *s) {
  float w = TextWidth(s, scale);
  if (e.Align == FBHudAlign::Centre) x -= 0.5f * w;
  else if (e.Align == FBHudAlign::Right) x -= w;
  out.Text(x, y - 0.5f * kFontQuadSize * scale, scale, e.R, e.G, e.B, s);
}

/* ---- the element kinds ----------------------------------------------------------------------
 * One function per kind, each reading ONLY the row and the frame. A kind that needed a second
 * FBState field not in FBHudDecl's vocabulary would be a kind that cannot be declared. */

void DrawText(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  const float s = e.Size * f.mg;
  char buf[96];
  const char *txt = buf;
  if (!e.Fmt.empty()) {
    double a = (double)FBHudNumber(e.Src, f.S, f.E), b = (double)FBHudNumber(e.Src2, f.S, f.E);
    snprintf(buf, sizeof buf, e.Fmt.c_str(), a, b);
  } else if (e.Str != FBHudStr::None) {
    txt = FBHudString(e.Str, f.S, f.E);
  } else {
    txt = e.Literal.c_str();
  }
  if (!txt || !*txt) return;
  PutText(out, e, x, y, s, txt);
}

void DrawCross(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  float g = e.Gap * f.mr, l = e.Len * f.mr;
  out.Line(x - l, y, x - g, y, e.R, e.G, e.B);
  out.Line(x + g, y, x + l, y, e.R, e.G, e.B);
  out.Line(x, y - l, x, y - g, e.R, e.G, e.B);
  out.Line(x, y + g, x, y + l, e.R, e.G, e.B);
}

void DrawBar(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  float w = e.W * f.mg, h = e.H * f.mg;
  float full = e.Span > 0.f ? e.Span : 100.f;
  float frac = FBHudNumber(e.Src, f.S, f.E) / full;
  frac = frac < 0.f ? 0.f : (frac > 1.f ? 1.f : frac);
  out.Box(x - 0.5f * w, y - 0.5f * h, x + 0.5f * w, y + 0.5f * h, e.R, e.G, e.B);
  if (h >= w) out.Fill(x - 0.5f * w, y + 0.5f * h - frac * h, x + 0.5f * w, y + 0.5f * h, e.R, e.G, e.B, 1.f);
  else out.Fill(x - 0.5f * w, y - 0.5f * h, x - 0.5f * w + frac * w, y + 0.5f * h, e.R, e.G, e.B, 1.f);
}

void DrawRose(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  float rad = 0.5f * e.W * f.mg;
  float hdg = FBHudNumber(FBHudNum::Heading, f.S, f.E);
  out.Circle(x, y, rad, 24, e.R, e.G, e.B);
  static const char *const kCard[4] = {"N", "E", "S", "W"};
  for (int i = 0; i < 4; i++) {
    float a = ((float)i * 90.f - hdg) * kRad;
    float lx = x + sinf(a) * rad * 1.18f, ly = y - cosf(a) * rad * 1.18f;
    PutText(out, e, lx, ly, e.Size * f.mg, kCard[i]);
  }
  if (e.Src != FBHudNum::None) {   /* the marked bearing — a waypoint dot on the ring */
    float a = FBHudNumber(e.Src, f.S, f.E) * kRad;
    out.Circle(x + sinf(a) * rad, y - cosf(a) * rad, 0.10f * rad, 8, e.R, e.G, e.B);
  }
  out.Circle(x, y, 0.09f * rad, 6, e.R, e.G, e.B);   /* the centre dot: own position */
}

void DrawVector(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  /* Comanche's Heading Velocity Display: where the aircraft will be in one second, drawn map-frame
   * (hud.md §2). Length is speed, so the scale is metres per second per scaled pixel. */
  float perMs = e.Span > 0.f ? e.Span : 1.f;
  float rel = (FBHudNumber(FBHudNum::Track, f.S, f.E) - FBHudNumber(FBHudNum::Heading, f.S, f.E)) * kRad;
  float len = FBHudNumber(FBHudNum::GsMs, f.S, f.E) / perMs * f.mg;
  float tipx = x + sinf(rel) * len, tipy = y - cosf(rel) * len;
  float d = 4.f * f.mg;
  out.Line(x - d, y, x + d, y, e.R, e.G, e.B);
  out.Line(x, y - d, x, y + d, e.R, e.G, e.B);
  out.Line(x, y, tipx, tipy, e.R, e.G, e.B);
  out.Circle(tipx, tipy, 0.6f * d, 10, e.R, e.G, e.B);
}

void DrawCompass(const Frame &f, const FBHudElement &e, float y, FBHudGeometry &out) {
  float hdg = FBHudNumber(e.Src != FBHudNum::None ? e.Src : FBHudNum::Heading, f.S, f.E);
  float span = e.Span > 0.f ? e.Span : 90.f;
  /* The scale walks in WHOLE units: a step under one would advance the loop by zero. */
  int step = e.Step >= 1.f ? (int)e.Step : 5;
  float half = e.W > 0.f ? 0.5f * e.W * f.mg : 0.5f * (f.x1 - f.x0);
  float ppd = half / (0.5f * span);
  int every = e.Count > 0 ? e.Count : 2;
  float tick = 5.f * f.mg;
  for (int h = (int)floorf((hdg - 0.5f * span) / (float)step) * step; h <= (int)(hdg + 0.5f * span); h += step) {
    float sx = f.cx + ((float)h - hdg) * ppd;
    if (sx < f.cx - half || sx > f.cx + half) continue;
    int n = ((h % 360) + 360) % 360;
    bool big = (n % (step * every)) == 0;
    out.Line(sx, y, sx, y - (big ? 2.f : 1.f) * tick, e.R, e.G, e.B);
    if (big) {
      char lb[8];
      snprintf(lb, sizeof lb, "%03d", n);
      PutText(out, e, sx, y - 2.f * tick - 8.f * f.mg, 1.1f * f.mg, lb);
    }
  }
  out.Line(f.cx - half, y, f.cx + half, y, e.R, e.G, e.B);
  char cur[8];
  snprintf(cur, sizeof cur, "%03d", ((int)(hdg + 0.5f) % 360 + 360) % 360);
  float bw = 15.f * f.mg, bh = 11.f * f.mg;
  out.Box(f.cx - bw, y + 2.f * f.mg, f.cx + bw, y + 2.f * f.mg + bh, e.R, e.G, e.B);
  PutText(out, e, f.cx, y + 2.f * f.mg + 0.5f * bh, 1.3f * f.mg, cur);
  /* THE STEERING CARET, and it becomes an off-band cue at the rail rather than vanishing
   * [mods/f22/doc/hud.md §2]. */
  if (e.Src2 != FBHudNum::None) {
    float rel = FBHudNumber(e.Src2, f.S, f.E);
    bool off = rel < -0.5f * span || rel > 0.5f * span;
    if (rel < -0.5f * span) rel = -0.5f * span;
    if (rel > 0.5f * span) rel = 0.5f * span;
    float sx = f.cx + rel * ppd, cw = 5.f * f.mg;
    out.Line(sx, y - 1.f, sx - cw, y - 2.2f * cw, e.R, e.G, e.B);
    out.Line(sx, y - 1.f, sx + cw, y - 2.2f * cw, e.R, e.G, e.B);
    if (off) out.Line(sx - cw, y - 2.2f * cw, sx + cw, y - 2.2f * cw, e.R, e.G, e.B);
  }
}

void DrawTape(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  float v = FBHudNumber(e.Src, f.S, f.E);
  float span = e.Span > 0.f ? e.Span : 200.f;
  int step = e.Step >= 1.f ? (int)e.Step : 10;   /* whole units, see DrawCompass */
  float halfH = 0.5f * (e.H > 0.f ? e.H * f.mg : 0.5f * f.H);
  float ppu = halfH / (0.5f * span);
  int every = e.Count > 0 ? e.Count : 2;
  bool right = e.Align == FBHudAlign::Right;
  float dir = right ? 1.f : -1.f, tick = 5.f * f.mg;
  for (int u = (int)floorf((v - 0.5f * span) / (float)step) * step; u <= (int)(v + 0.5f * span); u += step) {
    if (e.Gap > 0.f && (float)u < e.Gap) continue;   /* a scale with a floor (never a negative altitude) */
    float sy = y - ((float)u - v) * ppu;
    if (sy < y - halfH || sy > y + halfH) continue;
    bool big = (u % (step * every)) == 0;
    out.Line(x, sy, x + dir * (big ? 2.f : 1.f) * tick, sy, e.R, e.G, e.B);
    if (big) {
      char lb[12];
      snprintf(lb, sizeof lb, "%d", u);
      float lx = x + dir * (2.f * tick + 3.f * f.mg);
      FBHudElement le = e;
      le.Align = right ? FBHudAlign::Left : FBHudAlign::Right;
      PutText(out, le, lx, sy, 1.0f * f.mg, lb);
    }
  }
  out.Line(x, y - halfH, x, y + halfH, e.R, e.G, e.B);
  char cur[16];
  const char *cf = e.Fmt.empty() ? "%.0f" : e.Fmt.c_str();
  snprintf(cur, sizeof cur, cf, (double)v, 0.0);   /* the parser proved cf holds %f-class only */
  float bw = 28.f * f.mg, bh = 11.f * f.mg, bx = x - dir * bw;
  out.Box(bx < x ? bx : x, y - 0.5f * bh, bx < x ? x : bx, y + 0.5f * bh, e.R, e.G, e.B);
  FBHudElement ce = e;
  ce.Align = FBHudAlign::Centre;
  PutText(out, ce, 0.5f * (x + bx), y, 1.2f * f.mg, cur);
  if (!e.Literal.empty()) PutText(out, e, 0.5f * (x + bx), y - 0.5f * bh - 8.f * f.mg, 0.9f * f.mg, e.Literal.c_str());
}

void DrawLadder(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  int step = e.Step >= 1.f ? (int)e.Step : 5;   /* whole degrees, see DrawCompass */
  float span = e.Span > 0.f ? e.Span : 45.f;
  float halfW = 0.5f * (e.W > 0.f ? e.W * f.mg : 90.f * f.mg);
  float gap = 0.5f * (e.Gap > 0.f ? e.Gap * f.mg : 30.f * f.mg);
  float hdg = f.S.Platform.YawDeg;
  float dAz = halfW / f.Kc * kR2D;   /* the rung's half-length as an angle at this projector */
  float pitch = f.S.Platform.PitchDeg;
  int lo = (int)((pitch - span) / (float)step), hi = (int)((pitch + span) / (float)step);
  for (int k = lo; k <= hi; k++) {
    int deg = k * step;
    if (deg == 0) continue;   /* the zero rung is the horizon, and it is its own element */
    Proj a = f.World(hdg - dAz, (float)deg), b = f.World(hdg + dAz, (float)deg);
    if (a.zc < 0.05f || b.zc < 0.05f) continue;
    float tick = (deg > 0 ? 1.f : -1.f) * 8.f * f.mg;   /* the tips point AT the horizon */
    if (deg > 0) {
      out.Line(a.sx, a.sy, a.sx + gap, a.sy, e.R, e.G, e.B);
      out.Line(b.sx - gap, b.sy, b.sx, b.sy, e.R, e.G, e.B);
    } else {
      DashedLine(out, a.sx, a.sy, a.sx + gap, a.sy, 3, e.R, e.G, e.B);
      DashedLine(out, b.sx - gap, b.sy, b.sx, b.sy, 3, e.R, e.G, e.B);
    }
    out.Line(a.sx, a.sy, a.sx, a.sy + tick, e.R, e.G, e.B);
    out.Line(b.sx, b.sy, b.sx, b.sy + tick, e.R, e.G, e.B);
    char lb[8];
    snprintf(lb, sizeof lb, "%d", deg < 0 ? -deg : deg);
    FBHudElement le = e;
    le.Align = FBHudAlign::Right;
    PutText(out, le, a.sx - 4.f * f.mg, a.sy, 1.0f * f.mg, lb);
  }
}

void DrawHorizon(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  float half = e.W > 0.f ? 0.5f * e.W * f.mg : 86.f, gap = e.Gap > 0.f ? 0.5f * e.Gap * f.mg : 36.f;
  float alt = f.S.Platform.AltM > 1.f ? f.S.Platform.AltM : f.E.Agl;
  float dip = FBHorizonDipRad(alt) * kR2D;
  Proj a = f.World(f.S.Platform.YawDeg, -dip), b = f.World(f.S.Platform.YawDeg + 20.f, -dip);
  float dx = b.sx - a.sx, dy = b.sy - a.sy, L = sqrtf(dx * dx + dy * dy);
  if (L < 1.f) return;
  float ux = dx / L, uy = dy / L;
  out.QLine(a.sx - ux * half, a.sy - uy * half, a.sx - ux * gap, a.sy - uy * gap, 1.f, e.R, e.G, e.B);
  out.QLine(a.sx + ux * gap, a.sy + uy * gap, a.sx + ux * half, a.sy + uy * half, 1.f, e.R, e.G, e.B);
}

void DrawIls(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  /* Three lines, two vertical and one horizontal [mods/f22/doc/hud.md §5]. The deviations are read
   * against the active steerpoint, because no localiser is published on this bus. */
  float halfW = 0.5f * (e.W > 0.f ? e.W * f.mg : 70.f * f.mg);
  float halfH = 0.5f * (e.H > 0.f ? e.H * f.mg : 70.f * f.mg);
  float full = e.Span > 0.f ? e.Span : 5.f;
  auto clamp = [&](float d) { return d < -1.f ? -1.f : (d > 1.f ? 1.f : d); };
  float loc = clamp(FBHudNumber(FBHudNum::SteerRelBrg, f.S, f.E) / full);
  float yaw = clamp((FBHudNumber(FBHudNum::SteerRelBrg, f.S, f.E) -
                     (FBHudNumber(FBHudNum::Track, f.S, f.E) - FBHudNumber(FBHudNum::Heading, f.S, f.E))) / full);
  float gs = clamp((FBHudNumber(FBHudNum::SteerElDeg, f.S, f.E) + 3.0f) / full);
  out.Line(x - halfW, y - gs * halfH, x + halfW, y - gs * halfH, e.R, e.G, e.B);
  out.Line(x + loc * halfW, y - halfH, x + loc * halfW, y + halfH, e.R, e.G, e.B);
  DashedLine(out, x + yaw * halfW, y - halfH, x + yaw * halfW, y + halfH, 7, e.R, e.G, e.B);
}

void DrawScope(const Frame &f, const FBHudElement &e, float x, float y, FBHudGeometry &out) {
  float w = e.W * f.mg, h = e.H * f.mg;
  float x0 = x - 0.5f * w, y0 = y - 0.5f * h, x1 = x + 0.5f * w, y1 = y + 0.5f * h;
  float scaleNm = e.Span > 0.f ? e.Span : 25.f;
  out.Box(x0, y0, x1, y1, e.R, e.G, e.B);
  float ox = 0.5f * (x0 + x1), oy = y1 - 6.f * f.mg;
  out.Line(ox - 4.f * f.mg, oy + 2.f * f.mg, ox, oy - 5.f * f.mg, e.R, e.G, e.B);
  out.Line(ox, oy - 5.f * f.mg, ox + 4.f * f.mg, oy + 2.f * f.mg, e.R, e.G, e.B);
  if (!f.S.Radar.H.Readable()) return;
  const FBRadarBlock &r = f.S.Radar;
  float azHalf = r.ScanAzHalfDeg > 1.f ? r.ScanAzHalfDeg : 60.f;
  for (int i = 0; i < r.ContactCount && i < kMaxRadarContacts; i++) {
    const FBRadarContact &c = r.Contacts[i];
    float rn = c.RangeM * (float)kMToNm / scaleNm;
    if (rn > 1.f) continue;
    float px = ox + (c.AzDeg / azHalf) * 0.5f * (w - 8.f * f.mg);
    float py = oy - rn * (oy - y0 - 4.f * f.mg);
    if (px < x0 || px > x1) continue;
    float d = (i == r.LockIndex ? 4.f : 2.5f) * f.mg;
    out.Box(px - d, py - d, px + d, py + d, e.R, e.G, e.B);
    /* THE LEAD LINE: length proportional to closure, the Attack Display's own convention. */
    float lead = c.ClosureMs / 300.f * 10.f * f.mg;
    if (lead > 0.5f) out.Line(px, py - d, px, py - d - lead, e.R, e.G, e.B);
  }
}

void DrawFpm(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  if (!f.S.AirData.H.Readable()) return;
  Proj p = f.World(f.S.AirData.TrackDeg, f.S.AirData.FpaDeg);
  float rad = 0.5f * e.Size * f.mr, wing = e.Len * f.mr, tail = e.Size2 * f.mr;
  out.Circle(p.sx, p.sy, rad, 16, e.R, e.G, e.B);
  out.Line(p.sx - rad - wing, p.sy, p.sx - rad, p.sy, e.R, e.G, e.B);
  out.Line(p.sx + rad, p.sy, p.sx + rad + wing, p.sy, e.R, e.G, e.B);
  out.Line(p.sx, p.sy - rad, p.sx, p.sy - rad - tail, e.R, e.G, e.B);
}

void DrawContacts(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  if (!f.S.Radar.H.Readable()) return;
  const FBRadarBlock &r = f.S.Radar;
  for (int i = 0; i < r.ContactCount && i < kMaxRadarContacts; i++) {
    const FBRadarContact &c = r.Contacts[i];
    bool locked = i == r.LockIndex;
    Proj p = f.World(c.BearingDeg, c.ElevAngleDeg);
    float half = 0.5f * (locked ? e.Size : e.Size2) * f.mr;
    if (f.Inside(p)) {
      out.Box(p.sx - half, p.sy - half, p.sx + half, p.sy + half, e.R, e.G, e.B);
      /* The ONE identity carrier in this tree is the IFF reply; "no answer" stays unlabelled. */
      if (locked && c.Iff == FBIffReply::Friendly)
        out.Text(p.sx - 4.f * f.mg, p.sy - half - 6.f * f.mg, 1.08f * f.mg, e.R, e.G, e.B, "F");
      if (locked && !e.Fmt.empty())
        out.Printf(p.sx + half + 6.f * f.mg, p.sy + 4.f * f.mg, 1.08f * f.mg, e.R, e.G, e.B,
                   e.Fmt.c_str(), (double)(c.RangeM * (float)kMToNm), 0.0);
      continue;
    }
    if (e.Len <= 0.f) continue;
    /* OFF THE GLASS: the target locator line, not a box clamped to the rail. Its direction is a BODY
     * angle — a contact behind the nose has no projection but very much has a direction.
     * `gap` > 0 declares the OTHER convention instead: a small steering cue at the end of that
     * direction, which is what the F-22's manual describes (mods/f22/doc/hud.md §2). */
    float ang = atan2f(c.AzDeg, c.ElDeg);
    float ux = sinf(ang), uy = -cosf(ang), L = e.Len * f.mr;
    if (e.Gap > 0.f) {
      out.Circle(f.cx + ux * L, f.cy + uy * L, 0.5f * e.Gap * f.mr, 12, e.R, e.G, e.B);
      continue;
    }
    DashedLine(out, f.cx, f.cy, f.cx + ux * L, f.cy + uy * L, 5, e.R, e.G, e.B);
    float off = sqrtf(c.AzDeg * c.AzDeg + c.ElDeg * c.ElDeg);
    if (off > 99.f) off = 99.f;
    out.Printf(f.cx + ux * L * 1.15f - 8.f * f.mg, f.cy + uy * L * 1.15f, 1.08f * f.mg, e.R, e.G, e.B,
               "F%02d", (int)(off + 0.5f));
  }
}

void DrawCcip(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  const FBFireControlBlock &fc = f.S.FireControl;
  if (!f.S.AirData.H.Readable()) return;
  float dropM = f.S.Platform.AltM - fc.AgImpactElevM;
  float rngM = fc.AgRangeM > 1.f ? fc.AgRangeM : 1.f;
  Proj fpm = f.World(f.S.AirData.TrackDeg, f.S.AirData.FpaDeg);
  Proj pip = f.World(f.S.AirData.TrackDeg, -atan2f(dropM, rngM) * kR2D);
  DashedLine(out, fpm.sx, fpm.sy, pip.sx, pip.sy, e.Count > 0 ? e.Count : 9, e.R, e.G, e.B);
  out.Circle(pip.sx, pip.sy, 0.5f * e.Size * f.mr, 16, e.R, e.G, e.B);
  out.Circle(pip.sx, pip.sy, 0.5f * f.mr, 6, e.R, e.G, e.B);
  if (fc.AgInRange && e.Size2 > 0.f) out.Circle(pip.sx, pip.sy, 0.5f * e.Size2 * f.mr, 20, e.R, e.G, e.B);
  /* The release cue as a shrinking bar: the only number in this mode that demands an ACTION. */
  float window = e.Span > 0.f ? e.Span : 5.f;
  if (fc.AgTimeToReleaseS > 0.f && fc.AgTimeToReleaseS < window) {
    float h = 26.f * f.mg, frac = fc.AgTimeToReleaseS / window;
    float bx = pip.sx + 15.f * f.mg, by0 = pip.sy - 0.5f * h, by1 = pip.sy + 0.5f * h;
    out.Line(bx, by0, bx, by1, e.R, e.G, e.B);
    out.QLine(bx, by1 - h * frac, bx, by1, 2.f, e.R, e.G, e.B);
  }
}

void DrawFunnel(const Frame &f, const FBHudElement &e, FBHudGeometry &out) {
  const FBFireControlBlock &fc = f.S.FireControl;
  Proj lead = f.Body(fc.GunLeadAzDeg, fc.GunLeadElDeg);
  float topPx = 0.5f * fc.GunFunnelTopMr * 1e-3f * f.Kc;
  float botPx = 0.5f * fc.GunFunnelBottomMr * 1e-3f * f.Kc;
  float span = (e.H > 0.f ? e.H : 46.f) * f.mg;
  for (int s = -1; s <= 1; s += 2) {
    float ax = lead.sx + (float)s * botPx, ay = lead.sy + 0.5f * span;
    float bx = lead.sx + (float)s * topPx, by = lead.sy - 0.5f * span;
    out.Line(ax, ay, bx, by, e.R, e.G, e.B);
  }
  /* The target's span as a mark ON the walls: between them, the range fits. That IS GunInRange. */
  float sp = 0.5f * fc.GunSpanMr * 1e-3f * f.Kc;
  if (fc.GunInRange) {
    float den = botPx - topPx;
    float t = den > 1e-3f ? (botPx - sp) / den : 0.f;
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    float my = lead.sy + 0.5f * span - t * span;
    out.Line(lead.sx - sp, my, lead.sx + sp, my, e.R, e.G, e.B);
  }
  out.Circle(lead.sx, lead.sy, 0.5f * e.Size * f.mr, 12, e.R, e.G, e.B);
  if (fc.GunInFunnel) out.Circle(lead.sx, lead.sy, 0.25f * e.Size * f.mr, 10, e.R, e.G, e.B);
}

void DrawDlz(const Frame &f, const FBHudElement &e, float x, FBHudGeometry &out) {
  const FBFireControlBlock &fc = f.S.FireControl;
  float half = 0.5f * (e.H > 0.f ? e.H : 116.f) * f.mg, tw = (e.W > 0.f ? e.W : 10.f) * f.mg;
  float y0 = f.cy + half, y1 = f.cy - half;
  float top = fc.RaeroM > 1.f ? fc.RaeroM : 1.f;
  auto Y = [&](float m) {
    float t = m / top;
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    return y0 + (y1 - y0) * t;
  };
  out.Line(x, y0, x, y1, e.R, e.G, e.B);
  out.Line(x - 0.5f * tw, y1, x + 0.5f * tw, y1, e.R, e.G, e.B);
  out.Line(x - 0.5f * tw, Y(fc.RminM), x + 0.5f * tw, Y(fc.RminM), e.R, e.G, e.B);
  DashedLine(out, x - 0.4f * tw, Y(fc.RtrM), x + 0.4f * tw, Y(fc.RtrM), 3, e.R, e.G, e.B);
  float yr = Y(fc.TargetRangeM);
  out.Line(x - 0.9f * tw, yr, x, yr - 0.4f * tw, e.R, e.G, e.B);
  out.Line(x, yr - 0.4f * tw, x, yr + 0.4f * tw, e.R, e.G, e.B);
  out.Line(x, yr + 0.4f * tw, x - 0.9f * tw, yr, e.R, e.G, e.B);
  if (!e.Fmt.empty())
    out.Printf(x + 0.8f * tw, yr + 0.4f * tw, 1.08f * f.mg, e.R, e.G, e.B, e.Fmt.c_str(),
               (double)(fc.TargetRangeM * (float)kMToNm), 0.0);
  if (fc.TimeToImpactS > 0.f)
    out.Printf(x - 0.6f * tw, y0 + 14.f * f.mg, 1.08f * f.mg, e.R, e.G, e.B, "T%3.0f",
               (double)fc.TimeToImpactS);
}

} // namespace

void FBDeclaredHud::BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const {
  out.Reset();

  static const float kEyeOrigin[3] = {0, 0, 0};
  Frame f{state, env, Deck_};
  f.Cam = FBCameraBasisFrom(state.Platform.YawDeg, state.Platform.PitchDeg, state.Platform.RollDeg,
                            kEyeOrigin, kSceneVerticalFovDeg,
                            (float)env.Width / (float)env.Height, 1.f, 1000.f);
  f.W = (float)env.Width;
  f.H = (float)env.ViewH;
  f.mg = Deck_.Scale;
  f.cx = 0.5f * f.W;
  f.cy = 0.5f * f.H;
  f.x0 = Deck_.InsetPx;
  f.y0 = Deck_.InsetPx;
  f.x1 = f.W - Deck_.InsetPx;
  f.y1 = f.H - Deck_.InsetPx;
  /* The projector is the SCENE's — the world is not stretched to fit a HUD. It hangs on the full
   * frame height because the windscreen is a CROP of that projection, not a squeeze of it. */
  f.Kc = ((float)env.Height * 0.5f) / tanf(kSceneVerticalFovDeg * 0.5f * kRad);
  /* SYMBOL SIZE IS NOT POSITION. An angular size drawn at the scene's 60 deg looks 60/TFOV times
   * smaller than the pilot sees it through a combiner of TFOV degrees; positions are untouched by
   * this and stay conformal. tfov 0 = draw at true angular size (doc/render/hud-declaration.md). */
  float magnify = Deck_.TfovDeg > 1.f ? kSceneVerticalFovDeg / Deck_.TfovDeg : 1.f;
  f.mr = f.Kc * 1e-3f * magnify;

  for (const FBHudElement &e : Deck_.Elements) {
    /* WITHOUT TELEMETRY ONLY A ROW THAT ASKED FOR ITS ABSENCE DRAWS. Anything else would print a
     * zero that nothing measured, which is the one thing a display in this tree may not do. */
    bool on = FBHudFlagOn(e.When, state, env) != e.Invert;
    if (!on) continue;
    if (!env.Have && !(e.When == FBHudFlag::Telemetry && e.Invert)) continue;
    if (e.Gate != FBHudNum::None) {
      float g = FBHudNumber(e.Gate, state, env);
      if ((e.HaveLo && g < e.Lo) || (e.HaveHi && g > e.Hi)) continue;
    }

    float x = e.X * f.W + e.Dx * f.mg, y = e.Y * f.H + e.Dy * f.mg;
    if (e.Frame != FBHudFrame::Screen && e.Src != FBHudNum::None) {
      float az = FBHudNumber(e.Src, state, env), el = FBHudNumber(e.Src2, state, env);
      Proj p = e.Frame == FBHudFrame::World ? f.World(az, el) : f.Body(az, el);
      if (p.zc < 0.05f) continue;
      x = p.sx;
      y = p.sy;
    }

    switch (e.Kind) {
      case FBHudKind::Text: DrawText(f, e, x, y, out); break;
      case FBHudKind::Line:
        if (e.Count > 0) DashedLine(out, x, y, x + e.W * f.mg, y + e.H * f.mg, e.Count, e.R, e.G, e.B);
        else out.Line(x, y, x + e.W * f.mg, y + e.H * f.mg, e.R, e.G, e.B);
        break;
      case FBHudKind::Box:
        out.Box(x - 0.5f * e.W * f.mg, y - 0.5f * e.H * f.mg, x + 0.5f * e.W * f.mg, y + 0.5f * e.H * f.mg,
                e.R, e.G, e.B);
        break;
      case FBHudKind::Circle:
        out.Circle(x, y, 0.5f * e.Size * f.mr, e.Count > 2 ? e.Count : 24, e.R, e.G, e.B);
        break;
      case FBHudKind::Bar: DrawBar(f, e, x, y, out); break;
      case FBHudKind::Cross: DrawCross(f, e, x, y, out); break;
      case FBHudKind::Rose: DrawRose(f, e, x, y, out); break;
      case FBHudKind::Scope: DrawScope(f, e, x, y, out); break;
      case FBHudKind::Vector: DrawVector(f, e, x, y, out); break;
      case FBHudKind::Compass: DrawCompass(f, e, y, out); break;
      case FBHudKind::Tape: DrawTape(f, e, x, y, out); break;
      case FBHudKind::Ladder: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawLadder(f, e, out); out.ClearClip(); break;
      case FBHudKind::Horizon: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawHorizon(f, e, out); out.ClearClip(); break;
      case FBHudKind::Ils: DrawIls(f, e, x, y, out); break;
      case FBHudKind::Fpm: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawFpm(f, e, out); out.ClearClip(); break;
      case FBHudKind::Contacts: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawContacts(f, e, out); out.ClearClip(); break;
      case FBHudKind::Ccip: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawCcip(f, e, out); out.ClearClip(); break;
      case FBHudKind::Funnel: out.SetClip(f.x0, f.y0, f.x1, f.y1); DrawFunnel(f, e, out); out.ClearClip(); break;
      case FBHudKind::Dlz: DrawDlz(f, e, x, out); break;
    }
  }
}

} // namespace FlightBox::Systems
