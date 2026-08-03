#include "FBTacticalMap.h"

#include "FBGeodesy.h"
#include "FBHudFont.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace FlightBox::Render {

namespace {
/* The map's palette, and it carries the same information the symbol shape does — a colour-blind reader
 * loses nothing, which is why the shape is the primary channel and this the secondary. */
const float kFriendR = 0.35f, kFriendG = 0.80f, kFriendB = 1.00f;   /* APP-6 cyan */
const float kUnknownR = 1.00f, kUnknownG = 0.90f, kUnknownB = 0.35f;/* APP-6 yellow */
const float kGridR = 0.35f, kGridG = 0.70f, kGridB = 0.45f;
const float kTextR = 0.60f, kTextG = 1.00f, kTextB = 0.65f;
const float kWarnR = 1.00f, kWarnG = 0.55f, kWarnB = 0.25f;
/* [SET] A chart is READ, not flown through: the strokes carry width so they survive an OSM sheet under
 * them, and the sheet is dimmed rather than the symbols brightened. */
const float kSymbolW = 1.6f;

/* An age FADES a symbol rather than deleting it: what the force holds is the best it has, and the map
 * must not hide it just because it got old. */
float AgeFade(float ageS) {
  if (ageS <= FBTacticalMap::kStaleS) return 1.0f;
  if (ageS >= FBTacticalMap::kColdS) return 0.40f;
  float t = (ageS - FBTacticalMap::kStaleS) / (FBTacticalMap::kColdS - FBTacticalMap::kStaleS);
  return 1.0f - 0.60f * t;
}

void Dashed(Systems::FBHudGeometry &out, float x0, float y0, float x1, float y1, float dash,
            float w, float r, float g, float b) {
  float dx = x1 - x0, dy = y1 - y0;
  float len = std::sqrt(dx * dx + dy * dy);
  if (len < 1.0f) return;
  float ux = dx / len, uy = dy / len;
  for (float s = 0.0f; s < len; s += dash * 2.0f) {
    float e = s + dash < len ? s + dash : len;
    out.QLine(x0 + ux * s, y0 + uy * s, x0 + ux * e, y0 + uy * e, w, r, g, b);
  }
}

/* An arc as chords, because that is what the stroke pipeline draws. `a0`/`a1` in radians, 0 = +x. */
void Arc(Systems::FBHudGeometry &out, float cx, float cy, float r, float a0, float a1, int seg,
         float w, float cr, float cg, float cb) {
  float px = cx + r * std::cos(a0), py = cy + r * std::sin(a0);
  for (int i = 1; i <= seg; i++) {
    float a = a0 + (a1 - a0) * (float)i / (float)seg;
    float nx = cx + r * std::cos(a), ny = cy + r * std::sin(a);
    out.QLine(px, py, nx, ny, w, cr, cg, cb);
    px = nx; py = ny;
  }
}
} // namespace

void FBTacticalMap::SetSelected(const char *callsign) {
  if (!callsign) { Selected_[0] = 0; return; }
  std::strncpy(Selected_, callsign, kForceLabelLen - 1);
  Selected_[kForceLabelLen - 1] = 0;
}

void FBTacticalMap::SetStatus(const char *text) {
  if (!text) { Status_[0] = 0; return; }
  std::strncpy(Status_, text, sizeof Status_ - 1);
  Status_[sizeof Status_ - 1] = 0;
}

void FBTacticalMap::SetGroundNote(const char *text) {
  if (!text) { GroundNote_[0] = 0; return; }
  std::strncpy(GroundNote_, text, sizeof GroundNote_ - 1);
  GroundNote_[sizeof GroundNote_ - 1] = 0;
}

