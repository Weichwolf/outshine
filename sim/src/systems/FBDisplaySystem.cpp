#include "FBDisplaySystem.h"
#include "FBCamera.h"
#include "FBHudFont.h"
#include "FBStore.h"
#include "FBUnits.h"
#include <cmath>
#include <cstdio>

namespace FlightBox::Systems {

namespace {
constexpr float kRad = 3.14159265358979323846f / 180.f;
constexpr float kHudFovDeg = kSceneVerticalFovDeg;   /* conformal means the SCENE's number, not a second one */
/* Monochromes HUD-Gruen (MIL-STD-1787). */
constexpr float kHgR = 0.30f, kHgG = 1.0f, kHgB = 0.40f;
} // namespace

void FBDisplaySystem::BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const {
  out.Reset();
  float cx = 0.5f * (float)env.Width, cy = 0.5f * (float)env.ViewH;

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

/* ---------------- die MFD-Bank: die untere Rasterreihe ----------------
 * JEDE Zahl hier kommt aus einem publizierten Block, und ein Block, dessen Kopf nicht lesbar ist,
 * wird als NO DATA gezeichnet statt als Null — dieselbe Regel, die im HUD schon gilt (§12.4).
 * Die Seiten sind absichtlich karg: was ein Geraet nicht misst, steht nicht drauf. */
namespace {
constexpr float kMfdBodyS = 2.0f;                     /* 8 px Vorschub, 12 px Zeichen */
constexpr float kMfdLineH = 15.0f;
constexpr float kMfdDimR = 0.16f, kMfdDimG = 0.52f, kMfdDimB = 0.22f;   /* Rahmen/Raster */
constexpr float kMfdWarnR = 1.0f, kMfdWarnG = 0.70f, kMfdWarnB = 0.20f;
/* DER SCHLEIER. Die Schaechte stehen ueber der Aussenansicht statt ueber Schwarz — der Eigner will
 * durchsehen. Gefordert ist LESBARKEIT, und die ist ein Kontrastverhaeltnis, also eine Rechnung:
 * linear gemischt bleibt vom Untergrund (1-a) uebrig, das hellste im Flugbild GEMESSENE Perzentil
 * (99,5 %) des Schachtuntergrunds ist L = 0,93 (weisser SVS-Boden), und HUD-Gruen (L = 0,740) braucht
 * 4,5:1 (WCAG AA, kleiner Text):
 *   (0,740+0,05) / ((1-a)*0,93 + 0,05) >= 4,5  ->  a >= 0,865.
 * Bernstein (L = 0,535) landet damit bei 3,4:1 — die Schwelle fuer GROSSE Schrift, und Bernstein
 * traegt nur kurze Warnworte. Nachgemessen im Bild: doc/render/renderer.md §2.4. */
constexpr float kMfdVeil = 0.87f;

struct Bay { float x0, y0, x1, y1; };

/* Der ANZEIGEMASSSTAB ist eine Darstellungsentscheidung, keine Messung: die kleinste Stufe, die alles
 * Publizierte fasst, und sie steht als Zahl auf dem Schirm, damit niemand sie raten muss. */
float ScopeScaleNm(float maxNm) {
  static const float steps[] = {5.f, 10.f, 20.f, 40.f, 80.f, 160.f};
  for (float s : steps)
    if (maxNm <= s) return s;
  return 160.f;
}

void NoData(FBHudGeometry &out, const Bay &b, const char *what) {
  out.Printf(b.x0 + 10.f, b.y0 + 0.5f * (b.y1 - b.y0), kMfdBodyS, kMfdWarnR, kMfdWarnG, kMfdWarnB,
             "%s NO DATA", what);
}

/* Ein Kontaktsymbol, an dem Lock, Coast und IFF haengen — zweimal gebraucht (B-Scope und Karte), also
 * einmal geschrieben. */
void ContactMark(FBHudGeometry &out, float x, float y, const FBRadarContact &c, bool locked) {
  float d = locked ? 7.f : 4.f;
  out.Box(x - d, y - d, x + d, y + d, kHgR, kHgG, kHgB);
  if (locked) out.Box(x - d - 3.f, y - d - 3.f, x + d + 3.f, y + d + 3.f, kHgR, kHgG, kHgB);
  if (c.Coasting) out.Line(x - d, y - d, x + d, y + d, kHgR, kHgG, kHgB);
  /* Der EINZIGE Identitaetstraeger im Baum: die IFF-Antwort. "Kein Rueckruf" bleibt unbeschriftet. */
  if (c.Iff == FBIffReply::Friendly) out.Printf(x + d + 2.f, y - 6.f, 1.4f, kHgR, kHgG, kHgB, "F");
}

/* DIE BODENKARTE (FCR im GM-Modus), ein Plan-Position-Indicator: Nase oben, eigene Position im
 * Scheitel, Entfernungsboegen darueber. Das BILD selbst — die Rueckstreuung Zelle fuer Zelle — kommt
 * aus FBGroundMapBlock und wird von render/stages/FBGroundMapStage unter diese Striche gelegt; hier
 * steht nur, was das Bild BEDEUTET. Ein Radarbild ist keine Landkarte: hell heisst "viel Echo"
 * (Hang zum Strahl), schwarz heisst "kein Echo" — auch dort, wo ein Kamm davorstand.
 *
 * ERFUNDEN, weil keine Quelle sie zeichnet — dieselbe Deklarationspflicht wie bei den Zielsymbolen in
 * doc/modules/f16/hud-symbology.md: dass es ein PPI ist, steht in radar-sensors.md, aber die
 * AUFTEILUNG (Sektorscheitel unten mittig, vier Entfernungsboegen bei 1/4..4/4 mit nm-Zahl innen, der
 * Antennenstrich auf der aktuellen Sweep-Peilung, "GM <nm>" links oben, "A6" links unten, die
 * ELEV-Zahl rechts unten) ist HIER entschieden. Kein ED-Screenshot ist dafuer nachgezeichnet worden. */
void PageFcrMap(FBHudGeometry &out, const Bay &b, const FBState &s) {
  const FBGroundMapBlock &m = s.GroundMap;
  FBMfdBayRect body{b.x0, b.y0, b.x1, b.y1};
  float apexX, apexY, R;
  FBFcrFan(body, m.AzHalfDeg, apexX, apexY, R);
  float azHalf = m.AzHalfDeg * kRad;
  float scaleNm = m.RangeM * (float)kMToNm;

  /* Sektorrand: die zwei Schenkel und der aeussere Bogen. */
  out.Line(apexX, apexY, apexX + sinf(-azHalf) * R, apexY - cosf(azHalf) * R, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Line(apexX, apexY, apexX + sinf(azHalf) * R, apexY - cosf(azHalf) * R, kMfdDimR, kMfdDimG, kMfdDimB);
  for (int k = 1; k <= 4; k++) {
    float rr = R * (float)k / 4.f;
    float px = 0.f, py = 0.f;
    for (int i = 0; i <= 24; i++) {
      float a = -azHalf + 2.f * azHalf * (float)i / 24.f;
      float x = apexX + sinf(a) * rr, y = apexY - cosf(a) * rr;
      if (i) out.Line(px, py, x, y, kMfdDimR, kMfdDimG, kMfdDimB);
      px = x; py = y;
    }
    out.Printf(apexX + 3.f, apexY - rr - 12.f, 1.3f, kMfdDimR, kMfdDimG, kMfdDimB, "%2.0f",
               (double)(scaleNm * (float)k / 4.f));
  }
  out.Line(apexX, apexY, apexX, apexY - 10.f, kHgR, kHgG, kHgB);   /* die Nase */
  /* Wo die Antenne GERADE steht: alles rechts davon ist die Farbe des vorigen Sweeps. */
  float sw = -azHalf + 2.f * azHalf * m.SweepFrac;
  out.Line(apexX, apexY, apexX + sinf(sw) * R, apexY - cosf(sw) * R, 0.f, 0.55f, 0.25f);

  /* Der gewaehlte Zielpunkt IM Bild: Peilung gegen die eigene Nase, Entfernung auf denselben Bogen. */
  if (s.Nav.H.Readable() && scaleNm > 0.f) {
    float rel = (s.Nav.SteerBearingDeg - s.Platform.YawDeg) * kRad;
    while (rel > 3.14159265f) rel -= 6.28318531f;
    while (rel < -3.14159265f) rel += 6.28318531f;
    float rn = s.Nav.SteerDistNm / scaleNm;
    if (fabsf(rel) <= azHalf && rn <= 1.f) {
      float x = apexX + sinf(rel) * R * rn, y = apexY - cosf(rel) * R * rn;
      out.Line(x - 6.f, y, x, y - 6.f, kHgR, kHgG, kHgB);
      out.Line(x, y - 6.f, x + 6.f, y, kHgR, kHgG, kHgB);
      out.Line(x + 6.f, y, x, y + 6.f, kHgR, kHgG, kHgB);
      out.Line(x, y + 6.f, x - 6.f, y, kHgR, kHgG, kHgB);
      out.Printf(x + 8.f, y - 6.f, 1.3f, kHgR, kHgG, kHgB, "SP%02d", s.Ufc.SteerNum);
    }
  }
  /* Die Luftkontakte desselben Satzes, in DIESER Geometrie — siehe die benannte Vereinfachung in
   * sensors/FBRadarSystem: das echte Geraet teilt seine Modi zeitlich. */
  if (s.Radar.H.Readable() && scaleNm > 0.f) {
    for (int i = 0; i < s.Radar.ContactCount; i++) {
      const FBRadarContact &c = s.Radar.Contacts[i];
      float rel = c.AzDeg * kRad, rn = c.RangeM * (float)kMToNm / scaleNm;
      if (fabsf(rel) > azHalf || rn > 1.f) continue;
      ContactMark(out, apexX + sinf(rel) * R * rn, apexY - cosf(rel) * R * rn, c, i == s.Radar.LockIndex);
    }
  }
  out.Printf(b.x0 + 6.f, b.y0 + 20.f, 1.5f, kHgR, kHgG, kHgB, "GM %2.0f", (double)scaleNm);
  out.Printf(b.x0 + 6.f, b.y1 - 16.f, 1.5f, kHgR, kHgG, kHgB, "A%d", (int)(m.AzHalfDeg / 10.f + 0.5f));
  out.Printf(b.x1 - 84.f, b.y1 - 16.f, 1.5f, kHgR, kHgG, kHgB, "ELEV%5.0f",
             (double)(m.GroundAslM * (float)kMToFt));
}

/* B-Scope: Azimut ueber die Breite, Entfernung nach oben — die Darstellung, in der ein Kontakt genau
 * die beiden Zahlen traegt, die das Geraet publiziert (AzDeg, RangeM). Die Breite ist das, was das Set
 * WIRKLICH absucht (FBRadarBlock::ScanAzHalfDeg), nicht eine feste Halbkugel: ein Kontakt am Rand des
 * Musters gehoert an den Rand des Schirms. */
void PageFcr(FBHudGeometry &out, const Bay &b, const FBState &s) {
  if (s.GroundMap.H.Readable() && s.GroundMap.Mapping) { PageFcrMap(out, b, s); return; }
  if (!s.Radar.H.Readable()) { NoData(out, b, "FCR"); return; }
  const FBRadarBlock &r = s.Radar;
  float maxNm = 0.f;
  for (int i = 0; i < r.ContactCount; i++) maxNm = fmaxf(maxNm, r.Contacts[i].RangeM * (float)kMToNm);
  float scale = ScopeScaleNm(maxNm > 0.f ? maxNm : 40.f);
  const float az = r.ScanAzHalfDeg > 1.f ? r.ScanAzHalfDeg : 60.f;
  float gx0 = b.x0 + 34.f, gx1 = b.x1 - 8.f, gy0 = b.y0 + 20.f, gy1 = b.y1 - 20.f;
  out.Box(gx0, gy0, gx1, gy1, kMfdDimR, kMfdDimG, kMfdDimB);
  for (int k = 1; k < 4; k++) {   /* Viertel-Ringe als Entfernungslinien */
    float y = gy1 - (gy1 - gy0) * (float)k / 4.f;
    out.Line(gx0, y, gx1, y, kMfdDimR, kMfdDimG, kMfdDimB);
  }
  out.Line(0.5f * (gx0 + gx1), gy0, 0.5f * (gx0 + gx1), gy1, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Printf(b.x0 + 6.f, gy0 - 2.f, 1.5f, kHgR, kHgG, kHgB, "%3.0f", scale);
  out.Printf(b.x0 + 6.f, gy1 - 10.f, 1.5f, kHgR, kHgG, kHgB, "%s", r.Radiating ? "RAD" : "SIL");
  out.Printf(gx1 - 60.f, gy1 + 4.f, 1.5f, kHgR, kHgG, kHgB, "MODE%2d", r.ModeOrdinal);
  out.Printf(gx0 + 2.f, gy0 - 2.f, 1.3f, kMfdDimR, kMfdDimG, kMfdDimB, "A%2.0f", (double)az);
  for (int i = 0; i < r.ContactCount; i++) {
    const FBRadarContact &c = r.Contacts[i];
    if (fabsf(c.AzDeg) > az) continue;
    float x = 0.5f * (gx0 + gx1) + (c.AzDeg / az) * 0.5f * (gx1 - gx0);
    float rn = c.RangeM * (float)kMToNm / scale;
    float y = gy1 - (gy1 - gy0) * (rn > 1.f ? 1.f : rn);
    ContactMark(out, x, y, c, i == r.LockIndex);
  }
  out.Printf(gx0 + 2.f, gy1 + 4.f, 1.5f, kHgR, kHgG, kHgB, "TGT%d", r.ContactCount);
}

/* Der TWA-Schirm, wie er ist: Nase oben, Radius = Bedrohlichkeit, KEINE Entfernung. */
void PageRwr(FBHudGeometry &out, const Bay &b, const FBState &s) {
  if (!s.Rwr.H.Readable()) { NoData(out, b, "RWR"); return; }
  const FBRwrBlock &w = s.Rwr;
  float cx = 0.5f * (b.x0 + b.x1), cy = 0.5f * (b.y0 + b.y1) + 4.f;
  float R = 0.5f * (b.y1 - b.y0) - 22.f;
  out.Circle(cx, cy, R, 32, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Circle(cx, cy, R * 0.5f, 24, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Line(cx, cy - R - 5.f, cx, cy - R + 5.f, kHgR, kHgG, kHgB);
  for (int i = 0; i < w.ThreatCount; i++) {
    const FBRwrThreat &t = w.Threats[i];
    float a = t.BearingDeg * kRad, rad = R * (1.0f - t.LethalityNorm);
    float x = cx + sinf(a) * rad, y = cy - cosf(a) * rad;
    out.Printf(x - 8.f, y - 6.f, 1.6f, kHgR, kHgG, kHgB, "%d", t.Id);
    if (i == w.PriorityIndex) {   /* die Raute des hoechstpriorisierten Strahlers */
      out.Line(x - 10.f, y, x, y - 10.f, kHgR, kHgG, kHgB);
      out.Line(x, y - 10.f, x + 10.f, y, kHgR, kHgG, kHgB);
      out.Line(x + 10.f, y, x, y + 10.f, kHgR, kHgG, kHgB);
      out.Line(x, y + 10.f, x - 10.f, y, kHgR, kHgG, kHgB);
    }
    if (t.Mode == FBRwrThreatMode::Missile) out.Circle(x, y, 13.f, 16, kMfdWarnR, kMfdWarnG, kMfdWarnB);
  }
  out.Printf(b.x0 + 8.f, b.y1 - 16.f, 1.5f, kHgR, kHgG, kHgB, "%s  %s",
             w.Powered ? "PWR" : "OFF", w.Activity ? "ACT" : "   ");
  if (w.MissileLaunch)
    out.Text(b.x0 + 8.f, b.y0 + 22.f, kMfdBodyS, kMfdWarnR, kMfdWarnG, kMfdWarnB, "LAUNCH");
  if (w.HiddenSearch) out.Text(b.x1 - 60.f, b.y1 - 16.f, 1.5f, kMfdDimR, kMfdDimG, kMfdDimB, "FILT");
  if (s.Cmds.H.Readable())
    out.Printf(b.x1 - 118.f, b.y0 + 22.f, 1.5f, kHgR, kHgG, kHgB, "CH%3d FL%3d",
               s.Cmds.ChaffRemaining, s.Cmds.FlareRemaining);
}

/* Die Ablage-/Bewaffnungsseite: Stationsbelegung, Freigabezustand, Rundenzahl. Genau das, was der
 * Eigner aus dem HUD herausgenommen haben will. */
void PageSms(FBHudGeometry &out, const Bay &b, const FBState &s) {
  float x = b.x0 + 10.f, y = b.y0 + 22.f;
  if (!s.Stores.H.Readable()) { NoData(out, b, "SMS"); return; }
  const FBStoresBlock &st = s.Stores;
  bool armed = st.Arm == FBArmState::Arm;
  out.Text(x, y, kMfdBodyS, armed ? kMfdWarnR : kHgR, armed ? kMfdWarnG : kHgG, armed ? kMfdWarnB : kHgB,
           armed ? "MASTER ARM" : "MASTER SAFE");
  y += kMfdLineH;
  for (int i = 0; i < st.StationCount && y < b.y1 - 26.f; i++) {
    FBStoreKind k = (FBStoreKind)st.Station[i];
    const FBStoreSpec *spec = k == FBStoreKind::None ? nullptr : FBStoreSpecOf(k);
    bool sel = (i + 1) == st.SelectedStation;
    out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "%c%d %s", sel ? '>' : ' ', i + 1,
               spec ? spec->Key : "-");
    y += kMfdLineH;
  }
  y = b.y1 - 24.f;
  out.Printf(x, y, 1.6f, kHgR, kHgG, kHgB, "LOAD%2d  REL%2d  %5.0fLB", st.LoadedCount,
             st.ReleasedCount, (double)st.LoadedLbs);
  if (s.Gun.H.Readable())
    out.Printf(x, y - 16.f, 1.6f, s.Gun.Ready ? kHgR : kMfdDimR, s.Gun.Ready ? kHgG : kMfdDimG,
               s.Gun.Ready ? kHgB : kMfdDimB, "GUN %4d", s.Gun.RoundsRemaining);
  if (st.Designating) out.Text(b.x1 - 60.f, b.y0 + 22.f, 1.6f, kMfdWarnR, kMfdWarnG, kMfdWarnB, "LASE");
}

/* Die Lage in der Ebene — der Heads-down-MODUS: eigener Kurs oben, Steuerpunkt und Datenlink-Spuren
 * als Peilung/Entfernung, so wie beide Blocks sie melden. */
void PageHsd(FBHudGeometry &out, const Bay &b, const FBState &s) {
  bool nav = s.Nav.H.Readable();
  const FBDatalinkBlock &dl = s.Datalink.H.Readable() ? s.Datalink : s.NetLink;
  bool link = dl.H.Readable();
  if (!nav && !link) { NoData(out, b, "HSD"); return; }
  float cx = 0.5f * (b.x0 + b.x1), cy = 0.5f * (b.y0 + b.y1) + 4.f;
  float R = 0.5f * (b.y1 - b.y0) - 22.f;
  out.Circle(cx, cy, R, 32, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Line(cx, cy - R - 5.f, cx, cy - R + 5.f, kHgR, kHgG, kHgB);   /* eigene Nase */
  out.Line(cx - 5.f, cy, cx + 5.f, cy, kHgR, kHgG, kHgB);
  float hdg = s.Platform.YawDeg;
  float maxNm = nav ? s.Nav.SteerDistNm : 0.f;
  if (link)
    for (int i = 0; i < dl.TrackCount; i++) maxNm = fmaxf(maxNm, dl.Tracks[i].RangeM * (float)kMToNm);
  float scale = ScopeScaleNm(maxNm > 0.f ? maxNm : 40.f);
  out.Printf(b.x0 + 6.f, b.y0 + 20.f, 1.5f, kHgR, kHgG, kHgB, "%3.0f", scale);
  if (nav) {
    float rel = (s.Nav.SteerBearingDeg - hdg) * kRad, rn = s.Nav.SteerDistNm / scale;
    float x = cx + sinf(rel) * R * (rn > 1.f ? 1.f : rn), y = cy - cosf(rel) * R * (rn > 1.f ? 1.f : rn);
    out.Line(x - 6.f, y, x, y - 6.f, kHgR, kHgG, kHgB);
    out.Line(x, y - 6.f, x + 6.f, y, kHgR, kHgG, kHgB);
    out.Line(x + 6.f, y, x, y + 6.f, kHgR, kHgG, kHgB);
    out.Line(x, y + 6.f, x - 6.f, y, kHgR, kHgG, kHgB);
    out.Printf(b.x0 + 6.f, b.y1 - 16.f, 1.5f, kHgR, kHgG, kHgB, "SP%02d %4.1f", s.Ufc.SteerNum,
               (double)s.Nav.SteerDistNm);
    /* Aus dem HUD heruntergewandert: Bullseye-Bezug, Restflugzeit, Schraegentfernung mit ihrem
     * Quellenbuchstaben. Planungszahlen gehoeren auf die Lagekarte, nicht auf die Scheibe. */
    out.Printf(b.x0 + 6.f, b.y1 - 30.f, 1.5f, kHgR, kHgG, kHgB, "BULL %03d/%02.0f",
               ((int)(s.Nav.BullBearingDeg + 0.5f) % 360 + 360) % 360, (double)s.Nav.BullDistNm);
    if (s.Cruise.H.Readable())
      out.Printf(b.x1 - 84.f, b.y1 - 30.f, 1.5f, kHgR, kHgG, kHgB, "TTG%03d:%02d",
                 (int)(s.Cruise.SteerTtgS / 60.f), (int)s.Cruise.SteerTtgS % 60);
    if (s.FireControl.H.Readable())
      out.Printf(b.x1 - 84.f, b.y0 + 20.f, 1.5f, kHgR, kHgG, kHgB, "%c%05.1f",
                 s.FireControl.RangeProvider ? s.FireControl.RangeProvider : 'B',
                 (double)s.FireControl.SteerSlantNm);
  }
  if (link) {
    for (int i = 0; i < dl.TrackCount; i++) {
      const FBDatalinkTrack &t = dl.Tracks[i];
      float rel = (t.BearingDeg - hdg) * kRad, rn = t.RangeM * (float)kMToNm / scale;
      float x = cx + sinf(rel) * R * (rn > 1.f ? 1.f : rn), y = cy - cosf(rel) * R * (rn > 1.f ? 1.f : rn);
      out.Circle(x, y, 5.f, 12, kHgR, kHgG, kHgB);
      out.Printf(x + 7.f, y - 5.f, 1.3f, kHgR, kHgG, kHgB, "%.0f", (double)t.AgeS);
    }
    out.Printf(b.x1 - 84.f, b.y1 - 16.f, 1.5f, kHgR, kHgG, kHgB, "LINK%d %s", dl.TrackCount,
               dl.Transmitting ? "XMT" : "RCV");
  }
}

/* Der passive Kopf: Winkel und sonst nichts — eine Entfernung steht nur da, wenn der Entfernungsmesser
 * eine gemessen hat (doc/sensors.md §6). Das BILD unter diesen Strichen ist die Nachtsicht und kommt
 * aus render/stages/FBNvisStage; hier steht nur die Symbologie darueber, in DERSELBEN Winkelgeometrie
 * (FBNvisRect/FBNvisAzHalfDeg), sonst laege ein Kontaktring neben seiner eigenen Quelle. */
void PageIrst(FBHudGeometry &out, const Bay &b, const FBState &s) {
  if (!s.Irst.H.Readable()) { NoData(out, b, "IRST"); return; }
  const FBIrstBlock &ir = s.Irst;
  FBMfdBayRect vid = FBNvisRect(FBMfdBayRect{b.x0, b.y0, b.x1, b.y1});
  float cx = 0.5f * (vid.X0 + vid.X1), cy = 0.5f * (vid.Y0 + vid.Y1);
  float halfW = 0.5f * (vid.X1 - vid.X0), halfH = 0.5f * (vid.Y1 - vid.Y0);
  float azHalf = FBNvisAzHalfDeg(vid), elHalf = kNvisElHalfDeg;
  float tanAz = tanf(azHalf * kRad), tanEl = tanf(elHalf * kRad);
  out.Box(vid.X0, vid.Y0, vid.X1, vid.Y1, kMfdDimR, kMfdDimG, kMfdDimB);
  out.Line(cx - 14.f, cy, cx - 4.f, cy, kHgR, kHgG, kHgB);   /* das offene Fadenkreuz */
  out.Line(cx + 4.f, cy, cx + 14.f, cy, kHgR, kHgG, kHgB);
  out.Line(cx, cy - 14.f, cx, cy - 4.f, kHgR, kHgG, kHgB);
  out.Line(cx, cy + 4.f, cx, cy + 14.f, kHgR, kHgG, kHgB);
  for (int i = 0; i < ir.ContactCount; i++) {
    const FBIrstContact &c = ir.Contacts[i];
    if (fabsf(c.AzDeg) > azHalf || fabsf(c.ElDeg) > elHalf) continue;
    float x = cx + halfW * tanf(c.AzDeg * kRad) / tanAz;
    float y = cy - halfH * tanf(c.ElDeg * kRad) / tanEl;
    out.Circle(x, y, i == ir.LockIndex ? 8.f : 5.f, 12, kHgR, kHgG, kHgB);
    if (c.HasRange) out.Printf(x + 9.f, y - 5.f, 1.3f, kHgR, kHgG, kHgB, "%4.1f", c.RangeM * kMToNm);
  }
  out.Printf(b.x0 + 10.f, b.y1 - 18.f, 1.5f, kHgR, kHgG, kHgB, "%s %s  TGT%d  MSK%d",
             ir.Powered ? "PWR" : "OFF", ir.LaserArmed ? "LSR" : "   ", ir.ContactCount,
             ir.CloudMaskedCount);
  out.Printf(b.x1 - 96.f, b.y1 - 18.f, 1.5f, kMfdDimR, kMfdDimG, kMfdDimB, "NVIS %2.0f", (double)(2.f * elHalf));
}

/* Das Flugzeug selbst: die FLUGDATEN, Sprit, Fahrwerk, Triebwerk, und der Warnsatz als das, was er ist
 * — ein Bitmuster mit einer zweiten Maske fuer "nicht beurteilbar".
 * DIE FLUGDATEN STEHEN SEIT DIESER RUNDE HIER, weil der Eigner das HUD auf ZIELERFASSUNG reduziert hat
 * (modules/f16/displays/FBF16Hud.cpp). Fahrt, Mach, Hoehe, AGL, Steigen, Kurs und g hatten vorher
 * ueberhaupt keinen zweiten Ort — sie waeren mit den Baendern verschwunden. Gestrichen heisst
 * verschoben. Die drei Bloecke werden EINZELN auf Lesbarkeit geprueft: ein toter Luftdatenrechner
 * loescht seine Zeile, nicht die Seite. */
void PageSys(FBHudGeometry &out, const Bay &b, const FBState &s) {
  float x = b.x0 + 10.f, y = b.y0 + 22.f;
  if (!s.Airframe.H.Readable()) { NoData(out, b, "SYS"); return; }
  const FBAirframeBlock &a = s.Airframe;
  if (s.AirData.H.Readable())
    out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "CAS %3.0f  M%4.2f  G%+4.1f", (double)s.AirData.CasKt,
               (double)s.AirData.Mach, (double)s.AirData.GLoad);
  else out.Text(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "CAS ---  M----  G----");
  y += kMfdLineH;
  /* Hoehe ASL kommt von der Plattform, AGL vom Radarhoehenmesser — zwei Quellen, zwei Lesbarkeiten,
   * und ein unbestromter Hoehenmesser zeigt hier keine 0 ft (doc/modules/f16 controls-commands 6.4). */
  if (s.RadarAlt.H.Readable())
    out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "ASL%6.0f  AGL%5.0f", (double)(s.Platform.AltM * kMToFt),
               (double)s.RadarAlt.AglFt);
  else
    out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "ASL%6.0f  AGL ----", (double)(s.Platform.AltM * kMToFt));
  y += kMfdLineH;
  out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "HDG %03.0f  VS%+6.0f", (double)s.Platform.YawDeg,
             (double)(s.Platform.VsMs * kMToFt * 60.0));
  y += kMfdLineH;
  out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "FUEL %5.0f LB  %3.0f%%", (double)a.FuelLbs,
             (double)a.FuelPct);
  y += kMfdLineH;
  out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "GEAR %s  SB %2.0f%%",
             a.GearPosition > 0.99f ? "DN" : (a.GearPosition < 0.01f ? "UP" : "--"),
             (double)(a.SpeedbrakeNorm * 100.f));
  y += kMfdLineH;
  out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "ENG %s  WOW %s", a.EngineRunning ? "RUN" : "OUT",
             a.WeightOnWheels ? "Y" : "N");
  y += kMfdLineH;
  /* Die Fluglage in Zahlen — Ziel der Bank-Skala, die diese Runde aus dem HUD fiel: die Nickleiter
   * traegt den Rollwinkel konform, eine zweite Anzeige davon war Zustand in der Zielzone. */
  out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "PIT %+3.0f  BNK %3.0f%c", (double)s.Platform.PitchDeg,
             (double)fabsf(s.Platform.RollDeg), s.Platform.RollDeg < 0.f ? 'L' : 'R');
  y += kMfdLineH;
  if (s.Ufc.H.Readable()) {
    out.Printf(x, y, kMfdBodyS, kHgR, kHgG, kHgB, "ALOW%5.0f  BNGO%5.0f", (double)s.Ufc.AlowFt,
               (double)s.Ufc.BingoEffectiveLbs);
    y += kMfdLineH;
  }
  if (!s.Warnings.H.Readable()) return;
  uint32_t act = s.Warnings.Active, inh = s.Warnings.Inhibited;
  static const struct { uint32_t Bit; const char *Text; } kWarn[] = {
      {FBWarnAlow, "ALOW"}, {FBWarnBingo, "BINGO"}, {FBWarnGearUnsafe, "GEAR"}};
  float wx = x;
  for (const auto &w : kWarn) {
    if (act & w.Bit) out.Text(wx, y, kMfdBodyS, kMfdWarnR, kMfdWarnG, kMfdWarnB, w.Text);
    else if (inh & w.Bit) out.Text(wx, y, kMfdBodyS, kMfdDimR, kMfdDimG, kMfdDimB, w.Text);
    wx += 7.f * kFontAdvance * kMfdBodyS;   /* the longest word plus one blank */
  }
}

