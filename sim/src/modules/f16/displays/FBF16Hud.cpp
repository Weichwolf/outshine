#include "FBF16Hud.h"
#include "FBCamera.h"
#include <cmath>
#include <cstdio>

namespace FlightBox::Modules {

namespace {
constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kR2D = 57.29577951308232f;
constexpr float kHudFovDeg = 80.0f;
/* Combiner APERTURE — the real HUD is a small window, not the whole windscreen. Horizontal half-angle
 * is TFOV/2 [DOC]; the VERTICAL is DERIVED from IFOV's aspect ratio rather than set as a second magic
 * constant. doc/modules-f16.md §12.1. */
constexpr float kApertureHalfWidthDeg = 12.5f;                                    /* TFOV/2 */
constexpr float kApertureIfovH = 20.0f, kApertureIfovV = 13.5f;                   /* IFOV, for the ratio only */
constexpr float kApertureHalfHeightDeg = kApertureHalfWidthDeg * (kApertureIfovV / kApertureIfovH);
/* [SET] Enlarges only the DRAWN window and the fixed pixel sizes in it; the physical aperture ANGLE
 * above stays what the conformal projector uses. */
constexpr float kHudMagnify = 1.88f;
/* Text-scale FLOORS the magnify multiplies from [ABL], so the ink clears B612's own legibility ratio. */
constexpr float kHudReadoutScale = 1.15f, kHudSecondaryScale = 1.08f;
constexpr float kHgR = 0.30f, kHgG = 1.0f, kHgB = 0.40f;
constexpr float kMToFtF = (float)kMToFt;   /* from core/FBUnits.h: a local copy would drift */

struct Proj { float sx, sy, zc; };   /* zc = cos(angle from boresight); zc<=0 = behind */

/* The aperture in screen pixels, at the same projector scale every conformal element uses. */
struct Aperture { float cx, cy, x0, y0, x1, y1; };

float Wrap180(float d) {
  while (d > 180.f) d -= 360.f;
  while (d < -180.f) d += 360.f;
  return d;
}

void DashedLine(Systems::FBHudGeometry &out, float x0, float y0, float x1, float y1, int n, float r, float g, float b) {
  for (int i = 0; i < n; i++) {
    float t0 = (float)i / (float)n, t1 = ((float)i + 0.5f) / (float)n;
    out.Line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, r, g, b);
  }
}

/* "6,020"-style thousands comma; the sign works out because C++ division and modulo both truncate
 * toward zero. */
void PrintThousands(Systems::FBHudGeometry &out, float x, float y, float s, float r, float g, float b, int v) {
  if (v <= -1000 || v >= 1000) out.Printf(x, y, s, r, g, b, "%d,%03d", v / 1000, v % 1000 < 0 ? -(v % 1000) : v % 1000);
  else out.Printf(x, y, s, r, g, b, "%d", v);
}

/* Ray from the aperture centre through (px,py), pulled back to the rectangle's own boundary — so the
 * out-of-view clamp lands exactly ON the window edge instead of on an independent ring. The inset
 * shrinks it by a symbol's half-size, so its strokes are not scissored in half. */
void ClampToRect(const Aperture &ap, float px, float py, float insetX, float insetY, float &ox, float &oy) {
  float dx = px - ap.cx, dy = py - ap.cy;
  float halfW = 0.5f * (ap.x1 - ap.x0) - insetX, halfH = 0.5f * (ap.y1 - ap.y0) - insetY;
  float tx = fabsf(dx) > 1e-4f ? halfW / fabsf(dx) : 1e6f;
  float ty = fabsf(dy) > 1e-4f ? halfH / fabsf(dy) : 1e6f;
  float t = tx < ty ? tx : ty;
  ox = ap.cx + dx * t;
  oy = ap.cy + dy * t;
}
} // namespace

/* Every conformal element goes through ONE az/el projector built from the same camera basis the generic
 * HUD's horizon uses; "az/el" is WORLD-referenced (0=north, +el=up), so a world direction needs no
 * body-frame composition. Everything is then cropped to the combiner APERTURE — the pilot's own
 * eye-relative window, not the render target. Element table: doc/modules-f16.md §12.2. */