/* APP-6 FRIEND, air: a semicircle open at the bottom. Screen y is down, so the open side is +y. */
void FBTacticalMap::DrawFriendAir(Systems::FBHudGeometry &out, float x, float y, float r, float cr,
                                  float cg, float cb) const {
  Arc(out, x, y, r, (float)kPi, (float)(2.0 * kPi), 14, kSymbolW, cr, cg, cb);
  out.QLine(x - r, y, x + r, y, kSymbolW, cr, cg, cb);
}

/* APP-6 UNKNOWN, air: the quatrefoil, drawn as its four lobes. Deliberately NOT a diamond and not a
 * square — those are HOSTILE and NEUTRAL, and this tree may assert neither. */
void FBTacticalMap::DrawUnknownAir(Systems::FBHudGeometry &out, float x, float y, float r, float cr,
                                   float cg, float cb) const {
  float lobe = r * 0.55f, off = r * 0.55f;
  Arc(out, x - off, y - off, lobe, (float)kPi * 0.5f, (float)kPi * 2.2f, 10, kSymbolW, cr, cg, cb);
  Arc(out, x + off, y - off, lobe, (float)kPi * 0.8f, (float)kPi * 2.5f, 10, kSymbolW, cr, cg, cb);
  Arc(out, x - off, y + off, lobe, (float)kPi * -0.2f, (float)kPi * 1.5f, 10, kSymbolW, cr, cg, cb);
  Arc(out, x + off, y + off, lobe, (float)kPi * 0.5f, (float)kPi * 2.2f, 10, kSymbolW, cr, cg, cb);
}

/* A DIRECTION AND NOTHING ELSE. It starts at the observer who measured it and simply runs off the map:
 * there is no end, because there is no range. */
void FBTacticalMap::DrawBearing(Systems::FBHudGeometry &out, const FBMapView &v,
                                const FBForceSymbol &s) const {
  float ox, oy;
  v.Project(s.ObsLatDeg, s.ObsLonDeg, ox, oy);
  double a = s.BearingDeg * kDeg2Rad;
  float len = (float)v.Width;   /* off the edge: the clip does the cutting, not an invented range */
  float ex = ox + len * (float)std::sin(a), ey = oy - len * (float)std::cos(a);
  float f = AgeFade(s.AgeS);
  Dashed(out, ox, oy, ex, ey, 7.0f, 1.0f, kUnknownR * f, kUnknownG * f, kUnknownB * f * 0.5f);
}

void FBTacticalMap::DrawGrid(Systems::FBHudGeometry &out, const FBMapView &v) const {
  /* A ROUND NUMBER OF KILOMETRES, chosen so that six to twelve lines cross the frame at any span. */
  double stepM = 10000.0;
  while (v.SpanM / stepM > 12.0) stepM *= 2.0;
  while (v.SpanM / stepM < 4.0) stepM *= 0.5;
  double pxPerM = (double)v.Width / v.SpanM;
  int n = (int)(v.SpanM / stepM) + 2;
  for (int i = -n; i <= n; i++) {
    float x = (float)(v.CentreX() + i * stepM * pxPerM);
    float y = (float)(v.CentreY() + i * stepM * pxPerM);
    if (x >= 0.0f && x <= (float)v.Width) out.Line(x, 0.0f, x, (float)v.Height, kGridR, kGridG, kGridB);
    if (y >= 0.0f && y <= (float)v.Height) out.Line(0.0f, y, (float)v.Width, y, kGridR, kGridG, kGridB);
  }
  /* The scale bar carries the grid's own step, so the reader never has to trust a number he cannot
   * check against something drawn. */
  float barPx = (float)(stepM * pxPerM);
  float bx = 24.0f, by = (float)v.Height - 34.0f;
  out.QLine(bx, by, bx + barPx, by, 1.5f, kTextR, kTextG, kTextB);
  out.Line(bx, by - 5.0f, bx, by + 5.0f, kTextR, kTextG, kTextB);
  out.Line(bx + barPx, by - 5.0f, bx + barPx, by + 5.0f, kTextR, kTextG, kTextB);
  out.Printf(bx, by - 20.0f, 1.6f, kTextR, kTextG, kTextB, "%d KM", (int)(stepM / 1000.0));
  /* North, and it is always up: this map does not rotate, because a commander's chart does not. */
  float nx = (float)v.Width - 40.0f, ny = 46.0f;
  out.Line(nx, ny, nx, ny - 22.0f, kTextR, kTextG, kTextB);
  out.Line(nx, ny - 22.0f, nx - 5.0f, ny - 13.0f, kTextR, kTextG, kTextB);
  out.Line(nx, ny - 22.0f, nx + 5.0f, ny - 13.0f, kTextR, kTextG, kTextB);
  out.Printf(nx - 4.0f, ny + 6.0f, 1.6f, kTextR, kTextG, kTextB, "N");
}