void PageBody(FBMfdPage p, FBHudGeometry &out, const Bay &b, const FBState &s) {
  switch (p) {
    case FBMfdPage::Fcr: PageFcr(out, b, s); return;
    case FBMfdPage::Sms: PageSms(out, b, s); return;
    case FBMfdPage::Hsd: PageHsd(out, b, s); return;
    case FBMfdPage::Rwr: PageRwr(out, b, s); return;
    case FBMfdPage::Irst: PageIrst(out, b, s); return;
    case FBMfdPage::Sys: PageSys(out, b, s); return;
    case FBMfdPage::None: return;
  }
}
} // namespace

void FBDisplaySystem::BuildMfd(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const {
  const FBMfdBlock &m = state.Mfd;
  for (int i = 0; i < kMfdBays; i++) {
    FBMfdBayRect br = FBMfdBayAt(i, env.Width, env.ViewH, env.Height);
    Bay b{br.X0, br.Y0, br.X1, br.Y1};
    bool attention = (i == kMfdAttentionBay);
    int ord = (env.Have && m.H.Readable()) ? m.Bay[i] : -1;
    FBMfdPage page = (ord >= 0 && ord < m.PageCount) ? m.Pages[ord] : FBMfdPage::None;
    /* Ein Schacht mit Sensorvideo bekommt KEINEN Schleier: das Bild darunter ist selbst deckend, und
     * 87 % Schwarz darueber waere genau die Anzeige, die der Schacht ersetzen soll. */
    if (!FBMfdPageHasVideo(page, state)) out.Fill(b.x0, b.y0, b.x1, b.y1, 0.f, 0.f, 0.f, kMfdVeil);
    out.Box(b.x0, b.y0, b.x1, b.y1, kMfdDimR, kMfdDimG, kMfdDimB);
    if (!env.Have || !m.H.Readable()) {
      out.Text(b.x0 + 10.f, b.y0 + 22.f, kMfdBodyS, kMfdWarnR, kMfdWarnG, kMfdWarnB, "NO BUS");
      continue;
    }
    /* Der Schacht, auf den die Seitenwahl wirkt, traegt einen zweiten Rahmen — sonst ist am Bild nicht
     * zu sehen, WELCHE der drei Seiten der Pilot gerade gewaehlt hat. */
    if (attention) out.Box(b.x0 + 3.f, b.y0 + 3.f, b.x1 - 3.f, b.y1 - 3.f, kHgR, kHgG, kHgB);
    out.Text(b.x0 + 8.f, b.y0 + 5.f, 2.2f, kHgR, kHgG, kHgB, FBMfdPageLabel(page));
    if (ord >= 0 && !m.Selectable(ord))
      out.Text(b.x0 + 66.f, b.y0 + 7.f, 1.4f, kMfdDimR, kMfdDimG, kMfdDimB, "N/A");
    /* WANN die Seite zuletzt gewechselt wurde — der Zeitstempel, den die Box selbst gesetzt hat. */
    if (attention && m.LastSelectS >= 0.0)
      out.Printf(b.x1 - 96.f, b.y0 + 7.f, 1.5f, kHgR, kHgG, kHgB, "SEL %6.1f", m.LastSelectS);
    if (page == FBMfdPage::None) {
      out.Text(b.x0 + 10.f, b.y0 + 26.f, kMfdBodyS, kMfdDimR, kMfdDimG, kMfdDimB, "OFF");
      continue;
    }
    Bay body{b.x0, b.y0 + 12.f, b.x1, b.y1};
    out.SetClip(b.x0 + 2.f, b.y0 + 2.f, b.x1 - 2.f, b.y1 - 2.f);
    PageBody(page, out, body, state);
    out.ClearClip();
  }
}

} // namespace FlightBox::Systems
