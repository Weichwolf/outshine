#include "FBF16Hud.h"
#include "FBCamera.h"
#include <cmath>
#include <cstdio>

namespace FlightBox {

namespace {
constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kR2D = 57.29577951308232f;
constexpr float kHudFovDeg = 80.0f;
/* Combiner APERTURE -- the real F-16 HUD is a small window in front of the pilot, not the whole
 * windscreen. Two documented angular specs (doc/f16/hud-symbology.md's "Technical depth"; DTIC
 * ADA430578 TFOV note cross-checked against the GPL-2.0 FlightGear F-16 mod's Nasal/HUD/HUD_main.nas
 * `TFOV=25deg`): TFOV (Total FOV, the full cone symbology may be positioned in) ~25deg, IFOV
 * (Instantaneous FOV, what the combiner glass actually shows at one head position) ~20x13.5deg. This
 * window uses TFOV for its horizontal half-angle (matches the diamond's out-of-view clamp, which the
 * window edge now doubles as -- see the clamp below) and IFOV's aspect ratio (13.5/20) to size the
 * vertical half-angle, since the combiner is documented wider than tall, not to IFOV's raw vertical
 * number verbatim -- a "clean" derived definition rather than a second independent magic constant. */
constexpr float kApertureHalfWidthDeg = 12.5f;                                    /* TFOV/2 */
constexpr float kApertureIfovH = 20.0f, kApertureIfovV = 13.5f;                   /* IFOV, for the ratio only */
constexpr float kApertureHalfHeightDeg = kApertureHalfWidthDeg * (kApertureIfovV / kApertureIfovH);
constexpr float kHgR = 0.30f, kHgG = 1.0f, kHgB = 0.40f;
constexpr float kMToFt = 3.280839895f;

struct Proj { float sx, sy, zc; };   /* zc = cos(angle from boresight); zc<=0 = behind */

/* The aperture window in screen pixels: same projector scale (Kc) every conformal element uses, so the
 * window is exactly the disc that scale subtends at the two half-angles above. */
struct Aperture { float cx, cy, x0, y0, x1, y1; };

float Wrap180(float d) {
  while (d > 180.f) d -= 360.f;
  while (d < -180.f) d += 360.f;
  return d;
}

void DashedLine(FBHudGeometry &out, float x0, float y0, float x1, float y1, int n, float r, float g, float b) {
  for (int i = 0; i < n; i++) {
    float t0 = (float)i / (float)n, t1 = ((float)i + 0.5f) / (float)n;
    out.Line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, r, g, b);
  }
}

/* Altitude box's "6,020"-style thousands comma; %03d on the remainder handles the sign correctly since
 * C++ integer division/modulo both truncate toward zero. */
void PrintThousands(FBHudGeometry &out, float x, float y, float s, float r, float g, float b, int v) {
  if (v <= -1000 || v >= 1000) out.Printf(x, y, s, r, g, b, "%d,%03d", v / 1000, v % 1000 < 0 ? -(v % 1000) : v % 1000);
  else out.Printf(x, y, s, r, g, b, "%d", v);
}

/* Ray from the aperture centre through (px,py), pulled back to the rectangle's own boundary -- the
 * diamond's out-of-view clamp now lands exactly on the window edge instead of an independent circular
 * ring (task: "clamp ring merged with the window edge"). Degenerates to the centre point itself when
 * px==py==cx==cy (division guarded below), which never happens here (the diamond is never AT
 * boresight when clamping triggers). */
/* `inset` shrinks the effective rectangle so a symbol of that half-size clamped onto it stays FULLY
 * inside the aperture (its crossed-out strokes don't get scissored in half by SetClip below). */
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

/* F-16 HUD symbology (doc/f16/hud-symbology.md; DCS F-16C Viper Guide Part 16 p.706 as the reference
 * frame; cross-checked against the FlightGear F-16 mod's Nasal/HUD -- see the class banner). Every
 * conformal element (horizon, pitch ladder, FPM, steerpoint diamond/tadpole) goes through ONE az/el
 * projector built from the SAME camera basis (yaw/pitch/roll) the generic default HUD's horizon
 * already uses -- "az/el" here is WORLD-referenced (0=north, +el=up), so a world direction (ground
 * track, bearing to a steerpoint) needs no separate body-frame composition. All of it is then cropped
 * to the combiner APERTURE (kApertureHalfWidthDeg/kApertureHalfHeightDeg above): the pilot's own
 * eye-relative window, not the whole render target -- tapes/text/heading/bank sit AT that window's
 * edges/corners, and the conformal elements are scissored to its rectangle (FBHudGeometry::SetClip). */
void FBF16Hud::BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const {
  out.Reset();
  float cx = 0.5f * (float)env.Width, cy = 0.5f * (float)env.Height;
  if (!env.Have) {
    out.Text(cx - 60, 30, 3, 1, 0.8f, 0.2f, "NO TELEMETRY");
    return;
  }

  static const float kEyeOrigin[3] = {0, 0, 0};
  w3_cam cam = w3_cam_from(state.yaw, state.pitch, state.roll, kEyeOrigin, kHudFovDeg,
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
  float winHalfW = Kc * tanf(kApertureHalfWidthDeg * kRad), winHalfH = Kc * tanf(kApertureHalfHeightDeg * kRad);
  ap.x0 = cx - winHalfW; ap.x1 = cx + winHalfW;
  ap.y0 = cy - winHalfH; ap.y1 = cy + winHalfH;

  /* ===== Conformal group: horizon, pitch ladder, FPM -- all scissored to the aperture rectangle. ===== */
  out.SetClip(ap.x0, ap.y0, ap.x1, ap.y1);

  /* ----- Horizon: two segments flanking the boresight, gap for the FPM/ladder. ----- */
  {
    float dip = w3_horizon_dip_rad(state.alt > 1 ? state.alt : env.Agl) * kR2D;
    Proj p0 = Project(state.yaw, -dip), p1 = Project(state.yaw + 20.f, -dip);
    float ddx = p1.sx - p0.sx, ddy = p1.sy - p0.sy, LL = sqrtf(ddx * ddx + ddy * ddy);
    if (LL > 1.f) {
      float ux = ddx / LL, uy = ddy / LL, mx = p0.sx, my = p0.sy, half = 200.f, gap = 12.f;
      out.QLine(mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap, 1.0f, kHgR, kHgG, kHgB);
      out.QLine(mx + ux * gap, my + uy * gap, mx + ux * half, my + uy * half, 1.0f, kHgR, kHgG, kHgB);
    }
  }

  /* ----- Pitch ladder: earth-referenced bars every 5deg, positive solid / negative dashed (MIL-STD-1787
   * convention). Compacted to the aperture's own scale (doc/f16/hud-symbology.md's "Technical depth")
   * -- half the old azimuth spread, so a full bar's two segments fit inside the window's width; the
   * SetClip above still crops whatever pokes past it in elevation (task's expected effect: at 5deg
   * spacing, only ~1-3 rungs visible through an 8.4deg half-height window). ----- */
  {
    const float gapDeg = 1.0f, outerDeg = 4.5f, tickLen = 3.f;
    for (int Ldeg = -30; Ldeg <= 30; Ldeg += 5) {
      if (Ldeg == 0) continue;
      bool dashed = Ldeg < 0;
      float towardHorizonDeg = Ldeg > 0 ? (float)Ldeg - 2.f : (float)Ldeg + 2.f;
      for (int side = -1; side <= 1; side += 2) {
        Proj inner = Project(state.yaw + (float)side * gapDeg, (float)Ldeg);
        Proj outer = Project(state.yaw + (float)side * outerDeg, (float)Ldeg);
        if (dashed) DashedLine(out, inner.sx, inner.sy, outer.sx, outer.sy, 3, kHgR, kHgG, kHgB);
        else out.Line(inner.sx, inner.sy, outer.sx, outer.sy, kHgR, kHgG, kHgB);

        Proj toward = Project(state.yaw + (float)side * outerDeg, towardHorizonDeg);
        float tdx = toward.sx - outer.sx, tdy = toward.sy - outer.sy, tl = sqrtf(tdx * tdx + tdy * tdy);
        if (tl > 0.5f) {
          float ux = tdx / tl, uy = tdy / tl;
          out.Line(outer.sx, outer.sy, outer.sx + ux * tickLen, outer.sy + uy * tickLen, kHgR, kHgG, kHgB);
        }
        out.Printf(outer.sx + (side > 0 ? 2.f : -12.f), outer.sy - 3.f, 0.9f, kHgR, kHgG, kHgB, "%d", Ldeg > 0 ? Ldeg : -Ldeg);
      }
    }
  }

  /* ----- FPM (flight path marker): the Aircraft Reference Symbol in normal flight (MIL-STD-1787) --
   * circle + wings + tail, at the velocity vector's true ground track/flight-path angle. Computed
   * (not just drawn) inside the clip section since the tadpole below re-uses fpm.sx/sy as its own
   * anchor -- Project() itself is a pure function, unaffected by clip state. ----- */
  Proj fpm = Project(state.trackDeg, state.fpaDeg);
  {
    float fx = fpm.sx, fy = fpm.sy;
    out.Circle(fx, fy, 4.f, 16, kHgR, kHgG, kHgB);
    out.Line(fx - 10, fy, fx - 4, fy, kHgR, kHgG, kHgB);
    out.Line(fx + 4, fy, fx + 10, fy, kHgR, kHgG, kHgB);
    out.Line(fx, fy + 4, fx, fy + 8, kHgR, kHgG, kHgB);
  }

  out.ClearClip();

  /* ----- Heading tape (TOP of the APERTURE): magnetic (state.yaw - magVarDeg; magVarDeg is a 0deg
   * placeholder until a declination model exists). MOVED here from the aperture floor -- both our own
   * doc/f16/hud-symbology.md ("Heading tape | Top") and the FlightGear mod's HUD_main.nas
   * (head_mask/head_frame/head_curr all anchor at `sy*0.1`, i.e. ~10% down from the canvas TOP) agree;
   * the earlier bottom placement was a plain error, not a documented deviation. Ticks point DOWN
   * (toward the FPM/ladder), labels sit below the rail -- mirror image of the old top-of-scale layout,
   * value box still straddles the rail. ----- */
  {
    float hdg = state.yaw - state.magVarDeg;
    hdg = hdg < 0 ? hdg + 360.f : (hdg >= 360.f ? hdg - 360.f : hdg);
    float hy1 = ap.y0 + 15.f;                       /* rail y, near the aperture's top edge */
    float halfSpan = winHalfW - 12.f, hpd = 3.2f;   /* px/deg -- how much heading range the tape shows */
    for (int hh = (int)floorf((hdg - halfSpan / hpd) / 5.f) * 5; hh <= (int)(hdg + halfSpan / hpd); hh += 5) {
      float sx = cx + ((float)hh - hdg) * hpd;
      if (sx < ap.x0 || sx > ap.x1) continue;
      int hn = ((hh % 360) + 360) % 360;
      float tk = (hn % 10 == 0) ? 5.f : 3.f;
      out.Line(sx, hy1, sx, hy1 + tk, kHgR, kHgG, kHgB);
      if (hn % 30 == 0) {
        char nb[4];
        const char *label = nb;
        if (hn == 0) label = "N";
        else if (hn == 90) label = "E";
        else if (hn == 180) label = "S";
        else if (hn == 270) label = "W";
        else snprintf(nb, sizeof nb, "%02d", hn / 10);
        out.Text(sx - (label[1] ? 5.f : 2.f), hy1 + tk + 2.f, 0.65f, kHgR, kHgG, kHgB, label);
      }
    }
    out.Line(cx - halfSpan, hy1, cx + halfSpan, hy1, kHgR, kHgG, kHgB);
    out.Box(cx - 13, hy1 - 4, cx + 13, hy1 + 4, kHgR, kHgG, kHgB);
    out.Printf(cx - 10, hy1 - 3, 0.7f, kHgR, kHgG, kHgB, "%03.0f", hdg);
  }

  /* ----- Bank-angle scale (below the FPM): fixed ticks at 0/10/20/30/45deg + a pointer rotating with
   * roll (clamped +-45deg). FG's HUD_main.nas roll_lines/roll_pointer put rollPos=[0,25] and
   * rollRadius=50 against a half-canvas-height of 130px (centerOrigin to edge) -- 19.2% offset, 38.5%
   * radius; scaled onto our own half-aperture-height (winHalfH) instead of jammed at the aperture's
   * physical floor. ----- */
  {
    float bx = cx, by = cy + 0.192f * winHalfH, R = 0.385f * winHalfH, tk = 4.f;
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
    float roll = state.roll < -45.f ? -45.f : (state.roll > 45.f ? 45.f : state.roll), a = -roll * kRad;
    float px = bx + sinf(a) * R, py = by - cosf(a) * R;
    float nx = sinf(a), ny = -cosf(a), tx = -ny, ty = nx;
    out.Line(px, py, px + nx * 5.f + tx * 2.5f, py + ny * 5.f + ty * 2.5f, kHgR, kHgG, kHgB);
    out.Line(px, py, px + nx * 5.f - tx * 2.5f, py + ny * 5.f - ty * 2.5f, kHgR, kHgG, kHgB);
  }

  /* ----- G-load (top-left of the aperture) ----- */
  out.Printf(ap.x0 + 2.f, ap.y0 + 2.f, 1.0f, kHgR, kHgG, kHgB, "%.1f", state.gLoad);

  /* ----- Left status block (LEFT edge, just past vertical centre -- not jammed in the bottom corner):
   * master mode (NAV) FIRST, then Mach, Peak-G, ARM/SIM, bullseye bearing/distance. Both the vertical
   * anchor and the row ORDER now follow the FlightGear mod's window2 (armmode/submode text, TOP of its
   * stack at HUD_main.nas's y=147.7 of a 260px-tall canvas whose centre is y=130 -- 13.6% of the
   * half-height below centre) -> window7 (Mach) -> window8 (Peak-G) -> window11 (bullseye, bottom of
   * its stack); ARM/SIM has no FG-window counterpart at this position (kept, 4th row -- a documented
   * addition, not a repositioning). ----- */
  {
    float lx = ap.x0 + 2.f, ls = 8.f, s = 0.62f;
    float ly = cy + 0.136f * winHalfH;
    out.Text(lx, ly, s, kHgR, kHgG, kHgB, "NAV");
    out.Printf(lx, ly + ls, s, kHgR, kHgG, kHgB, "%.2f", state.mach);
    out.Printf(lx, ly + 2 * ls, s, kHgR, kHgG, kHgB, "%.1f", state.gLoadPeak);
    out.Text(lx, ly + 3 * ls, s, kHgR, kHgG, kHgB, state.armState == FBArmState::Arm ? "ARM" : "SIM");
    out.Printf(lx, ly + 4 * ls, s, kHgR, kHgG, kHgB, "%03d %02.0f",
              ((int)(state.bullBearingDeg + 0.5f) % 360 + 360) % 360, state.bullDistNm);
  }

  /* ----- Right status block (RIGHT edge, same vertical band as the left block): radar altitude (R),
   * ALOW floor (AL), the 'B' slant range, TTG, distance>steerpoint -- order matches FlightGear's
   * window10(TA/ALOW)->window3(slant range)->window4(TTG)->window5(nav-range>steerpoint) run; R (radar
   * altitude) has no FG-window counterpart here (FG gives it its own dedicated "radalt" text elsewhere)
   * but stays as an extra lead row -- a documented addition. ----- */
  {
    float rx = ap.x1 - 46.f, ls = 8.f, s = 0.62f;
    float ry = cy + 0.136f * winHalfH;
    out.Printf(rx, ry, s, kHgR, kHgG, kHgB, "R%4.0f", state.radarAltFt);
    out.Printf(rx, ry + ls, s, kHgR, kHgG, kHgB, "AL%3.0f", state.alowFt);
    out.Printf(rx, ry + 2 * ls, s, kHgR, kHgG, kHgB, "%c%05.1f",
              state.rangeProvider ? state.rangeProvider : 'B', state.steerSlantNm);
    int ttgM = (int)(state.steerTtgS / 60.f), ttgS = (int)state.steerTtgS % 60;
    out.Printf(rx, ry + 3 * ls, s, kHgR, kHgG, kHgB, "%03d:%02d", ttgM, ttgS);
    out.Printf(rx, ry + 4 * ls, s, kHgR, kHgG, kHgB, "%03.0f>%02d", state.steerDistNm, state.steerNum);
  }

  /* ----- Airspeed tape (LEFT side, CAS): minor ticks every 20kt, boxed exact value + "C" CAS tag,
   * inset from the aperture's own edge (FG's speed_frame box sits at ~8-29% of canvas width from the
   * left, not flush against the combiner edge -- HUD_main.nas line 357's `0.20*sx*uv_used` anchor).
   * Numeric tick labels dropped (no room at this scale) -- the box carries the exact value, same
   * simplification the altitude tape below makes. ----- */
  {
    float ax = ap.x0 + 0.08f * (ap.x1 - ap.x0), as = state.casKts, tapeHalf = 20.f, pxPerKt = 0.55f;
    for (int av = (int)floorf((as - tapeHalf / pxPerKt) / 20.f) * 20; av <= (int)(as + tapeHalf / pxPerKt); av += 20) {
      if (av < 0) continue;
      float sy = cy - ((float)av - as) * pxPerKt;
      if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
      float tk = (av % 100 == 0) ? 5.f : 3.f;
      out.Line(ax, sy, ax + tk, sy, kHgR, kHgG, kHgB);
    }
    out.Line(ax, cy - tapeHalf, ax, cy + tapeHalf, kHgR, kHgG, kHgB);
    out.Box(ax + 2, cy - 5, ax + 24, cy + 5, kHgR, kHgG, kHgB);
    out.Printf(ax + 4, cy - 3, 0.68f, kHgR, kHgG, kHgB, "%3.0f", as);
  }

  /* ----- Altitude tape (RIGHT side, barometric ASL): minor ticks every 100ft, boxed exact value,
   * thousands-comma'd ("6,020" ft), inset from the aperture edge to mirror the CAS tape (FG's alt_frame
   * anchors at ~0.80*sx*uv_used, the same ~8% margin from its edge). ----- */
  {
    float axr = ap.x1 - 0.08f * (ap.x1 - ap.x0), asl = state.alt * kMToFt, tapeHalf = 20.f, pxPerFt = 0.03f;
    for (int av = (int)floorf((asl - tapeHalf / pxPerFt) / 100.f) * 100; av <= (int)(asl + tapeHalf / pxPerFt); av += 100) {
      float sy = cy - ((float)av - asl) * pxPerFt;
      if (sy < cy - tapeHalf || sy > cy + tapeHalf) continue;
      float tk = (av % 500 == 0) ? 5.f : 3.f;
      out.Line(axr, sy, axr - tk, sy, kHgR, kHgG, kHgB);
    }
    out.Line(axr, cy - tapeHalf, axr, cy + tapeHalf, kHgR, kHgG, kHgB);
    out.Box(axr - 26, cy - 5, axr - 2, cy + 5, kHgR, kHgG, kHgB);
    PrintThousands(out, axr - 24, cy - 3, 0.6f, kHgR, kHgG, kHgB, (int)asl);
  }

  /* ===== Steerpoint diamond + tadpole -- back under the aperture clip (task: these stay conformal). The
   * cross-out gate and the clamp position both use the RECTANGULAR aperture now, not a separate circular
   * ring, so the crossed-out diamond always sits exactly ON the window's own edge (task 4). ===== */
  out.SetClip(ap.x0, ap.y0, ap.x1, ap.y1);
  {
    Proj sp = Project(state.steerBearingDeg, state.steerElevAngleDeg);
    bool outOfFov = sp.zc < 0.05f || sp.sx < ap.x0 || sp.sx > ap.x1 || sp.sy < ap.y0 || sp.sy > ap.y1;
    float dw = 5.f, dh = 4.5f;
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

    /* Tadpole: near the FPM, X clamped to the aperture's own half-width (scaled down from the old
     * screen-width-fraction clamp), rotated so it points UP when the steerpoint is ahead of track,
     * DOWN when behind (doc/f16/hud-symbology.md). */
    float relBrg = Wrap180(state.steerBearingDeg - state.trackDeg);
    float clampX = winHalfW - 12.f;
    float tx = relBrg * 1.2f;
    tx = tx < -clampX ? -clampX : (tx > clampX ? clampX : tx);
    float tpx = fpm.sx + tx, tpy = fpm.sy;
    float ar = relBrg * kRad, sA = sinf(ar), cA = cosf(ar);
    auto Rot = [&](float lx, float ly, float &ox, float &oy) { ox = tpx + lx * cA - ly * sA; oy = tpy + lx * sA + ly * cA; };
    float tipx, tipy, basex, basey, lhx, lhy, rhx, rhy;
    Rot(0, -6, tipx, tipy); Rot(0, 4, basex, basey);
    Rot(-2.5f, -2.5f, lhx, lhy); Rot(2.5f, -2.5f, rhx, rhy);
    out.Line(basex, basey, tipx, tipy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, lhx, lhy, kHgR, kHgG, kHgB);
    out.Line(tipx, tipy, rhx, rhy, kHgR, kHgG, kHgB);
  }
  out.ClearClip();
}

} // namespace FlightBox
