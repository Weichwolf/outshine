#include "FBF16Hud.h"
#include "FBCamera.h"
#include <cmath>
#include <cstdio>

namespace FlightBox::Modules {

namespace {
constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kR2D = 57.29577951308232f;
/* Conformal = the SCENE's projector. One number for both (core/FBCamera.h), never a second copy. */
constexpr float kHudFovDeg = kSceneVerticalFovDeg;
/* DIE ZIELERFASSUNGSFLAECHE IST DAS FENSTER. Der Eigner hat den Maszstab gesetzt: die oberen zwei
 * Rasterreihen sind die Scheibe, und die Symbolik fuellt sie. Frueher wurde stattdessen die
 * Kombinierer-Apertur (~25 deg TFOV) gezeichnet — ein 520x348-Rechteck mitten in einem 1280x480-Fenster.
 * Was BLEIBT, ist der Maszstab: Kc ist unveraendert der Szenen-Projektor, die Welt wird nicht gedehnt. */
constexpr float kWindowInsetPx = 10.f;
/* Fester Pixelmaszstab fuer Symbole/Ticks — er waechst NICHT mit dem Fenster, sonst wuerde das HUD auf
 * einem groszen Schirm zur Plakatwand. [SET], Nachfolger von kHudMagnify. */
constexpr float kHudScale = 1.9f;
/* Text-scale FLOORS the scale multiplies from [ABL], so the ink clears B612's own legibility ratio. */
constexpr float kHudReadoutScale = 1.15f, kHudSecondaryScale = 1.08f;
constexpr float kHgR = 0.30f, kHgG = 1.0f, kHgB = 0.40f;
constexpr float kMToFtF = (float)kMToFt;   /* from core/FBUnits.h: a local copy would drift */

struct Proj { float sx, sy, zc; };   /* zc = cos(angle from boresight); zc<=0 = behind */

/* The drawn window in screen pixels — the windscreen itself, inset. */
struct Window { float cx, cy, x0, y0, x1, y1, halfW, halfH; };

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

/* Ray from the window centre through (px,py), pulled back to the rectangle's own boundary — so the
 * out-of-view clamp lands exactly ON the window edge instead of on an independent ring. The inset
 * shrinks it by a symbol's half-size, so its strokes are not scissored in half. */
void ClampToRect(const Window &w, float px, float py, float insetX, float insetY, float &ox, float &oy) {
  float dx = px - w.cx, dy = py - w.cy;
  float halfW = w.halfW - insetX, halfH = w.halfH - insetY;
  float tx = fabsf(dx) > 1e-4f ? halfW / fabsf(dx) : 1e6f;
  float ty = fabsf(dy) > 1e-4f ? halfH / fabsf(dy) : 1e6f;
  float t = tx < ty ? tx : ty;
  ox = w.cx + dx * t;
  oy = w.cy + dy * t;
}
} // namespace

/* DER SCHNITT (Eigner, diese Runde): im HUD steht, WOMIT MAN ZIELT UND NAVIGIERT — Geschwindigkeits-
 * vektor, Nickleiter, die drei schmalen Baender, die Wegpunkt-Raute. Alles, was ZUSTAND ist, steht
 * unten in der MFD-Bank (systems/FBDisplaySystem::BuildMfd): Rollwinkel auf SYS, Bullseye/Restflugzeit/
 * Schraegentfernung auf HSD. Element-fuer-Element-Liste: doc/modules/f16/hud-symbology.md.
 * Jedes konforme Element geht durch EINEN az/el-Projektor aus derselben Kamerabasis wie die Szene;
 * "az/el" ist WELTBEZOGEN (0 = Nord, +el = oben). */
void FBF16Hud::BuildHud(const FBState &state, const Systems::FBHudEnv &env, Systems::FBHudGeometry &out) const {
  out.Reset();
  /* Der Kombinierer sitzt in der Mitte der SCHEIBE (den oberen zwei Rasterreihen), waehrend Kc unten
   * an env.Height haengt: der Projektor-Massstab ist der der Szene, und der aendert sich durch das
   * Raster nicht — die Szene wird beschnitten, nicht gestaucht. */
  float cx = 0.5f * (float)env.Width, cy = 0.5f * (float)env.ViewH;
  if (!env.Have) {
    out.Text(cx - 60 * kHudScale, 30 * kHudScale, 3 * kHudScale, 1, 0.8f, 0.2f, "NO TELEMETRY");
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

  Window win;
  win.cx = cx; win.cy = cy;
  win.x0 = kWindowInsetPx; win.x1 = (float)env.Width - kWindowInsetPx;
  win.y0 = kWindowInsetPx; win.y1 = (float)env.ViewH - kWindowInsetPx;
  win.halfW = 0.5f * (win.x1 - win.x0);
  win.halfH = 0.5f * (win.y1 - win.y0);
  const float mg = kHudScale;

  /* ===== Conformal group: horizon, pitch ladder, FPM — all scissored to the window. ===== */
  out.SetClip(win.x0, win.y0, win.x1, win.y1);

  /* ----- Horizon: two segments flanking the boresight, gap for the FPM/ladder. Long enough to cross
   * the window at any bank; the clip cuts it to size. ----- */
  {
    float dip = FBHorizonDipRad(state.Platform.AltM > 1 ? state.Platform.AltM : env.Agl) * kR2D;
    Proj p0 = Project(state.Platform.YawDeg, -dip), p1 = Project(state.Platform.YawDeg + 20.f, -dip);
    float ddx = p1.sx - p0.sx, ddy = p1.sy - p0.sy, LL = sqrtf(ddx * ddx + ddy * ddy);
    if (LL > 1.f) {
      float ux = ddx / LL, uy = ddy / LL, mx = p0.sx, my = p0.sy;
      float half = win.halfW + win.halfH, gap = 12.f * mg;
      out.QLine(mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap, 1.0f, kHgR, kHgG, kHgB);
      out.QLine(mx + ux * gap, my + uy * gap, mx + ux * half, my + uy * half, 1.0f, kHgR, kHgG, kHgB);
    }
  }

  /* ----- Pitch ladder: earth-referenced bars every 5 deg, positive solid / negative dashed
   * (MIL-STD-1787). Der azimutale SPREIZWINKEL ist eine Fensterbreite, keine physikalische Groesze:
   * er wird aus dem Fenster ZURUECKGERECHNET (atan(px/Kc)), damit die Leiter das Fenster fuellt statt
   * in seiner Mitte zu kleben. ----- */
  {
    const float gapDeg = kR2D * atanf(0.045f * win.halfW / Kc);
    const float outerDeg = kR2D * atanf(0.30f * win.halfW / Kc);
    const float tickLen = 4.f * mg;
    for (int Ldeg = -45; Ldeg <= 45; Ldeg += 5) {
      if (Ldeg == 0) continue;
      bool dashed = Ldeg < 0;
      float towardHorizonDeg = Ldeg > 0 ? (float)Ldeg - 2.f : (float)Ldeg + 2.f;
      for (int side = -1; side <= 1; side += 2) {
        Proj inner = Project(state.Platform.YawDeg + (float)side * gapDeg, (float)Ldeg);
        Proj outer = Project(state.Platform.YawDeg + (float)side * outerDeg, (float)Ldeg);
        if (dashed) DashedLine(out, inner.sx, inner.sy, outer.sx, outer.sy, 4, kHgR, kHgG, kHgB);
        else out.Line(inner.sx, inner.sy, outer.sx, outer.sy, kHgR, kHgG, kHgB);

        Proj toward = Project(state.Platform.YawDeg + (float)side * outerDeg, towardHorizonDeg);
        float tdx = toward.sx - outer.sx, tdy = toward.sy - outer.sy, tl = sqrtf(tdx * tdx + tdy * tdy);
        if (tl > 0.5f) {
          float ux = tdx / tl, uy = tdy / tl;
          out.Line(outer.sx, outer.sy, outer.sx + ux * tickLen, outer.sy + uy * tickLen, kHgR, kHgG, kHgB);
        }
        out.Printf(outer.sx + (side > 0 ? 3.f : -14.f) * mg, outer.sy - 3.f * mg,
                   kHudSecondaryScale * mg, kHgR, kHgG, kHgB, "%d", Ldeg > 0 ? Ldeg : -Ldeg);
      }
    }
  }

  /* ----- FPM: the MIL-STD-1787 Aircraft Reference Symbol at the velocity vector. Computed here rather
   * than at its draw site because the tadpole below re-uses its anchor. ----- */
  Proj fpm = Project(state.AirData.TrackDeg, state.AirData.FpaDeg);
  {
    float fx = fpm.sx, fy = fpm.sy;
    out.Circle(fx, fy, 5.f * mg, 16, kHgR, kHgG, kHgB);
    out.Line(fx - 13 * mg, fy, fx - 5 * mg, fy, kHgR, kHgG, kHgB);
    out.Line(fx + 5 * mg, fy, fx + 13 * mg, fy, kHgR, kHgG, kHgB);
    out.Line(fx, fy + 5 * mg, fx, fy + 10 * mg, kHgR, kHgG, kHgB);
  }

  out.ClearClip();

  /* ----- Heading tape, at the very TOP of the window, ticks pointing down: magnetic (MagVar is a
   * 0 deg placeholder until a declination model exists). Die Skala spannt +-35 deg ueber die volle
   * Fensterbreite — das Band steht oben, nicht bei einem Drittel. ----- */
  {
    float hdg = state.Platform.YawDeg - state.Nav.MagVarDeg;
    hdg = hdg < 0 ? hdg + 360.f : (hdg >= 360.f ? hdg - 360.f : hdg);
    float hy1 = win.y0 + 9.f * mg;                            /* rail y, at the window's top edge */
    const float bandDeg = 35.f;
    float halfSpan = win.halfW - 10.f * mg, hpd = halfSpan / bandDeg;   /* px/deg */
    for (int hh = (int)floorf((hdg - bandDeg) / 5.f) * 5; hh <= (int)(hdg + bandDeg); hh += 5) {
      float sx = cx + ((float)hh - hdg) * hpd;
      if (sx < win.x0 || sx > win.x1) continue;
      int hn = ((hh % 360) + 360) % 360;
      float tk = (hn % 10 == 0) ? 6.f : 3.5f;
      tk *= mg;
      out.Line(sx, hy1, sx, hy1 + tk, kHgR, kHgG, kHgB);
      if (hn % 10 == 0) {
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
    out.Box(cx - 15 * mg, hy1 - 13 * mg, cx + 15 * mg, hy1 - 2 * mg, kHgR, kHgG, kHgB);
    out.Printf(cx - 11 * mg, hy1 - 11 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "%03.0f", hdg);
  }

  const bool airDataOk = state.AirData.H.Readable();

  /* ----- Airspeed band (CAS) at the LEFT window edge: minor ticks every 20 kt, boxed exact value.
   * Numeric tick labels dropped — the box carries the value. ----- */
  {
    float ax = win.x0 + 26.f * mg, as = state.AirData.CasKt;
    float tapeHalf = 0.62f * win.halfH, pxPerKt = tapeHalf / 120.f;   /* +-120 kt visible */
    /* A dead ADC has no scale to move: frame and box stay (the instrument is there), ticks do not. */
    if (airDataOk) {
      for (int av = (int)floorf((as - tapeHalf / pxPerKt) / 20.f) * 20; av <= (int)(as + tapeHalf / pxPerKt); av += 20) {
        if (av < 0) continue;
        float sy = cy - ((float)av - as) * pxPerKt;
        if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
        float tk = (av % 100 == 0) ? 7.f : 4.f;
        out.Line(ax, sy, ax + tk * mg, sy, kHgR, kHgG, kHgB);
        if (av % 100 == 0)
          out.Printf(ax - 22.f * mg, sy - 3.f * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB, "%3d", av);
      }
    }
    out.Line(ax, cy - tapeHalf, ax, cy + tapeHalf, kHgR, kHgG, kHgB);
    out.Box(ax + 2 * mg, cy - 6 * mg, ax + 26 * mg, cy + 6 * mg, kHgR, kHgG, kHgB);
    if (airDataOk) out.Printf(ax + 5 * mg, cy - 4 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "%3.0f", as);
    else out.Text(ax + 5 * mg, cy - 4 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, "---");
  }

  /* ----- Altitude band (barometric ASL) at the RIGHT window edge: minor ticks every 200 ft, boxed
   * thousands-comma'd value, mirroring the CAS band. ----- */
  {
    float axr = win.x1 - 26.f * mg, asl = state.Platform.AltM * kMToFtF;
    float tapeHalf = 0.62f * win.halfH, pxPerFt = tapeHalf / 2000.f;   /* +-2000 ft visible */
    for (int av = (int)floorf((asl - tapeHalf / pxPerFt) / 200.f) * 200; av <= (int)(asl + tapeHalf / pxPerFt); av += 200) {
      float sy = cy - ((float)av - asl) * pxPerFt;
      if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
      float tk = (av % 1000 == 0) ? 7.f : 4.f;
      out.Line(axr, sy, axr - tk * mg, sy, kHgR, kHgG, kHgB);
      if (av % 1000 == 0)
        PrintThousands(out, axr + 3.f * mg, sy - 3.f * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB, av);
    }
    out.Line(axr, cy - tapeHalf, axr, cy + tapeHalf, kHgR, kHgG, kHgB);
    /* wide enough for a 6-char "10,020" at the readout scale */
    out.Box(axr - 38 * mg, cy - 6 * mg, axr - 2 * mg, cy + 6 * mg, kHgR, kHgG, kHgB);
    PrintThousands(out, axr - 35 * mg, cy - 4 * mg, kHudReadoutScale * mg, kHgR, kHgG, kHgB, (int)asl);
  }

  /* ===== Steerpoint diamond + tadpole: conformal, so back under the window clip. ===== */
  out.SetClip(win.x0, win.y0, win.x1, win.y1);
  /* No nav solution, no steering symbology: a diamond from an unwritten block would point the pilot at
   * a steerpoint that does not exist (MIL-STD-1787's declutter rule). A BFM mission is exactly this. */
  if (state.Nav.H.Readable()) {
    Proj sp = Project(state.Nav.SteerBearingDeg, state.Nav.SteerElevAngleDeg);
    bool outOfFov = sp.zc < 0.05f || sp.sx < win.x0 || sp.sx > win.x1 || sp.sy < win.y0 || sp.sy > win.y1;
    float dw = 6.f * mg, dh = 5.5f * mg;
    float px = sp.sx, py = sp.sy;
    if (outOfFov) ClampToRect(win, sp.sx, sp.sy, dw, dh, px, py);
    out.Line(px - dw, py, px, py - dh, kHgR, kHgG, kHgB);
    out.Line(px, py - dh, px + dw, py, kHgR, kHgG, kHgB);
    out.Line(px + dw, py, px, py + dh, kHgR, kHgG, kHgB);
    out.Line(px, py + dh, px - dw, py, kHgR, kHgG, kHgB);
    if (outOfFov) {
      out.Line(px - dw, py - dh, px + dw, py + dh, kHgR, kHgG, kHgB);
      out.Line(px - dw, py + dh, px + dw, py - dh, kHgR, kHgG, kHgB);
    }

    /* Tadpole: near the FPM, X clamped to the window half-width, rotated so it points UP when the
     * steerpoint is ahead of track and DOWN when behind it [DOC]. */
    float relBrg = Wrap180(state.Nav.SteerBearingDeg - state.AirData.TrackDeg);
    float clampX = win.halfW - 14.f * mg;
    float tx = relBrg * 1.2f * mg;
    tx = tx < -clampX ? -clampX : (tx > clampX ? clampX : tx);
    float tpx = fpm.sx + tx, tpy = fpm.sy;
    float ar = relBrg * kRad, sA = sinf(ar), cA = cosf(ar);
    auto Rot = [&](float lx, float ly, float &ox, float &oy) { ox = tpx + lx * cA - ly * sA; oy = tpy + lx * sA + ly * cA; };
    float tipx, tipy, basex, basey, lhx, lhy, rhx, rhy;
    Rot(0, -8 * mg, tipx, tipy); Rot(0, 5 * mg, basex, basey);
    Rot(-3.f * mg, -3.f * mg, lhx, lhy); Rot(3.f * mg, -3.f * mg, rhx, rhy);
    out.Line(basex, basey, tipx, tipy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, lhx, lhy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, rhx, rhy, kHgR, kHgG, kHgB);
  }
  out.ClearClip();

  /* ----- Die EINE Navigationszeile, unten links aus der Zielzone heraus: welcher Wegpunkt und wie
   * weit. Restflugzeit und Schraegentfernung stehen auf HSD — sie sind Planung, nicht Zielhilfe. ----- */
  {
    float s = kHudSecondaryScale * mg, ly = win.y1 - 10.f * mg, lx = win.x0 + 6.f * mg;
    if (state.Nav.H.Readable())
      out.Printf(lx, ly, s, kHgR, kHgG, kHgB, "STPT %02d  %5.1f", state.Ufc.SteerNum, (double)state.Nav.SteerDistNm);
    else out.Text(lx, ly, s, kHgR, kHgG, kHgB, "STPT --  ---.-");
  }
}

} // namespace FlightBox::Modules
