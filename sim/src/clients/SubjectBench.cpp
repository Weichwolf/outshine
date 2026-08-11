#include "SubjectBench.h"

#include "CalmWeather.h"
#include "Camera.h"
#include "CloudDensity.h"
#include "Geodesy.h"
#include "Log.h"
#include "State.h"
#include "Units.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <cstdio>
#include <cstring>

namespace outshine::Clients {

namespace {

/* WHY THE OVERCAST AND NOT A SWITCH. "Pure sky light" has to come out of the engine's own model or it
 * measures something the scene can never show: a closed deck extinguishes the direct beam through the
 * very cloudMeanThru every lit surface already reads, and hands the loss back as the deck's downward
 * diffuse. Switching the sun term off by hand would be a second lighting model. */
class OvercastWeather : public CalmWeather {
public:
  explicit OvercastWeather(double coverPct) : Cover_(coverPct) {}
  CloudLayers Clouds(double latDeg, double lonDeg) const override {
    (void)latDeg; (void)lonDeg;
    CloudLayers c;
    c.TotalPct = Cover_;
    c.LowPct = Cover_;
    c.CeilingM = 600.0;   /* [SET] a stratus base; the deck's HEIGHT is not under judgement here */
    c.HaveCeiling = Cover_ > 0.0;
    return c;
  }

private:
  double Cover_;
};

/* 5x7, one byte per row, low bit rightmost. Digits, a point and the three unit letters — everything
 * a scale label needs and nothing else. */
struct Glyph { char C; unsigned char Row[7]; };
const Glyph kFont[] = {
  {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
  {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
  {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
  {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
  {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
  {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
  {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
  {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
  {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
  {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
  {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
  {'m', {0x00, 0x00, 0x1A, 0x15, 0x15, 0x15, 0x15}},
  {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
};

void PutPixel(uint8_t *rgba, int w, int h, int x, int y, uint8_t v) {
  if (x < 0 || y < 0 || x >= w || y >= h) return;
  uint8_t *p = rgba + ((size_t)y * (size_t)w + (size_t)x) * 4;
  p[0] = v; p[1] = v; p[2] = v; p[3] = 255;
}

void FillRect(uint8_t *rgba, int w, int h, int x0, int y0, int rw, int rh, uint8_t v) {
  for (int y = y0; y < y0 + rh; y++)
    for (int x = x0; x < x0 + rw; x++) PutPixel(rgba, w, h, x, y, v);
}

void DrawGlyph(uint8_t *rgba, int w, int h, int x0, int y0, char c, int s, uint8_t v) {
  for (const Glyph &g : kFont) {
    if (g.C != c) continue;
    for (int r = 0; r < 7; r++)
      for (int b = 0; b < 5; b++)
        if (g.Row[r] & (1u << (4 - b)))
          FillRect(rgba, w, h, x0 + b * s, y0 + r * s, s, s, v);
    return;
  }
}

void DrawText(uint8_t *rgba, int w, int h, int x0, int y0, const char *s, int sc) {
  for (int pass = 0; pass < 2; pass++) {          /* black halo first, so it reads over grass and sky */
    int x = x0;
    for (const char *p = s; *p; p++, x += 6 * sc) {
      if (pass == 0) {
        for (int dy = -1; dy <= 1; dy++)
          for (int dx = -1; dx <= 1; dx++)
            if (dx || dy) DrawGlyph(rgba, w, h, x + dx * sc, y0 + dy * sc, *p, sc, 0);
      } else {
        DrawGlyph(rgba, w, h, x, y0, *p, sc, 255);
      }
    }
  }
}

/* 1-2-5, the only ladder a ruler is ever read off. */
double NiceStep(double x) {
  if (!(x > 0.0)) return 1.0;
  const double e = std::pow(10.0, std::floor(std::log10(x)));
  const double m = x / e;
  if (m >= 5.0) return 5.0 * e;
  if (m >= 2.0) return 2.0 * e;
  return e;
}

/* The bar is the calibration made visible: its length is a 1-2-5 number of metres, its pixel length
 * follows from the declared framing, and the label states which. Both halves of the statement. */
double DrawScaleBar(uint8_t *rgba, int w, int h, double mPerPx) {
  double barM = NiceStep(0.30 * (double)w * mPerPx);
  while (barM / mPerPx > 0.60 * (double)w) barM = NiceStep(barM * 0.45);
  while (barM / mPerPx < 0.08 * (double)w) barM = NiceStep(barM * 2.4);
  const int barPx = (int)std::lround(barM / mPerPx);
  const int sc = std::max(2, h / 240);
  const int bh = std::max(3, h / 160);
  const int mar = std::max(8, h / 36);
  const int x0 = mar, y0 = h - mar - bh;

  FillRect(rgba, w, h, x0 - 2, y0 - 2, barPx + 4, bh + 4, 0);
  FillRect(rgba, w, h, x0, y0, barPx, bh, 255);
  for (int e = 0; e < 2; e++) {   /* end ticks, so the bar's ends are unambiguous over any background */
    const int x = e ? x0 + barPx - bh : x0;
    FillRect(rgba, w, h, x - 2, y0 - 3 * bh - 2, bh + 4, 3 * bh + 4, 0);
    FillRect(rgba, w, h, x, y0 - 3 * bh, bh, 3 * bh, 255);
  }

  char label[32];
  if (barM >= 1.0) std::snprintf(label, sizeof label, "%g m", barM);
  else if (barM >= 0.1) std::snprintf(label, sizeof label, "%.1f m", barM);
  else std::snprintf(label, sizeof label, "%g mm", barM * 1000.0);
  DrawText(rgba, w, h, x0, y0 - 3 * bh - 4 - 7 * sc, label, sc);
  return barM;
}

/* [SET] an eighth of the frame width. Wide enough that a reader can average a patch of it by eye,
 * narrow enough that what it costs the subject is a margin and not a share of the picture — the
 * complaint that moved the card off the floor was that it stood in 15-20 % of every frame. */
constexpr double kCardFrameFrac = 0.125;

}  // namespace

bool SubjectBench::Select(const char *templateName, double heightOverrideM) {
  for (size_t i = 0; i < Veg_.TemplateCount(); i++) {
    if (Veg_.Name(i) != templateName) continue;
    Bucket_ = (int)i;
    Kind_ = Subject::Herb;
    Template_ = templateName;
    /* The template's own declared plant height is the default, so no number here names a layer. */
    HeightM_ = heightOverrideM > 0.0 ? heightOverrideM : (double)Veg_.Rows()[i].Dry[3];
    return HeightM_ > 0.0;
  }
  return false;
}

bool SubjectBench::SelectTree(const char *speciesName, double heightM) {
  if (!(heightM > 0.0)) return false;
  Kind_ = Subject::Tree;
  Bucket_ = -1;
  Template_ = speciesName;
  HeightM_ = heightM;
  return true;
}

/* THE CAMERA PLACED AND THE STUDIO SET, in one turn: everything here is derived arithmetic and
 * renderer state, and the only thing that takes time is the temporal settle the burst at the end
 * pays for. */
void SubjectBench::Place(const Order &o) {
  const View &v = *o.Frame;
  const double camAzDeg = o.CamAzDeg;
  const int w = R_.SceneW(), h = R_.SceneH();
  const double aspect = (double)w / (double)h;
  double fov = kFovDeg;
  double halfV = 0.5 * fov * kDeg2Rad;
  double halfH = std::atan(std::tan(halfV) * aspect);

  double frameW = std::max(v.WidthM, v.WidthPerH * HeightM_);
  if (v.FrameHPerH > 0.0) frameW = std::max(frameW, v.FrameHPerH * HeightM_ * aspect);
  if (v.Square) frameW *= aspect;   /* the DECLARED width is the square crop's, not the frame's */

  double dist = v.DistPerH > 0.0 ? v.DistPerH * HeightM_ : 0.5 * frameW / std::tan(halfH);
  /* A MACRO SHOT IS A LONGER LENS, NOT A CLOSER CAMERA. Two floors on the working distance, and both
   * are derived: ten times the near cut, so the subject's own lean (up to 0.30 of its height, see
   * a stand's own lean) cannot fall through the near plane, and twice the subject's height, so the
   * picture is a SHAPE and not a perspective. The normal 45 mm holds wherever neither binds. */
  const double minWork = std::max(10.0 * (double)Render::Renderer::kNearM, 2.0 * HeightM_);
  if (dist < minWork) dist = minWork;
  if (v.DistPerH > 0.0) frameW = 2.0 * dist * std::tan(halfH);
  else if (0.5 * frameW / std::tan(halfH) < minWork) {
    halfH = std::atan(0.5 * frameW / dist);
    halfV = std::atan(std::tan(halfH) / aspect);
    fov = 2.0 * halfV / kDeg2Rad;
  }
  const double frameH = frameW / aspect;

  double pitch = v.PitchDeg, eyeAgl, targetH;
  if (v.EyeAglM >= 0.0) {
    eyeAgl = v.EyeAglM;
    targetH = eyeAgl + dist * std::sin(pitch * kDeg2Rad);
    /* A declared human eye is a herb-layer anchor. Once the subject's top would leave the frame it
     * stops being one, and the camera aims at mid-height instead — that is what lets a 25 m subject
     * through this same view without a line of code moving. */
    if (targetH + 0.5 * frameH < HeightM_) {
      pitch = std::atan2(0.5 * HeightM_ - eyeAgl, dist) / kDeg2Rad;
      targetH = eyeAgl + dist * std::sin(pitch * kDeg2Rad);
    }
  } else {
    targetH = v.TargetPerH < 0.0 ? 0.5 * frameH : v.TargetPerH * HeightM_;
    eyeAgl = targetH - dist * std::sin(pitch * kDeg2Rad);
  }

  const double azR = camAzDeg * kDeg2Rad;
  const double sinA = std::sin(azR), cosA = std::cos(azR);
  const double outward = dist * std::cos(pitch * kDeg2Rad);
  const double camE = -outward * sinA, camN = -outward * cosA;
  const double camLat = Lat_ + camN / kMPerDeg;
  const double camLon = Lon_ + camE / (kMPerDeg * std::cos(Lat_ * kDeg2Rad));
  const double camAsl = AslM_ + eyeAgl;

  /* THE WIND BLOWS ACROSS THE FRAME, left to right, so a bend is a bend and not a foreshortening.
   * It comes FROM camAz + 270 and therefore blows TOWARD camAz + 90, which is the camera's right. */
  R_.SetWind(camAzDeg + 270.0, o.WindMs);
  R_.SetWindClock(0.0);

  OvercastWeather wx(o.CloudCover * 100.0);
  R_.SetFovDeg(fov);

  State hs{};
  hs.Platform.AltM = (float)camAsl;
  hs.Platform.YawDeg = (float)camAzDeg;
  hs.Platform.PitchDeg = (float)pitch;
  hs.Env.SunElDeg = (float)o.SunElDeg;
  hs.Env.SunAzDeg = (float)o.SunAzDeg;
  hs.Env.CloudCover = (float)o.CloudCover;
  R_.SetSceneState(hs);

  double eye[3], fwd[3], right[3], up[3];
  GeoToEcef(camLat, camLon, camAsl, eye);
  CameraBasisEcef(camAzDeg, pitch, 0.0, camLat, camLon, fwd, right, up);
  R_.SetCameraBasis(eye, fwd, right, up);

  /* The floor reaches to the eye's own geometric horizon, sqrt(2*R*h) — one metre further and it
   * would stand above the horizon the sky pass draws. */
  const double floorR = std::sqrt(2.0 * 6371000.0 * std::max(eyeAgl, 0.001));
  const double gridM = NiceStep(frameW / 8.0);
  R_.SetBenchGround(eyeAgl, floorR, gridM);
  R_.SetBenchSubstrate(SubstrateRgb_);
  /* The subject stands where the camera aims, on the floor: the same `ahead` the card is placed by,
   * so plant, card and ruled plane cannot drift apart. */
  if (Kind_ == Subject::Tree)
    R_.SetTreeStand(sinA * dist * std::cos(pitch * kDeg2Rad), cosA * dist * std::cos(pitch * kDeg2Rad),
                    eyeAgl, HeightM_);

  /* WHERE THE NEUTRAL CARD STANDS. At the frame's LEFT EDGE and upright at the target plane, because
   * the only place a reference can be without becoming the subject's background is the edge, and the
   * only orientation that reads under every one of the lights is square to the sight line. It is as
   * tall as the frame or half again the subject, whichever is more, so a closed stand cannot bury it.
   * A nadir view gets none: its picture IS the measured square metre and a card inside it would be
   * area subtracted from the coverage. */
  Render::BenchCard card;
  if (!v.Square) {
    const double latE = cosA, latN = -sinA;   /* the camera's right, in the ground plane */
    const double ahead = dist * std::cos(pitch * kDeg2Rad);
    const double half = 0.5 * kCardFrameFrac * frameW;
    const double lateral = half - 0.5 * frameW;
    card.EastM = sinA * ahead + latE * lateral;
    card.NorthM = cosA * ahead + latN * lateral;
    card.LatE = latE;
    card.LatN = latN;
    card.HalfWidthM = half;
    card.HeightM = std::max(frameH, 1.5 * HeightM_);
  }
  R_.SetBenchCard(card);

  ExposureParams ep;
  if (o.Metering) {
    ep.Mode = ExposureMode::Auto;
  } else {
    ep.Mode = ExposureMode::Manual;
    ep.KeyEv = KeyEv_;   /* ONE placement for every light, or the three are not comparable */
  }
  R_.SetExposure(ep);

  /* The bench PLACES the camera per cell instead of walking it there, so the accumulated history
   * belongs to another view entirely; it is dropped and rebuilt rather than reprojected. */
  R_.ResetTemporal();
  for (int f = 0; f < 3 + R_.TemporalSettleFrames(); f++) R_.RenderFrame();

  Shot_.Aspect = aspect;
  Shot_.Fov = fov;
  Shot_.HalfV = halfV;
  Shot_.FrameW = frameW;
  Shot_.FrameH = frameH;
  Shot_.Dist = dist;
  Shot_.Pitch = pitch;
  Shot_.EyeAgl = eyeAgl;
  Shot_.TargetH = targetH;
  Shot_.FloorR = floorR;
  Shot_.GridM = gridM;
  Shot_.MPerPx = frameW / (double)w;
  Shot_.OutW = v.Square ? h : w;
  Shot_.OutH = h;
  Shot_.Ox = v.Square ? (w - h) / 2 : 0;
  Shot_.Oy = 0;
  Shot_.Card = card;
}

/* THE MATRIX, LAID OUT. Every view under every light, no exceptions — the framings are the
 * botanist's axis, the lights are the art-director's, and a hole in it is a question neither can
 * answer. It is built once so that the run through it is an index and not four nested loops with a
 * readback in the middle of them.
 *
 * THE REFERENCE LIGHT METERS FIRST and every other picture is held at its placement. Auto-exposing
 * each light would normalise all three and make "transmission against reflection" unmeasurable. */
bool SubjectBench::Begin() {
  if (Kind_ == Subject::None) return false;

  static const View kHerbViews[] = {
    /* name          frameH/H  width/H   widthM  dist/H   pitch    eye   target   az     sq    turn   wind */
    {"portrait",     1.0,      0.0,      0.0,    0.0,      0.0,    -1.0, -1.0,     0.0, false, true,  true},
    {"a",            0.0,      0.0,      0.0,    3.3333,   0.0,    -1.0, -1.0,     0.0, false, false, false},
    {"b",            0.0,      0.0,      0.0,    3.3333,   0.0,    -1.0, -1.0,    90.0, false, false, false},
    {"closeup_hd",   0.0,      0.2,      0.0,    0.0,      0.0,    -1.0, -1.0,     0.0, false, false, false},
    {"tuft",         0.0,      1.3333,   0.0,    0.0,    -35.0,    -1.0,  0.5,     0.0, false, true,  true},
    {"sward",        0.0,      0.0,      1.0,    0.0,    -90.0,    -1.0,  0.0,     0.0, true,  false, true},
    {"eye",          0.0,      2.0,      4.0,    0.0,     -3.0,     1.70, 0.0,     0.0, false, true,  true},
  };

  /* THE TREE'S OWN FRAMINGS, and the two that changed say why. `sward` measured coverage over one
   * square metre held vertically — at 30 m that samples a random hand's breadth of canopy and answers
   * nothing, so `crown` takes its place: a NADIR square 1.6 subject heights wide, which is the picture
   * a declared `spread_m` is read off directly. `tuft` framed 0.4 m of grass from 35 deg above; the
   * same declaration at 30 m is a raised three-quarter of the whole tree, so it is named `oblique`
   * for what it now is. `closeup_hd` keeps the BOLE, where bark is judged, and `leaf_hd` is the same
   * lens raised to 0.65 of the height, where the lamina is — one framing cannot hold both at 0.2
   * subject heights. THE WIND ROW IS GONE: nothing in the tree's draw reads the wind this round,
   * and three identical files with three different names is the one thing this bench may not write. */
  static const View kTreeViews[] = {
    /* name          frameH/H  width/H   widthM  dist/H   pitch    eye   target   az     sq    turn   wind */
    {"portrait",     1.0,      0.0,      0.0,    0.0,      0.0,    -1.0, -1.0,     0.0, false, true,  false},
    {"a",            0.0,      0.0,      0.0,    3.3333,   0.0,    -1.0, -1.0,     0.0, false, false, false},
    {"b",            0.0,      0.0,      0.0,    3.3333,   0.0,    -1.0, -1.0,    90.0, false, false, false},
    {"closeup_hd",   0.0,      0.2,      0.0,    0.0,      0.0,    -1.0, -1.0,     0.0, false, false, false},
    {"leaf_hd",      0.0,      0.2,      0.0,    0.0,      0.0,    -1.0,  0.65,    0.0, false, false, false},
    {"oblique",      0.0,      1.3333,   0.0,    0.0,    -35.0,    -1.0,  0.5,     0.0, false, true,  false},
    {"crown",        0.0,      1.6,      0.0,    0.0,    -90.0,    -1.0,  0.0,     0.0, true,  false, false},
    {"eye",          0.0,      2.0,      4.0,    0.0,     -3.0,     1.70, 0.0,     0.0, false, true,  false},
  };

  const View *views = Kind_ == Subject::Tree ? kTreeViews : kHerbViews;
  const size_t viewCount = Kind_ == Subject::Tree ? std::size(kTreeViews) : std::size(kHerbViews);

  Orders_.push_back({&views[0], "frontlit", 0.0, 180.0, kSunElDeg, 0.0, 0.0, true});
  for (size_t vi = 0; vi < viewCount; vi++) {
    const View &v = views[vi];
    const double az = v.AzOffDeg;
    Orders_.push_back({&v, "frontlit", az, az + 180.0, kSunElDeg, 0.0, 0.0, false});
    Orders_.push_back({&v, "backlit",  az, az,         kSunElDeg, 0.0, 0.0, false});
    Orders_.push_back({&v, "skylight", az, az + 180.0, kSunElDeg, 1.0, 0.0, false});
    /* THE WIND ROW, and it was refused until this round because three identical files would have
     * been a lie. Same geometry, same light, same exposure; the only thing that differs is the
     * declared met wind, so a difference between two of these files can be nothing else. */
    if (v.Wind)
      for (double ms : {0.0, 6.0, 12.0}) {
        char name[24];
        std::snprintf(name, sizeof name, "wind%02d", (int)std::lround(ms));
        Orders_.push_back({&v, name, az, az + 180.0, kSunElDeg, 0.0, ms, false});
      }
    if (!v.Turntable) continue;
    for (int k = 0; k < TurnSteps_; k++) {
      const double a = az + 360.0 * (double)k / (double)TurnSteps_;
      char name[24];
      std::snprintf(name, sizeof name, "turn%03d", (int)std::lround(a - az) % 360);
      /* THE SUN STAYS WHERE THE VIEW'S OWN FRONTLIT PUT IT and the camera walks around: that is what a
       * turntable measures. It is therefore indexed by the same quantity the named lights are — the
       * sun's bearing RELATIVE to the camera, logged as `sunRelDeg` — and turn000 is frontlit to the
       * bit. turn180 is the backlit BEARING seen from the other face of the stand, which is not the
       * backlit PICTURE. */
      Orders_.push_back({&v, name, a, az + 180.0, kSunElDeg, 0.0, 0.0, false});
    }
  }
  return true;
}

SubjectBench::Progress SubjectBench::Step() {
  if (!Begun_) {
    Begun_ = true;
    if (!Begin()) return Progress::Failed;
  }
  if (OrderIx_ >= Orders_.size()) {
    Log::Info("rig", "done", {{"template", Template_}, {"heightM", HeightM_}, {"images", Written_},
        {"dir", OutDir_}, {"keyEv", (double)KeyEv_}, {"fovDeg", kFovDeg},
        {"substrate", Substrate_},
        {"substrateRgb", std::to_string(SubstrateRgb_[0]) + "," + std::to_string(SubstrateRgb_[1])
                       + "," + std::to_string(SubstrateRgb_[2])},
        {"cardAlbedo", (double)Render::BenchGroundStage::kCardAlbedo},
        {"gaps", "leaf|season"}});
    return Progress::Done;
  }

  const Order &o = Orders_[OrderIx_];
  switch (Take_) {
    case Take::Place:
      Place(o);
      Take_ = Take::Meter;
      return Progress::Running;
    case Take::Meter:
    case Take::MeterIrradiance:
      return Meter(o);
    case Take::Pixels: {
      const Render::ReadState st = R_.ReadPixels(Rgba_);
      if (st == Render::ReadState::Pending) return Progress::Running;
      if (st == Render::ReadState::Failed) return Progress::Failed;
      Img_.assign((size_t)Shot_.OutW * (size_t)Shot_.OutH * 4, 0);
      for (int y = 0; y < Shot_.OutH; y++)
        std::memcpy(&Img_[(size_t)y * (size_t)Shot_.OutW * 4],
                    &Rgba_[((size_t)(y + Shot_.Oy) * (size_t)R_.SceneW() + (size_t)Shot_.Ox) * 4],
                    (size_t)Shot_.OutW * 4);
      Take_ = Take::DepthAll;
      return Progress::Running;
    }
    /* THE STUDIO IS TAKEN AWAY, NOT THE SUBJECT, and that is what makes this work for anything that
     * will ever stand on the bench: retiring the floor leaves subject + card, retiring the card as
     * well leaves the subject alone against the sky, and a reversed-Z depth of 0 is "nothing here".
     * No render below touches whatever draws the subject. */
    case Take::DepthAll: {
      const Render::ReadState st = R_.ReadDepth(DepthAll_);   /* subject + card + floor */
      if (st == Render::ReadState::Pending) return Progress::Running;
      if (st == Render::ReadState::Failed) return Progress::Failed;
      R_.SetBenchCard(Render::BenchCard{});
      for (int i = 0; i < 3; i++) R_.RenderFrame();
      Take_ = Take::DepthNoCard;
      return Progress::Running;
    }
    case Take::DepthNoCard: {
      const Render::ReadState st = R_.ReadDepth(DepthNoCard_);   /* subject + floor */
      if (st == Render::ReadState::Pending) return Progress::Running;
      if (st == Render::ReadState::Failed) return Progress::Failed;
      R_.SetBenchGround(Shot_.EyeAgl, 0.0, Shot_.GridM);   /* the floor retires the card with it */
      for (int i = 0; i < 3; i++) R_.RenderFrame();
      Take_ = Take::DepthSubject;
      return Progress::Running;
    }
    case Take::DepthSubject: {
      const Render::ReadState st = R_.ReadDepth(DepthSubject_);   /* subject alone, sky behind */
      if (st == Render::ReadState::Pending) return Progress::Running;
      if (st == Render::ReadState::Failed) return Progress::Failed;
      R_.SetBenchGround(Shot_.EyeAgl, Shot_.FloorR, Shot_.GridM);
      R_.SetBenchCard(Shot_.Card);
      Take_ = Take::Write;
      return Progress::Running;
    }
    case Take::Write:
      return Write(o);
  }
  return Progress::Failed;
}

SubjectBench::Progress SubjectBench::Meter(const Order &o) {
  if (Take_ == Take::Meter) {
    const Render::ReadState st = R_.ReadExposure(Meter_);
    if (st == Render::ReadState::Pending) return Progress::Running;
    if (st == Render::ReadState::Failed) return Progress::Failed;
    if (!o.Metering) {
      Take_ = Take::Pixels;
      return Progress::Running;
    }
    KeyEv_ = Meter_[1];
    Metered_ = true;
    Take_ = Take::MeterIrradiance;
    return Progress::Running;
  }
  float irr[Render::IrradianceStage::kFloats] = {};
  const Render::ReadState st = R_.ReadIrradiance(irr);
  if (st == Render::ReadState::Pending) return Progress::Running;
  Log::Info("rig", "meter", {{"keyEv", (double)KeyEv_}, {"light", o.Light},
      {"horizE", (double)Meter_[2]}, {"sunY", 0.2126 * irr[0] + 0.7152 * irr[1] + 0.0722 * irr[2]},
      {"skyY", 0.2126 * irr[4] + 0.7152 * irr[5] + 0.0722 * irr[6]}});
  OrderIx_++;
  Take_ = Take::Place;
  return Progress::Running;
}

SubjectBench::Progress SubjectBench::Write(const Order &o) {
  const View &v = *o.Frame;
  const Fill fill = MeasureFill();
  if (fill.Pct < 0.0) return Progress::Failed;

  const double barM = DrawScaleBar(Img_.data(), Shot_.OutW, Shot_.OutH, Shot_.MPerPx);

  char path[512];
  std::snprintf(path, sizeof path, "%s/%s-%s-%s.png", OutDir_.c_str(), Template_.c_str(), v.Name,
                o.Light.c_str());
  if (Out_.Png(path, Img_.data(), Shot_.OutW, Shot_.OutH) == Artifacts::Delivery::Refused) {
    Log::Error("rig", "png_write_failed", {{"path", std::string(path)}});
    return Progress::Failed;
  }

  /* A NADIR FILL IS THE BOTANICAL COVERAGE and at no other pointing is it one, so the name only
   * appears where the geometry earns it. */
  const bool nadir = v.Square && Shot_.Pitch <= -89.9;
  const double fPx = 0.5 * (double)R_.SceneH() / std::tan(Shot_.HalfV);
  Log::Info("rig", "shot", {{"path", std::string(path)}, {"view", v.Name}, {"light", o.Light},
      {"w", Shot_.OutW}, {"h", Shot_.OutH},
      {"frameWidthM", v.Square ? Shot_.FrameW / Shot_.Aspect : Shot_.FrameW},
      {"frameHeightM", Shot_.FrameH},
      {"distM", Shot_.Dist}, {"eyeAglM", Shot_.EyeAgl}, {"targetM", Shot_.TargetH},
      {"pitchDeg", Shot_.Pitch}, {"camAzDeg", o.CamAzDeg}, {"sunAzDeg", o.SunAzDeg},
      {"sunElDeg", o.SunElDeg},
      {"sunRelDeg", std::fmod(o.SunAzDeg - o.CamAzDeg + 720.0, 360.0)},
      {"cloudCover", o.CloudCover}, {"fovDeg", Shot_.Fov}, {"focalPx", fPx},
      {"focal35mm", 12.0 / std::tan(Shot_.HalfV)},
      {"mmPerPx", Shot_.MPerPx * 1000.0}, {"scaleBarM", barM}, {"gridM", Shot_.GridM},
      {"substrate", Substrate_}, {"cardWidthM", 2.0 * Shot_.Card.HalfWidthM},
      {"cardHeightM", Shot_.Card.HeightM},
      {"cardPct", fill.CardPct}, {"cardMedian", fill.CardMedian},
      {"fillPct", fill.Pct}, {"subjectMedian", fill.Median},
      {"coverPct", nadir ? fill.Pct : -1.0}});

  /* [SET] a tenth of the frame filled and a median at the 8-bit floor. A picture in that state carries
   * no judgement, and it is the state both critics mistook for an EMPTY view — so the bench says it
   * out loud instead of leaving a reader to infer it from a field. */
  if (fill.Pct >= 10.0 && fill.Median >= 0 && fill.Median <= 1)
    Log::Warn("rig", "subject_below_floor", {{"path", std::string(path)}, {"fillPct", fill.Pct},
        {"subjectMedian", fill.Median}, {"keyEv", (double)KeyEv_},
        {"why", "geometry stands in the frame and renders at code 0 — a shading defect, not an empty view"}});
  Written_++;
  OrderIx_++;
  Take_ = Take::Place;
  return Progress::Running;
}

SubjectBench::Fill SubjectBench::MeasureFill() {
  Fill f;
  const int w = R_.SceneW();
  long subjHist[256] = {}, cardHist[256] = {}, subj = 0, cardSeen = 0, open = 0;
  for (int y = 0; y < Shot_.OutH; y++)
    for (int x = 0; x < Shot_.OutW; x++) {
      const size_t i = (size_t)(y + Shot_.Oy) * (size_t)w + (size_t)(x + Shot_.Ox);
      /* Reversed Z: 0 is "nothing here", and nearer is larger. */
      const bool hasCard = DepthAll_[i] > DepthNoCard_[i] * 1.001f;
      const bool hasSubj = !hasCard && DepthSubject_[i] > 0.0f;
      const uint8_t *p = &Rgba_[i * 4];
      const int y8 = (int)(0.2126 * (double)p[0] + 0.7152 * (double)p[1] + 0.0722 * (double)p[2]);
      const int lum = y8 < 0 ? 0 : (y8 > 255 ? 255 : y8);
      if (hasCard) { cardHist[lum]++; cardSeen++; }
      else { if (hasSubj) { subjHist[lum]++; subj++; } open++; }
    }
  const auto median = [](const long *h, long n) {
    long acc = 0;
    for (int i = 0; i < 256; i++) { acc += h[i]; if (n > 0 && acc * 2 >= n) return i; }
    return -1;
  };
  f.Pct = open ? 100.0 * (double)subj / (double)open : 0.0;
  f.CardPct = 100.0 * (double)cardSeen / ((double)Shot_.OutW * (double)Shot_.OutH);
  f.Median = median(subjHist, subj);
  f.CardMedian = median(cardHist, cardSeen);
  return f;
}

} // namespace outshine::Clients