void FBTacticalMap::DrawLegend(Systems::FBHudGeometry &out, const FBForcePicture &pic,
                               const FBMapView &v) const {
  out.Fill(0.0f, 0.0f, (float)v.Width, 30.0f, 0.0f, 0.05f, 0.0f, 0.55f);
  out.Printf(14.0f, 20.0f, 1.8f, kTextR, kTextG, kTextB,
             "TACTICAL  OWN %d  CONTACTS %d  BEARINGS %d  T %.1f",
             pic.OwnCount(), pic.ContactCount(), pic.BearingCount(), pic.NowS());
  out.Printf((float)v.Width - 330.0f, 20.0f, 1.6f, kTextR, kTextG, kTextB,
             "APP-6  FRIEND=ARC  UNKNOWN=QUATREFOIL");
  if (Selected_[0])
    out.Printf(14.0f, (float)v.Height - 54.0f, 1.8f, kFriendR, kFriendG, kFriendB, "SEL %s", Selected_);
  if (Status_[0])
    out.Printf(14.0f, (float)v.Height - 12.0f, 1.6f, kTextR, kTextG, kTextB, "%s", Status_);
  /* A MISSING GROUND IS A STATEMENT, not an absence: a chart with no sheet under it looks exactly
   * like a chart over empty country, and the reader would trust the empty country. */
  if (GroundNote_[0]) {
    const float sc = 2.4f;
    float w = Systems::kFontAdvance * sc * (float)std::strlen(GroundNote_) + 40.0f;
    float x0 = (float)v.Width * 0.5f - w * 0.5f, y0 = (float)v.Height * 0.5f - 42.0f;
    out.Fill(x0, y0, x0 + w, y0 + 40.0f, 0.22f, 0.06f, 0.02f, 0.82f);
    out.Box(x0, y0, x0 + w, y0 + 40.0f, kWarnR, kWarnG, kWarnB);
    out.Printf(x0 + 20.0f, y0 + 13.0f, sc, kWarnR, kWarnG, kWarnB, "%s", GroundNote_);
  }
}

