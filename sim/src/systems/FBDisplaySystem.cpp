#include "FBDisplaySystem.h"
#include "FBCamera.h"
#include <cmath>
#include <cstdio>

namespace FlightBox::Systems {

namespace {
constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kHudFovDeg = 80.0f;
/* Monochromes HUD-Gruen (MIL-STD-1787). */
constexpr float kHgR = 0.30f, kHgG = 1.0f, kHgB = 0.40f;
} // namespace

void FBDisplaySystem::BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const {
  out.Reset();
  float cx = 0.5f * (float)env.Width, cy = 0.5f * (float)env.Height;

  /* Waterline/Boresight: die FESTE Zellenreferenz, bildschirmfest. */
  out.Line(cx - 28, cy, cx - 10, cy, kHgR, kHgG, kHgB);
  out.Line(cx + 10, cy, cx + 28, cy, kHgR, kHgG, kHgB);
  out.Line(cx - 10, cy, cx, cy + 7, kHgR, kHgG, kHgB);
  out.Line(cx, cy + 7, cx + 10, cy, kHgR, kHgG, kHgB);
  if (!env.Have) {
    out.Text(cx - 60, 30, 3, 1, 0.8f, 0.2f, "NO TELEMETRY");
    return;
  }

  /* Konformer Horizont, gekippt durch DIESELBE Kameraprojektion wie die Szene. Der Dip haengt an der
   * Hoehe ueber der Kruemmungsreferenz (ASL), NICHT an AGL — sonst atmet er mit dem Gelaenderelief. */
  {
    static const float kEyeOrigin[3] = {0, 0, 0};
    FBCameraBasis cam = FBCameraBasisFrom(state.Platform.YawDeg, state.Platform.PitchDeg, state.Platform.RollDeg, kEyeOrigin, kHudFovDeg,
                             (float)env.Width / (float)env.Height, 1.f, 1000.f);
    float Kc = ((float)env.Height * 0.5f) / tanf(kHudFovDeg * 0.5f * kRad), p[2][2];
    float dip = FBHorizonDipRad(state.Platform.AltM > 1 ? state.Platform.AltM : env.Agl), cd = cosf(dip), sd = sinf(dip);
    for (int k = 0; k < 2; k++) {
      float az = (state.Platform.YawDeg + (k ? 20.f : 0.f)) * kRad;
      float d[3] = {cd * sinf(az), -sd, -cd * cosf(az)};
      float xc = d[0] * cam.sr[0] + d[1] * cam.sr[1] + d[2] * cam.sr[2];
      float yc = d[0] * cam.up[0] + d[1] * cam.up[1] + d[2] * cam.up[2];
      float zc = d[0] * cam.f[0] + d[1] * cam.f[1] + d[2] * cam.f[2];
      if (zc < 0.05f) zc = 0.05f;
      p[k][0] = cx + Kc * xc / zc;
      p[k][1] = cy - Kc * yc / zc;
    }
    float ddx = p[1][0] - p[0][0], ddy = p[1][1] - p[0][1], LL = sqrtf(ddx * ddx + ddy * ddy);
    if (LL > 1.f) {
      float ux = ddx / LL, uy = ddy / LL, mx = p[0][0], my = p[0][1], half = 86.f, gap = 36.f;
      out.QLine(mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap, 1.0f, kHgR, kHgG, kHgB);
      out.QLine(mx + ux * gap, my + uy * gap, mx + ux * half, my + uy * half, 1.0f, kHgR, kHgG, kHgB);
    }
  }
  float hdg = state.Platform.YawDeg < 0 ? state.Platform.YawDeg + 360 : state.Platform.YawDeg;

  /* Heading-Tape. HomeBearingDeg ist nasenrelativ, sitzt auf einem nasenzentrierten Tape also direkt
   * als Versatz von der Mitte. */
  {
    float hpd = 5.f, hy1 = 40;
    for (int hh = (int)floorf((hdg - 45.f) / 5.f) * 5; hh <= (int)hdg + 45; hh += 5) {
      float sx = cx + ((float)hh - hdg) * hpd;
      if (sx < cx - 202.f || sx > cx + 202.f) continue;
      int hn = ((hh % 360) + 360) % 360;
      float tk = (hn % 30 == 0) ? 11.f : 6.f;
      out.Line(sx, hy1, sx, hy1 - tk, kHgR, kHgG, kHgB);
      if (hn % 30 == 0) {
        char nb[4];
        const char *label = nb;
        if (hn == 0) label = "N";
        else if (hn == 90) label = "E";
        else if (hn == 180) label = "S";
        else if (hn == 270) label = "W";
        else snprintf(nb, sizeof nb, "%02d", hn / 10);
        out.Text(sx - (label[1] ? 7.f : 3.f), hy1 - tk - 15, 2.f, kHgR, kHgG, kHgB, label);
      }
    }
    out.Line(cx - 200, hy1, cx + 200, hy1, kHgR, kHgG, kHgB);
    out.Line(cx - 7, hy1 + 7, cx, hy1, kHgR, kHgG, kHgB);
    out.Line(cx, hy1, cx + 7, hy1 + 7, kHgR, kHgG, kHgB); /* up-caret */
    out.Box(cx - 26, hy1 + 8, cx + 26, hy1 + 30, kHgR, kHgG, kHgB);
    out.Printf(cx - 22, hy1 + 13, 2.f, kHgR, kHgG, kHgB, "%03.0f", hdg);
    float hb = state.Platform.HomeBearingDeg;
    if (hb > 44) hb = 44;
    if (hb < -44) hb = -44;
    float hsx = cx + hb * hpd;
    out.Line(hsx, hy1 - 1, hsx - 6, hy1 - 11, kHgR, kHgG, kHgB);
    out.Line(hsx, hy1 - 1, hsx + 6, hy1 - 11, kHgR, kHgG, kHgB);
    out.Line(hsx - 6, hy1 - 11, hsx + 6, hy1 - 11, kHgR, kHgG, kHgB);
    out.Text(hsx - 5.5f, hy1 - 25, 1.4f, kHgR, kHgG, kHgB, "SP");
  }

  /* Groundspeed-Tape (links). */
  {
    float apx = 5.f, ax = 70.f, as = state.Platform.GsMs;
    for (int av = (int)floorf((as - 30.f) / 5.f) * 5; av <= (int)as + 30; av += 5) {
      if (av < 0) continue;
      float sy = cy - ((float)av - as) * apx;
      if (sy < cy - 150.f || sy > cy + 150.f) continue;
      float tk = (av % 10 == 0) ? 11.f : 6.f;
      out.Line(ax, sy, ax - tk, sy, kHgR, kHgG, kHgB);
      if (av % 10 == 0) out.Printf(ax - tk - 26, sy - 4, 1.7f, kHgR, kHgG, kHgB, "%3d", av);
    }
    out.Line(ax, cy - 150, ax, cy + 150, kHgR, kHgG, kHgB);
    out.Box(ax + 3, cy - 11, ax + 63, cy + 11, kHgR, kHgG, kHgB);
    out.Printf(ax + 9, cy - 7, 2.f, kHgR, kHgG, kHgB, "%3.0f", as);
    out.Line(ax, cy, ax + 3, cy - 6, kHgR, kHgG, kHgB);
    out.Line(ax, cy, ax + 3, cy + 6, kHgR, kHgG, kHgB); /* caret at the rail */
    out.Text(ax + 3, cy - 30, 1.4f, kHgR, kHgG, kHgB, "GS");
  }

  /* Hoehen-Tape (rechts), darunter AGL und VS. */
  {
    float mpx = 1.5f, axr = (float)env.Width - 70.f, asl = state.Platform.AltM;
    for (int av = (int)floorf((asl - 100.f) / 10.f) * 10; av <= (int)asl + 100; av += 10) {
      if (av < 0) continue;
      float sy = cy - ((float)av - asl) * mpx;
      if (sy < cy - 150.f || sy > cy + 150.f) continue;
      float tk = (av % 20 == 0) ? 11.f : 6.f;
      out.Line(axr, sy, axr + tk, sy, kHgR, kHgG, kHgB);
      if (av % 20 == 0) out.Printf(axr + tk + 3, sy - 4, 1.6f, kHgR, kHgG, kHgB, "%3d", av);
    }
    out.Line(axr, cy - 150, axr, cy + 150, kHgR, kHgG, kHgB);
    out.Box(axr - 63, cy - 11, axr - 3, cy + 11, kHgR, kHgG, kHgB);
    out.Printf(axr - 58, cy - 7, 2.f, kHgR, kHgG, kHgB, "%3.0f", asl);
    out.Line(axr, cy, axr - 3, cy - 6, kHgR, kHgG, kHgB);
    out.Line(axr, cy, axr - 3, cy + 6, kHgR, kHgG, kHgB); /* < caret at the rail */
    out.Text(axr - 58, cy - 30, 1.4f, kHgR, kHgG, kHgB, "ASL");
    out.Printf(axr - 63, cy + 18, 1.6f, kHgR, kHgG, kHgB, "AGL%4.0f", env.Agl);
    out.Printf(axr - 63, cy + 36, 1.6f, kHgR, kHgG, kHgB, "VS%+4.0f", state.Platform.VsMs);
  }
}

} // namespace FlightBox::Systems