void FBF16Hud::BuildHud(const FBState &state, const Systems::FBHudEnv &env, Systems::FBHudGeometry &out) const {
  out.Reset();
  float cx = 0.5f * (float)env.Width, cy = 0.5f * (float)env.Height;
  if (!env.Have) {
    out.Text(cx - 60 * kHudMagnify, 30 * kHudMagnify, 3 * kHudMagnify, 1, 0.8f, 0.2f, "NO TELEMETRY");
    return;
  }

  static const float kEyeOrigin[3] = {0, 0, 0};
  FBCameraBasis cam = FBCameraBasisFrom(state.Platform.YawDeg, state.Platform.PitchDeg, state.Platform.RollDeg, kEyeOrigin, kHudFovDeg,
                           (float)env.Width / (float)env.Height, 1.f, 1000.f);
  float Kc = ((float)env.Height * 0.5f) / tanf(kHudFovDeg * 0.5f * kRad);

  auto Project = [&](float azDeg, float elDeg) -> Proj {
    float az = azDeg * kRad, el = elDeg * kRad;
    float d[3] = {cosf(el) * sinf(az), sinf(el), -cosf(el) * cosf(az)};
    float xc = d[0] * cam.sr[0] + d[1] * cam.sr[1] + d[2] * cam.sr[2];
    float yc = d[0] * cam.up[0] + d[1] * cam.up[1] + d[2] * cam.up[2];
    float zc = d[0] * cam.f[0] + d[1] * cam.f[1] + d[2] * cam.f[2];
    float zcs = zc > 0.05f ? zc : 0.05f;
    return {cx + Kc * xc / zcs, cy - Kc * yc / zcs, zc};
  };

  Aperture ap;
  ap.cx = cx; ap.cy = cy;
  /* Kc stays the real scene-FOV projector, so positions stay conformal; kHudMagnify only inflates the
   * WINDOW they are cropped to. */
  float winHalfW = Kc * tanf(kApertureHalfWidthDeg * kRad) * kHudMagnify;
  float winHalfH = Kc * tanf(kApertureHalfHeightDeg * kRad) * kHudMagnify;
  ap.x0 = cx - winHalfW; ap.x1 = cx + winHalfW;
  ap.y0 = cy - winHalfH; ap.y1 = cy + winHalfH;

  /* ===== Conformal group: horizon, pitch ladder, FPM — all scissored to the aperture. ===== */
  out.SetClip(ap.x0, ap.y0, ap.x1, ap.y1);

  /* ----- Horizon: two segments flanking the boresight, gap for the FPM/ladder. ----- */
  {
    float dip = FBHorizonDipRad(state.Platform.AltM > 1 ? state.Platform.AltM : env.Agl) * kR2D;
    Proj p0 = Project(state.Platform.YawDeg, -dip), p1 = Project(state.Platform.YawDeg + 20.f, -dip);
    float ddx = p1.sx - p0.sx, ddy = p1.sy - p0.sy, LL = sqrtf(ddx * ddx + ddy * ddy);
    if (LL > 1.f) {
      float ux = ddx / LL, uy = ddy / LL, mx = p0.sx, my = p0.sy;
      float half = 200.f * kHudMagnify, gap = 12.f * kHudMagnify;
      out.QLine(mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap, 1.0f, kHgR, kHgG, kHgB);
      out.QLine(mx + ux * gap, my + uy * gap, mx + ux * half, my + uy * half, 1.0f, kHgR, kHgG, kHgB);
    }
  }

  /* ----- Pitch ladder: earth-referenced bars every 5 deg, positive solid / negative dashed
   * (MIL-STD-1787), compacted so a full bar's two segments fit the window's width. ----- */
  {
    /* gapDeg/outerDeg are stylistic azimuth SPREAD, not a physical quantity — only Ldeg carries real
     * pitch — so kHudMagnify scales them like any other screen length. */
    const float gapDeg = 1.0f * kHudMagnify, outerDeg = 4.5f * kHudMagnify, tickLen = 3.f * kHudMagnify;
    for (int Ldeg = -30; Ldeg <= 30; Ldeg += 5) {
      if (Ldeg == 0) continue;
      bool dashed = Ldeg < 0;
      float towardHorizonDeg = Ldeg > 0 ? (float)Ldeg - 2.f : (float)Ldeg + 2.f;
      for (int side = -1; side <= 1; side += 2) {
        Proj inner = Project(state.Platform.YawDeg + (float)side * gapDeg, (float)Ldeg);
        Proj outer = Project(state.Platform.YawDeg + (float)side * outerDeg, (float)Ldeg);
        if (dashed) DashedLine(out, inner.sx, inner.sy, outer.sx, outer.sy, 3, kHgR, kHgG, kHgB);
        else out.Line(inner.sx, inner.sy, outer.sx, outer.sy, kHgR, kHgG, kHgB);

        Proj toward = Project(state.Platform.YawDeg + (float)side * outerDeg, towardHorizonDeg);
        float tdx = toward.sx - outer.sx, tdy = toward.sy - outer.sy, tl = sqrtf(tdx * tdx + tdy * tdy);
        if (tl > 0.5f) {
          float ux = tdx / tl, uy = tdy / tl;
          out.Line(outer.sx, outer.sy, outer.sx + ux * tickLen, outer.sy + uy * tickLen, kHgR, kHgG, kHgB);
        }
        out.Printf(outer.sx + (side > 0 ? 2.f : -12.f) * kHudMagnify, outer.sy - 3.f * kHudMagnify,
                   kHudSecondaryScale * kHudMagnify, kHgR, kHgG, kHgB, "%d", Ldeg > 0 ? Ldeg : -Ldeg);
      }
    }
  }

  /* ----- FPM: the MIL-STD-1787 Aircraft Reference Symbol at the velocity vector. Computed here rather
   * than at its draw site because the tadpole below re-uses its anchor. ----- */
  Proj fpm = Project(state.AirData.TrackDeg, state.AirData.FpaDeg);
  {
    float fx = fpm.sx, fy = fpm.sy, mg = kHudMagnify;
    out.Circle(fx, fy, 4.f * mg, 16, kHgR, kHgG, kHgB);
    out.Line(fx - 10 * mg, fy, fx - 4 * mg, fy, kHgR, kHgG, kHgB);
    out.Line(fx + 4 * mg, fy, fx + 10 * mg, fy, kHgR, kHgG, kHgB);
    out.Line(fx, fy + 4 * mg, fx, fy + 8 * mg, kHgR, kHgG, kHgB);
  }

  out.ClearClip();

  /* ----- Heading tape, TOP of the aperture, ticks pointing down: magnetic (MagVar is a 0 deg
   * placeholder until a declination model exists). ----- */
  {
    float mg = kHudMagnify;
    float hdg = state.Platform.YawDeg - state.Nav.MagVarDeg;
    hdg = hdg < 0 ? hdg + 360.f : (hdg >= 360.f ? hdg - 360.f : hdg);
    float hy1 = ap.y0 + 15.f * mg;                       /* rail y, near the aperture's top edge */
    float halfSpan = winHalfW - 12.f * mg, hpd = 3.2f * mg;   /* px/deg */
    for (int hh = (int)floorf((hdg - halfSpan / hpd) / 5.f) * 5; hh <= (int)(hdg + halfSpan / hpd); hh += 5) {
      float sx = cx + ((float)hh - hdg) * hpd;
      if (sx < ap.x0 || sx > ap.x1) continue;
      int hn = ((hh % 360) + 360) % 360;
      float tk = (hn % 10 == 0) ? 5.f : 3.f;
      tk *= mg;
      out.Line(sx, hy1, sx, hy1 + tk, kHgR, kHgG, kHgB);
      if (hn % 30 == 0) {
        char nb[4];
        const char *label = nb;
        if (hn == 0) label = "N";
        else if (hn == 90) label = "E";
        else if (hn == 180) label = "S";
        else if (hn == 270) label = "W";
        else snprintf(nb, sizeof nb, "%02d", hn / 10);
        out.Text(sx - (label[1] ? 5.f : 2.f) * mg, hy1 + tk + 2.f * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB, label);
      }
    }
    out.Line(cx - halfSpan, hy1, cx + halfSpan, hy1, kHgR, kHgG, kHgB);
    out.Box(cx - 13 * mg, hy1 - 4 * mg, cx + 13 * mg, hy1 + 4 * mg, kHgR, kHgG, kHgB);
    out.Printf(cx - 10 * mg, hy1 - 3 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "%03.0f", hdg);
  }

  /* ----- Bank-angle scale below the FPM: fixed ticks + a pointer rotating with roll, clamped +-45 deg.
   * Offset and radius are fractions of the half-aperture-height (§12.2). ----- */
  {
    float mg = kHudMagnify;
    /* R/by already carry kHudMagnify through winHalfH; only the literal tick length needs its own. */
    float bx = cx, by = cy + 0.192f * winHalfH, R = 0.385f * winHalfH, tk = 4.f * mg;
    static const float marks[] = {0, 10, 20, -10, -20};
    static const float longMarks[] = {30, -30, 45, -45};
    for (float m : marks) {
      float a = m * kRad;
      out.Line(bx + sinf(a) * R, by - cosf(a) * R, bx + sinf(a) * (R + tk * 0.6f), by - cosf(a) * (R + tk * 0.6f), kHgR, kHgG, kHgB);
    }
    for (float m : longMarks) {
      float a = m * kRad;
      out.Line(bx + sinf(a) * R, by - cosf(a) * R, bx + sinf(a) * (R + tk), by - cosf(a) * (R + tk), kHgR, kHgG, kHgB);
    }
    float roll = state.Platform.RollDeg < -45.f ? -45.f : (state.Platform.RollDeg > 45.f ? 45.f : state.Platform.RollDeg), a = -roll * kRad;
    float px = bx + sinf(a) * R, py = by - cosf(a) * R;
    float nx = sinf(a), ny = -cosf(a), tx = -ny, ty = nx;
    out.Line(px, py, px + (nx * 5.f + tx * 2.5f) * mg, py + (ny * 5.f + ty * 2.5f) * mg, kHgR, kHgG, kHgB);
    out.Line(px, py, px + (nx * 5.f - tx * 2.5f) * mg, py + (ny * 5.f - ty * 2.5f) * mg, kHgR, kHgG, kHgB);
  }

  /* ----- G-load, top-left. THE VALIDITY RULE (§12.4): every readout asks its source block's head
   * first, because a pilot must be able to tell a FAILED sensor from a quiet one — 1.0 g and zero
   * knots are not what a dead ADC means. ----- */
  const bool airDataOk = state.AirData.H.Readable();
  if (airDataOk)
    out.Printf(ap.x0 + 2.f * kHudMagnify, ap.y0 + 2.f * kHudMagnify, kHudSecondaryScale * kHudMagnify, kHgR, kHgG, kHgB, "%.1f", state.AirData.GLoad);
  else
    out.Text(ap.x0 + 2.f * kHudMagnify, ap.y0 + 2.f * kHudMagnify, kHudSecondaryScale * kHudMagnify, kHgR, kHgG, kHgB, "-.-");

  /* ----- Left status block: NAV, Mach, Peak-G, ARM/SIM, bullseye. ARM/SIM is a documented ADDITION —
   * the reference layout has no counterpart for it at this position. ----- */
  {
    float lx = ap.x0 + 2.f * kHudMagnify, ls = 8.f * kHudMagnify, s = kHudSecondaryScale * kHudMagnify;
    float ly = cy + 0.136f * winHalfH;
    out.Text(lx, ly, s, kHgR, kHgG, kHgB, "NAV");
    if (airDataOk) {
      out.Printf(lx, ly + ls, s, kHgR, kHgG, kHgB, "%.2f", state.AirData.Mach);
      out.Printf(lx, ly + 2 * ls, s, kHgR, kHgG, kHgB, "%.1f", state.AirData.GLoadPeak);
    } else {
      out.Text(lx, ly + ls, s, kHgR, kHgG, kHgB, "-.--");
      out.Text(lx, ly + 2 * ls, s, kHgR, kHgG, kHgB, "-.-");
    }
    out.Text(lx, ly + 3 * ls, s, kHgR, kHgG, kHgB, state.Stores.Arm == FBArmState::Arm ? "ARM" : "SIM");
    out.Printf(lx, ly + 4 * ls, s, kHgR, kHgG, kHgB, "%03d %02.0f",
              ((int)(state.Nav.BullBearingDeg + 0.5f) % 360 + 360) % 360, state.Nav.BullDistNm);
  }

  /* ----- Right status block: R (radar altitude, a documented ADDITION), AL, 'B' slant range, TTG,
   * distance>steerpoint. A HELD block still shows its value — deliberately frozen is not broken, and
   * blanking it would throw away information the pilot is entitled to. ----- */
  {
    float rx = ap.x1 - 46.f * kHudMagnify, ls = 8.f * kHudMagnify, s = kHudSecondaryScale * kHudMagnify;
    float ry = cy + 0.136f * winHalfH;
    if (state.RadarAlt.H.Readable()) out.Printf(rx, ry, s, kHgR, kHgG, kHgB, "R%4.0f", state.RadarAlt.AglFt);
    else out.Text(rx, ry, s, kHgR, kHgG, kHgB, "R----");
    if (state.Ufc.H.Readable()) out.Printf(rx, ry + ls, s, kHgR, kHgG, kHgB, "AL%3.0f", state.Ufc.AlowFt);
    else out.Text(rx, ry + ls, s, kHgR, kHgG, kHgB, "AL---");
    if (state.FireControl.H.Readable())
      out.Printf(rx, ry + 2 * ls, s, kHgR, kHgG, kHgB, "%c%05.1f",
                 state.FireControl.RangeProvider ? state.FireControl.RangeProvider : 'B',
                 state.FireControl.SteerSlantNm);
    else out.Text(rx, ry + 2 * ls, s, kHgR, kHgG, kHgB, "B---.-");
    if (state.Cruise.H.Readable()) {
      int ttgM = (int)(state.Cruise.SteerTtgS / 60.f), ttgS = (int)state.Cruise.SteerTtgS % 60;
      out.Printf(rx, ry + 3 * ls, s, kHgR, kHgG, kHgB, "%03d:%02d", ttgM, ttgS);
    } else out.Text(rx, ry + 3 * ls, s, kHgR, kHgG, kHgB, "---:--");
    if (state.Nav.H.Readable())
      out.Printf(rx, ry + 4 * ls, s, kHgR, kHgG, kHgB, "%03.0f>%02d", state.Nav.SteerDistNm, state.Ufc.SteerNum);
    else out.Text(rx, ry + 4 * ls, s, kHgR, kHgG, kHgB, "---> --");
  }

  /* ----- Airspeed tape (CAS): minor ticks every 20 kt, boxed exact value, inset from the aperture
   * edge. Numeric tick labels dropped — no room at this scale, and the box carries the value. ----- */
  {
    float mg = kHudMagnify;
    float ax = ap.x0 + 0.08f * (ap.x1 - ap.x0), as = state.AirData.CasKt;
    float tapeHalf = 20.f * mg, pxPerKt = 0.55f * mg;
    /* A dead ADC has no scale to move: frame and box stay (the instrument is there), ticks do not. */
    if (airDataOk) {
      for (int av = (int)floorf((as - tapeHalf / pxPerKt) / 20.f) * 20; av <= (int)(as + tapeHalf / pxPerKt); av += 20) {
        if (av < 0) continue;
        float sy = cy - ((float)av - as) * pxPerKt;
        if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
        float tk = (av % 100 == 0) ? 5.f : 3.f;
        out.Line(ax, sy, ax + tk * mg, sy, kHgR, kHgG, kHgB);
      }
    }
    out.Line(ax, cy - tapeHalf, ax, cy + tapeHalf, kHgR, kHgG, kHgB);
    out.Box(ax + 2 * mg, cy - 5 * mg, ax + 24 * mg, cy + 5 * mg, kHgR, kHgG, kHgB);
    if (airDataOk) out.Printf(ax + 4 * mg, cy - 3 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "%3.0f", as);
    else out.Text(ax + 4 * mg, cy - 3 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "---");
  }

  /* ----- Altitude tape (barometric ASL): minor ticks every 100 ft, boxed thousands-comma'd value,
   * inset to mirror the CAS tape. ----- */
  {
    float mg = kHudMagnify;
    float axr = ap.x1 - 0.08f * (ap.x1 - ap.x0), asl = state.Platform.AltM * kMToFtF;
    float tapeHalf = 20.f * mg, pxPerFt = 0.03f * mg;
    for (int av = (int)floorf((asl - tapeHalf / pxPerFt) / 100.f) * 100; av <= (int)(asl + tapeHalf / pxPerFt); av += 100) {
      float sy = cy - ((float)av - asl) * pxPerFt;
      if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
      float tk = (av % 500 == 0) ? 5.f : 3.f;
      out.Line(axr, sy, axr - tk * mg, sy, kHgR, kHgG, kHgB);
    }
    out.Line(axr, cy - tapeHalf, axr, cy + tapeHalf, kHgR, kHgG, kHgB);
    /* wide enough for a 6-char "10,020": 6 * advance(4 * 2.162) ~= 52 px < 32*mg */
    out.Box(axr - 34 * mg, cy - 5 * mg, axr - 2 * mg, cy + 5 * mg, kHgR, kHgG, kHgB);
    PrintThousands(out, axr - 32 * mg, cy - 3 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, (int)asl);
  }

  /* ===== Steerpoint diamond + tadpole: conformal, so back under the aperture clip. ===== */
  out.SetClip(ap.x0, ap.y0, ap.x1, ap.y1);
  /* No nav solution, no steering symbology: a diamond from an unwritten block would point the pilot at
   * a steerpoint that does not exist (MIL-STD-1787's declutter rule). A BFM mission is exactly this. */
  if (state.Nav.H.Readable()) {
    Proj sp = Project(state.Nav.SteerBearingDeg, state.Nav.SteerElevAngleDeg);
    bool outOfFov = sp.zc < 0.05f || sp.sx < ap.x0 || sp.sx > ap.x1 || sp.sy < ap.y0 || sp.sy > ap.y1;
    float dw = 5.f * kHudMagnify, dh = 4.5f * kHudMagnify;
    float px = sp.sx, py = sp.sy;
    if (outOfFov) ClampToRect(ap, sp.sx, sp.sy, dw, dh, px, py);
    out.Line(px - dw, py, px, py - dh, kHgR, kHgG, kHgB);
    out.Line(px, py - dh, px + dw, py, kHgR, kHgG, kHgB);
    out.Line(px + dw, py, px, py + dh, kHgR, kHgG, kHgB);
    out.Line(px, py + dh, px - dw, py, kHgR, kHgG, kHgB);
    if (outOfFov) {
      out.Line(px - dw, py - dh, px + dw, py + dh, kHgR, kHgG, kHgB);
      out.Line(px - dw, py + dh, px + dw, py - dh, kHgR, kHgG, kHgB);
    }

    /* Tadpole: near the FPM, X clamped to the aperture half-width, rotated so it points UP when the
     * steerpoint is ahead of track and DOWN when behind [DOC]. */
    float mg = kHudMagnify;
    float relBrg = Wrap180(state.Nav.SteerBearingDeg - state.AirData.TrackDeg);
    float clampX = winHalfW - 12.f * mg;
    float tx = relBrg * 1.2f * mg;
    tx = tx < -clampX ? -clampX : (tx > clampX ? clampX : tx);
    float tpx = fpm.sx + tx, tpy = fpm.sy;
    float ar = relBrg * kRad, sA = sinf(ar), cA = cosf(ar);
    auto Rot = [&](float lx, float ly, float &ox, float &oy) { ox = tpx + lx * cA - ly * sA; oy = tpy + lx * sA + ly * cA; };
    float tipx, tipy, basex, basey, lhx, lhy, rhx, rhy;
    Rot(0, -6 * mg, tipx, tipy); Rot(0, 4 * mg, basex, basey);
    Rot(-2.5f * mg, -2.5f * mg, lhx, lhy); Rot(2.5f * mg, -2.5f * mg, rhx, rhy);
    out.Line(basex, basey, tipx, tipy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, lhx, lhy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, rhx, rhy, kHgR, kHgG, kHgB);
  }
  out.ClearClip();
}

} // namespace FlightBox::Modules