void FBTacticalMap::Build(const FBForcePicture &pic, const FBMapView &v,
                          Systems::FBHudGeometry &out) const {
  if (v.Width <= 0 || v.Height <= 0) return;
  out.SetClip(0.0f, 0.0f, (float)v.Width, (float)v.Height);
  /* THE VEIL over the OSM sheet, and it is the MFD bays' own device: symbology needs contrast, and
   * dimming the ground is the way to get it that does not change what the ground says. */
  out.Fill(0.0f, 0.0f, (float)v.Width, (float)v.Height, 0.02f, 0.05f, 0.09f, 0.62f);
  DrawGrid(out, v);

  /* The directions first, so the points sit on top of them: a bearing crosses the whole frame and
   * would otherwise strike through every symbol it passes. */
  for (int i = 0; i < pic.Count(); i++) {
    const FBForceSymbol &s = pic.At(i);
    if (!s.HavePoint) DrawBearing(out, v, s);
  }

  for (int i = 0; i < pic.Count(); i++) {
    const FBForceSymbol &s = pic.At(i);
    if (!s.HavePoint) continue;
    float x, y;
    v.Project(s.LatDeg, s.LonDeg, x, y);
    if (x < -40.0f || y < -40.0f || x > (float)v.Width + 40.0f || y > (float)v.Height + 40.0f) continue;

    float f = AgeFade(s.AgeS);
    bool friendly = s.Aff == FBAffiliation::Friend;
    float cr = (friendly ? kFriendR : kUnknownR) * f;
    float cg = (friendly ? kFriendG : kUnknownG) * f;
    float cb = (friendly ? kFriendB : kUnknownB) * f;
    float r = 11.0f;

    if (friendly) DrawFriendAir(out, x, y, r, cr, cg, cb);
    else DrawUnknownAir(out, x, y, r, cr, cg, cb);

    /* THE MOVEMENT MODIFIER, and only where a source actually carried a velocity: a cooperative
     * message does, an anonymous echo does not. A leader drawn from nothing would be an invention. */
    if (s.SpeedMs >= 0.0f && s.HeadingDeg >= 0.0f) {
      double a = s.HeadingDeg * kDeg2Rad;
      float lead = 14.0f + (float)(s.SpeedMs * 0.06);
      out.QLine(x, y, x + lead * (float)std::sin(a), y - lead * (float)std::cos(a), kSymbolW, cr, cg, cb);
    }
    /* The datum's own uncertainty, drawn: past the coast limit it is an AGE and not a place, so the
     * ring says how far a 250 m/s target could have moved since anybody looked. */
    if (s.AgeS > kColdS) {
      float ringM = (float)(s.AgeS * 250.0);
      float ringPx = (float)(ringM * (double)v.Width / v.SpanM);
      if (ringPx > r && ringPx < (float)v.Width) {
        int seg = 24;
        for (int k = 0; k < seg; k += 2) {
          float a0 = (float)(2.0 * kPi * k / seg), a1 = (float)(2.0 * kPi * (k + 1) / seg);
          Arc(out, x, y, ringPx, a0, a1, 2, 1.0f, cr * 0.7f, cg * 0.7f, cb * 0.7f);
        }
      }
    }
    if (Selected_[0] && s.Label[0] && std::strcmp(Selected_, s.Label) == 0)
      out.Box(x - r - 6.0f, y - r - 6.0f, x + r + 6.0f, y + r + 6.0f, kFriendR, kFriendG, kFriendB);

    /* THE PROVENANCE, ON THE SCREEN. Every symbol prints where it came from and how old it is, which
     * is doc/player-layer.md §1 check 2 made visible rather than merely enforced in a constructor. */
    char line[64];
    if (s.Label[0]) std::snprintf(line, sizeof line, "%s", s.Label);
    else std::snprintf(line, sizeof line, "%s", FBForceSourceStr(s.Src));
    out.Printf(x + r + 5.0f, y - 3.0f, 1.9f, cr, cg, cb, "%s", line);
    if (s.AgeS > 0.05f)
      out.Printf(x + r + 5.0f, y + 14.0f, 1.5f, cr * 0.95f, cg * 0.95f, cb * 0.95f, "%s %.0fS",
                 FBForceSourceStr(s.Src), (double)s.AgeS);
    else if (s.Label[0])
      out.Printf(x + r + 5.0f, y + 14.0f, 1.5f, cr * 0.8f, cg * 0.8f, cb * 0.8f, "%s",
                 FBForceSourceStr(s.Src));
    if (s.AltM > 1.0f)
      out.Printf(x - r - 46.0f, y + 14.0f, 1.5f, cr * 0.85f, cg * 0.85f, cb * 0.85f, "%d",
                 (int)(s.AltM / 30.48f));   /* flight level: hundreds of feet */
  }

  DrawLegend(out, pic, v);
  out.ClearClip();
}

} // namespace FlightBox::Render
