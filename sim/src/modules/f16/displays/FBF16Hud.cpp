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

struct Proj { float sx, sy, zc; };   /* zc = cos(angle from boresight); zc<=0 = behind */

/* The drawn window in screen pixels — the windscreen itself, inset. */
struct Window { float cx, cy, x0, y0, x1, y1, halfW, halfH; };

void DashedLine(Systems::FBHudGeometry &out, float x0, float y0, float x1, float y1, int n, float r, float g, float b) {
  for (int i = 0; i < n; i++) {
    float t0 = (float)i / (float)n, t1 = ((float)i + 0.5f) / (float)n;
    out.Line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, r, g, b);
  }
}

} // namespace

/* DER SCHNITT (Eigner, diese Runde): IM HUD STEHT NUR NOCH ZIELERFASSUNG. Nicht mehr "womit man zielt
 * UND NAVIGIERT" — das Navigieren ist raus. Weg sind Horizont, Nickleiter, Kurs-, Fahrt- und
 * Hoehenband, Wegpunkt-Raute, Kaulquappe und die STPT-Zeile; sie stehen unten in der MFD-Bank, und was
 * es dort noch nicht gab (CAS, Mach, ASL, AGL, VS, Kurs, g) hat diese Runde auf SYS gelegt — gestrichen
 * heisst VERSCHOBEN, nicht verschwunden.
 *
 * Was bleibt, bleibt aus einem Grund, nicht aus Gewohnheit:
 *   Visierkreuz   wohin die NASE zeigt, also das Rohr — die Zielreferenz selbst
 *   FPM           kein Fluginstrument hier, sondern der Bezugspunkt: die Fallinie haengt an ihm
 *   Zielmarken    jeder Radarkontakt an seinem eigenen weltbezogenen az/el, der gerastete als TD-Box
 *   CCIP-Pipper   wo die Bombe LANDET, plus Fallinie und Auslose-Cue
 *   EEGS-Trichter die Bleiloesung mit den Spannweiten-Waenden
 *   DLZ-Klammer   Rmin/Rtr/Raero und die Ziel-Entfernung darin
 *   eine Zeile    die gewaehlte Waffe, und der Schuss-Cue
 * Element-fuer-Element-Liste: doc/modules/f16/hud-symbology.md.
 *
 * ZWEI PROJEKTOREN, und die Trennung ist die Semantik: ein WELTwinkel (Kontakt, Pipper, FPM) geht durch
 * dieselbe Kamerabasis wie die Szene und ist damit konform; ein KOERPERwinkel (Bleiloesung, Visier) geht
 * direkt auf den Schirm, weil der Schirm das Flugzeugsystem IST — eine Rolldrehung darauf waere falsch.
 * "az/el" ist im ersten Fall weltbezogen (0 = Nord, +el = oben), im zweiten nasenbezogen. */
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
  /* MILLIRADIAN, NICHT PIXEL. Jede Symbolgroesze unten ist eine WINKELgroesze aus der Quelle
   * (doc/modules/f16/hud-symbology.md, BMS-Dash-34-Addendum). Kc ist Pixel je Radiant, ein Millirad
   * also Kc*1e-3 px. Das ersetzt die Vorrunde, die JEDE Proportion in Pixeln erfinden musste.
   *
   * UND EINE EINZIGE ERFUNDENE ZAHL BLEIBT — hier, sichtbar, statt zwanzigmal verteilt. Roh gezeichnet
   * ist ein 10-mR-Symbol auf diesem Schirm 6 px: richtig und praktisch unlesbar. Der Grund ist kein
   * Zeichenfehler, sondern ein Blickwinkel-Unterschied: die Szene wird mit 60 Grad vertikal projiziert
   * (core/FBCamera.h kSceneVerticalFovDeg), waehrend der Pilot die Symbolik durch einen Kombinierer von
   * rund 25 Grad TFOV sieht [DOC hud-symbology.md]. Ein bei wahrer Winkelgroesze gezeichnetes Symbol
   * erscheint deshalb um 60/25 kleiner, als der Pilot es wahrnimmt.
   *   kSymbolMagnify = 60 / 25 = 2.4
   * DIE POSITION IST DAVON UNBERUEHRT — konform bleibt konform, der FPM sitzt auf dem
   * Geschwindigkeitsvektor. Skaliert wird nur, WIE GROSZ ein Symbol an seinem Ort gezeichnet wird, und
   * das ist auch im echten Geraet keine konforme Groesze. Wer die Szene auf 25 Grad stellt, setzt diese
   * Zahl auf 1. */
  constexpr float kHudCombinerTfovDeg = 25.0f;   /* [DOC] doc/modules/f16/hud-symbology.md */
  constexpr float kSymbolMagnify = kHudFovDeg / kHudCombinerTfovDeg;
  const float mr = Kc * 1e-3f * kSymbolMagnify;

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

  /* ===== ZIELERFASSUNG, KONFORM: Kontakte, Zielmarke, Pipper, Trichter. Alles auf das Fenster
   * beschnitten, alles durch DENSELBEN Projektor wie die Szene. ===== */
  out.SetClip(win.x0, win.y0, win.x1, win.y1);

  /* ----- FPM. Er bleibt, weil er kein Fluginstrument ist, sondern der BEZUGSPUNKT, gegen den gezielt
   * wird: die Fallinie haengt an ihm und der Trichter wird gegen ihn gelesen. ----- */
  Proj fpm = Project(state.AirData.TrackDeg, state.AirData.FpaDeg);
  if (state.AirData.H.Readable()) {
    /* [BMS-34 S. 108] "a 10 mR circle with 10 mR wings ... along with a 5 mR tail". Durchmesser 10 mR,
     * also Radius 5; die Fluegel sind je 10 mR lang und setzen am Kreis an. DAS LEITWERK ZEIGT NACH
     * OBEN — die vorige Runde zeichnete es nach unten und stellte das Symbol damit auf den Kopf. */
    float fx = fpm.sx, fy = fpm.sy;
    out.Circle(fx, fy, 5.f * mr, 16, kHgR, kHgG, kHgB);
    out.Line(fx - 15.f * mr, fy, fx - 5.f * mr, fy, kHgR, kHgG, kHgB);
    out.Line(fx + 5.f * mr, fy, fx + 15.f * mr, fy, kHgR, kHgG, kHgB);
    out.Line(fx, fy - 5.f * mr, fx, fy - 10.f * mr, kHgR, kHgG, kHgB);
  }

  /* ----- DIE RADARKONTAKTE ALS ZIELMARKEN. Weltbezogenes az/el steht am Kontakt selbst
   * (core/FBRadarContact.h BearingDeg/ElevAngleDeg, ausdruecklich damit ein Verbraucher das Echo ohne
   * Rueckdrehung setzen kann), also geht es durch denselben Projektor. Ein Kontakt traegt KEINE
   * Identitaet — deshalb steht an keinem Kaestchen eine Zugehoerigkeit, nur die IFF-Antwort, und die
   * kennt kein "hostile".
   * GROESZEN BELEGT [BMS-34 S. 109]: "the Primary TD-box measures 25 mR on each side ... the Secondary
   * TD-boxes ... are 15 mR on each side". Der gerastete Kontakt ist die primaere Box, jeder andere eine
   * sekundaere — die Unterscheidung ist die Quelle, nicht Geschmack. ----- */
  if (state.Radar.H.Readable()) {
    const FBRadarBlock &r = state.Radar;
    for (int i = 0; i < r.ContactCount && i < kMaxRadarContacts; i++) {
      const FBRadarContact &c = r.Contacts[i];
      bool locked = i == r.LockIndex;
      Proj p = Project(c.BearingDeg, c.ElevAngleDeg);
      float half = (locked ? 12.5f : 7.5f) * mr;   /* 25 mR bzw. 15 mR Kantenlaenge */
      bool outOfFov = p.zc < 0.05f || p.sx < win.x0 || p.sx > win.x1 || p.sy < win.y0 || p.sy > win.y1;
      if (!outOfFov) {
        out.Box(p.sx - half, p.sy - half, p.sx + half, p.sy + half, kHgR, kHgG, kHgB);
        /* Der EINZIGE Identitaetstraeger im Baum ist die IFF-Antwort; "kein Rueckruf" bleibt stumm. */
        if (locked && c.Iff == FBIffReply::Friendly)
          out.Text(p.sx - 4.f * mg, p.sy - half - 6.f * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB, "F");
        if (locked)
          out.Printf(p.sx + half + 6.f * mg, p.sy + 4.f * mg, kHudSecondaryScale * mg,
                     kHgR, kHgG, kHgB, "%4.1f", (double)(c.RangeM * kMToNm));
        continue;
      }
      /* AUSSERHALB DES SICHTFELDS: die TARGET LOCATOR LINE, nicht eine an den Rand geklemmte Box.
       * [BMS-34 S. 104, 109] "40 mR dotted line from the gun boresight cross, oriented at the relative
       * bearing to the target; text = source letter (F = FCR) + 2-digit target angle, updated in 1
       * degree increments". Die vorige Runde erfand hier eine Randklemme mit Kreuz; das hier ist die
       * dokumentierte Anzeige und traegt zusaetzlich die Zahl, die sie tragen soll.
       * Die Richtung kommt aus den KOERPERwinkeln, nicht aus der Projektion: ein Ziel hinter dem Bug
       * hat keine gueltige Projektion, aber sehr wohl eine Richtung. 0 Grad = senkrecht nach oben,
       * positiv im Uhrzeigersinn. */
      float ang = atan2f(c.AzDeg, c.ElDeg);
      float ux = sinf(ang), uy = -cosf(ang);
      DashedLine(out, cx, cy, cx + ux * 40.f * mr, cy + uy * 40.f * mr, 5, kHgR, kHgG, kHgB);
      float offDeg = sqrtf(c.AzDeg * c.AzDeg + c.ElDeg * c.ElDeg);
      if (offDeg > 99.f) offDeg = 99.f;
      out.Printf(cx + ux * 46.f * mr - 8.f * mg, cy + uy * 46.f * mr, kHudSecondaryScale * mg,
                 kHgR, kHgG, kHgB, "F%02d", (int)(offDeg + 0.5f));
    }
  }

  /* ----- CCIP: der Pipper steht dort, wo die Bombe LANDET. Die Lage ist aus dem Block herleitbar und
   * braucht keine neue Verdrahtung: der Aufschlagpunkt liegt AgRangeM voraus auf dem BODENKURS (eine
   * ungelenkte Waffe erbt den Geschwindigkeitsvektor) und AltM - AgImpactElevM darunter, also ist die
   * Depression atan2 der beiden. Die Fallinie verbindet FPM und Pipper — sie IST die Bahn, auf der der
   * Aufschlag wandert, wenn der Pilot nichts tut. ----- */
  bool agShown = false;
  if (state.FireControl.H.Readable() && state.FireControl.AgValid && state.AirData.H.Readable()) {
    const FBFireControlBlock &fc = state.FireControl;
    float dropM = state.Platform.AltM - fc.AgImpactElevM;
    float rngM = fc.AgRangeM > 1.f ? fc.AgRangeM : 1.f;
    float depDeg = -atan2f(dropM, rngM) * kR2D;
    Proj pip = Project(state.AirData.TrackDeg, depDeg);
    agShown = true;
    DashedLine(out, fpm.sx, fpm.sy, pip.sx, pip.sy, 9, kHgR, kHgG, kHgB);
    /* [BMS-34 S. 625] "1 mR dot at the centre of a 12 mR circle" — Radius also 6 mR, der Punkt ein
     * halbes Millirad. Der zweite Ring ist der IN-RANGE-Cue mit 16 mR und steht nur, wenn die Loesung
     * ihn hergibt; er ist eine AUSSAGE, kein Zierring. */
    out.Circle(pip.sx, pip.sy, 6.f * mr, 16, kHgR, kHgG, kHgB);
    out.Circle(pip.sx, pip.sy, 0.5f * mr, 6, kHgR, kHgG, kHgB);
    if (fc.AgInRange) out.Circle(pip.sx, pip.sy, 8.f * mr, 20, kHgR, kHgG, kHgB);
    /* Der Auslose-Cue als schrumpfender Balken am Pipper: er ist die einzige Zahl dieses Modus, die
     * eine HANDLUNG verlangt, und er wird abgelesen, nicht gerechnet. 5 s Vollausschlag [SET]. */
    if (fc.AgTimeToReleaseS > 0.f && fc.AgTimeToReleaseS < 5.f) {
      float h = 26.f * mg, frac = fc.AgTimeToReleaseS / 5.f;
      float bx = pip.sx + 15.f * mg, by0 = pip.sy - 0.5f * h, by1 = pip.sy + 0.5f * h;
      out.Line(bx, by0, bx, by1, kHgR, kHgG, kHgB);
      out.QLine(bx, by1 - h * frac, bx, by1, 2.0f, kHgR, kHgG, kHgB);
    }
  }

  out.ClearClip();

  /* ===== KOERPERFESTES: alles, was gegen die NASE gelesen wird. Ein Koerperwinkel geht NICHT durch die
   * Kamerabasis — der Schirm IST das Flugzeugsystem, also bildet er direkt ab, ohne Rolldrehung. Kc ist
   * derselbe Massstab wie oben, damit Trichter und Szene denselben Winkel meinen. ===== */
  auto ProjectBody = [&](float azDeg, float elDeg) -> Proj {
    return {cx + Kc * tanf(azDeg * kRad), cy - Kc * tanf(elDeg * kRad), 1.f};
  };

  /* ----- Das Visierkreuz: wohin die NASE zeigt, und damit das Rohr. Bildschirmfest, immer da. ----- */
  out.Line(cx - 14 * mg, cy, cx - 5 * mg, cy, kHgR, kHgG, kHgB);
  out.Line(cx + 5 * mg, cy, cx + 14 * mg, cy, kHgR, kHgG, kHgB);
  out.Line(cx, cy - 14 * mg, cx, cy - 5 * mg, kHgR, kHgG, kHgB);
  out.Line(cx, cy + 5 * mg, cx, cy + 14 * mg, kHgR, kHgG, kHgB);

  /* ----- DER TRICHTER (EEGS). Seine Waende sind die SPANNWEITE des Ziels, gezeichnet an der
   * Entfernung, fuer die das Rohr richtig zeigt — genau deshalb ist "ausser Schussweite" hier
   * buchstaeblich GunSpanMr < GunFunnelBottomMr und keine zweite Regel. Der Trichter haengt an der
   * Bleiloesung (koerperfest), nicht am Kontakt. ----- */
  if (state.FireControl.H.Readable() && state.FireControl.GunValid) {
    const FBFireControlBlock &fc = state.FireControl;
    out.SetClip(win.x0, win.y0, win.x1, win.y1);
    Proj lead = ProjectBody(fc.GunLeadAzDeg, fc.GunLeadElDeg);
    /* Milliradian -> Pixel ueber denselben Massstab: 1 mr = Kc * 1e-3 px in Kleinwinkelnaehe. */
    float topPx = 0.5f * fc.GunFunnelTopMr * 1e-3f * Kc;
    float botPx = 0.5f * fc.GunFunnelBottomMr * 1e-3f * Kc;
    float span = 46.f * mg;   /* die Hoehe des gezeichneten Trichters [SET] */
    for (int s = -1; s <= 1; s += 2) {
      float x0 = lead.sx + (float)s * botPx, y0 = lead.sy + 0.5f * span;
      float x1 = lead.sx + (float)s * topPx, y1 = lead.sy - 0.5f * span;
      out.Line(x0, y0, x1, y1, kHgR, kHgG, kHgB);
    }
    /* Die Spannweite des Ziels als Marke auf den Waenden: liegt sie zwischen ihnen, passt die
     * Entfernung. In der Loesung ist das GunInRange. */
    float sp = 0.5f * fc.GunSpanMr * 1e-3f * Kc;
    if (fc.GunInRange) {
      float t = (botPx - topPx) > 1e-3f ? (botPx - sp) / (botPx - topPx) : 0.f;
      t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
      float my = lead.sy + 0.5f * span - t * span;
      out.Line(lead.sx - sp, my, lead.sx + sp, my, kHgR, kHgG, kHgB);
    }
    /* Der Pipper sitzt in der Bleiloesung; gefuellt, wenn die Nase drin ist. */
    out.Circle(lead.sx, lead.sy, 3.f * mg, 12, kHgR, kHgG, kHgB);
    if (fc.GunInFunnel) out.Circle(lead.sx, lead.sy, 1.6f * mg, 10, kHgR, kHgG, kHgB);
    out.ClearClip();
  }

  /* ----- DIE DLZ-KLAMMER, rechts neben dem Visier: die einzige Entfernungsdarstellung, die ein
   * Schuetze braucht. Von unten nach oben Rmin .. Raero, die Ziel-Entfernung als Zeiger, Rtr als
   * Querstrich. Ohne Rasten und ohne gewaehlte Waffe mit Schusszone gibt es sie nicht. ----- */
  if (state.FireControl.H.Readable() && state.FireControl.DlzValid) {
    const FBFireControlBlock &fc = state.FireControl;
    float sx = cx + 118.f * mg, y0 = cy + 58.f * mg, y1 = cy - 58.f * mg;
    float top = fc.RaeroM > 1.f ? fc.RaeroM : 1.f;
    auto Y = [&](float m) { float t = m / top; t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t); return y0 + (y1 - y0) * t; };
    out.Line(sx, y0, sx, y1, kHgR, kHgG, kHgB);
    out.Line(sx - 5 * mg, y1, sx + 5 * mg, y1, kHgR, kHgG, kHgB);
    out.Line(sx - 5 * mg, Y(fc.RminM), sx + 5 * mg, Y(fc.RminM), kHgR, kHgG, kHgB);
    DashedLine(out, sx - 4 * mg, Y(fc.RtrM), sx + 4 * mg, Y(fc.RtrM), 3, kHgR, kHgG, kHgB);
    float cyr = Y(fc.TargetRangeM);
    out.Line(sx - 9 * mg, cyr, sx, cyr - 4 * mg, kHgR, kHgG, kHgB);
    out.Line(sx, cyr - 4 * mg, sx, cyr + 4 * mg, kHgR, kHgG, kHgB);
    out.Line(sx, cyr + 4 * mg, sx - 9 * mg, cyr, kHgR, kHgG, kHgB);
    out.Printf(sx + 8 * mg, cyr + 4 * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB,
               "%4.1f", (double)(fc.TargetRangeM * kMToNm));
    if (fc.TimeToImpactS > 0.f)
      out.Printf(sx - 6 * mg, y0 + 14 * mg, kHudSecondaryScale * mg, kHgR, kHgG, kHgB,
                 "T%3.0f", (double)fc.TimeToImpactS);
  }

  /* ----- DIE EINE ZEILE: womit geschossen wird und ob jetzt. Mehr Text gibt es im HUD nicht — alles
   * Uebrige steht unten in der MFD-Bank. ----- */
  {
    /* KEIN Master-Mode-Wort. Er steht nicht auf dem Bus, sondern wird an Run() gereicht, und FBHudEnv
     * ist ausdruecklich Verdrahtung statt Sim-Zustand — ihn dafuer aufzubohren waere ein Umweg um eine
     * Grenze, die absichtlich da ist. Er ist ohnehin ablesbar: Trichter heisst Kanone, Pipper heisst
     * Boden, Klammer heisst Flugkoerper. */
    float s = kHudReadoutScale * mg, ly = win.y1 - 10.f * mg, lx = win.x0 + 6.f * mg;
    if (state.Gun.H.Readable() && state.Gun.Ready)
      out.Printf(lx, ly, s, kHgR, kHgG, kHgB, "GUN %4d", state.Gun.RoundsRemaining);
    else if (state.Stores.H.Readable() && state.Stores.SelectedStation > 0)
      out.Printf(lx, ly, s, kHgR, kHgG, kHgB, "STA %d", state.Stores.SelectedStation);

    /* Der Schuss-Cue, mittig unter dem Visier: die einzige Aufforderung, die das HUD ausspricht. */
    const FBFireControlBlock &fc = state.FireControl;
    const char *cue = nullptr;
    if (state.FireControl.H.Readable()) {
      if (fc.GunValid && fc.GunInFunnel) cue = "SHOOT";
      else if (fc.DlzValid && fc.InZone) cue = "SHOOT";
      else if (agShown && fc.AgInRange && fc.AgTimeToReleaseS <= 0.f) cue = "RELEASE";
      else if (fc.DlzValid) cue = "IN RANGE";
    }
    if (cue) out.Text(cx - 26 * mg, cy + 40 * mg, s, kHgR, kHgG, kHgB, cue);
  }
}

} // namespace FlightBox::Modules
