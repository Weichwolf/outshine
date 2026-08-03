/* FlightBox — FBDisplaySystem: der Anzeigen-Slot. Run() ist die periodische Anzeigenlogik (20 Hz),
 * BuildHud das generische MIL-STD-1787-artige Default-HUD (1x je gerendertem Frame) und der zweite
 * Override-Punkt. doc/systems.md, Abschnitt 8. */
#ifndef FBDISPLAYSYSTEM_H
#define FBDISPLAYSYSTEM_H

#include <cmath>

#include "FBHudGeometry.h"
#include "FBMasterMode.h"
#include "FBState.h"

namespace FlightBox::Systems {

/* Was BuildHud jenseits von FBState braucht: Render-/Telemetrie-Verdrahtung, kein Sim-Zustand — reist
 * getrennt, statt FBState fuer einen Konsumenten aufzublaehen. */
struct FBHudEnv {
  int Width, Height;
  /* DAS 3x3-RASTER. Die oberen zwei Reihen sind die Scheibe: dort und nur dort steht Aussenansicht +
   * HUD, und ViewH ist ihre Unterkante. Der Kombinierer wird darin zentriert, waehrend der
   * Projektor-MASSSTAB an Height haengt — die Szene wird oben BESCHNITTEN, nicht gestaucht, also
   * bleibt px/rad dasselbe. Die dritte Reihe darunter ist die MFD-Bank. */
  int ViewH;
  float Agl;   /* m, ASL - DEM-Boden; AGL-Anzeige + Horizont-Dip-Fallback bei alt<=1 */
  bool Have;   /* Telemetrie vorhanden; false -> nur der NO-TELEMETRY-Fallback */
};

/* THE BANK'S GEOMETRY IN FRAME PIXELS, as ONE function: the page symbology is drawn here, the sensor
 * RASTER under it is drawn by a render stage, and two hand-kept copies of this arithmetic would put
 * the picture and its frame in different places. */
struct FBMfdBayRect {
  float X0, Y0, X1, Y1;
};

inline FBMfdBayRect FBMfdBayAt(int bay, int width, int viewH, int height) {
  float bw = (float)width / (float)kMfdBays;
  return {(float)bay * bw + 5.f, (float)viewH + 4.f, (float)(bay + 1) * bw - 5.f, (float)height - 4.f};
}

/* The rectangle a page body may draw in: the bay minus its header strip. */
inline FBMfdBayRect FBMfdBodyOf(const FBMfdBayRect &bay) {
  return {bay.X0, bay.Y0 + 12.f, bay.X1, bay.Y1};
}

/* The ground-map SECTOR inside a body: apex at the bottom centre (own position), nose straight up.
 * The radius is whichever of height / half-width at the sector edge runs out first. */
inline void FBFcrFan(const FBMfdBayRect &body, float azHalfDeg, float &apexX, float &apexY, float &radius) {
  float x0 = body.X0 + 8.f, x1 = body.X1 - 8.f, y0 = body.Y0 + 20.f, y1 = body.Y1 - 20.f;
  apexX = 0.5f * (x0 + x1);
  apexY = y1;
  float s = sinf(azHalfDeg * 3.14159265358979323846f / 180.f);
  float byWidth = s > 0.05f ? 0.5f * (x1 - x0) / s : 1e6f;
  radius = (y1 - y0) < byWidth ? (y1 - y0) : byWidth;
}

/* THE NIGHT-VISION PICTURE'S GEOMETRY. The image is a gnomonic crop of the very camera the windscreen
 * shows, so a symbol is placed by the TANGENT of its angle and never by the angle itself — anything
 * else puts a contact ring beside its own source towards the frame edge. Vertical half-angle = an
 * AN/AVS-9-class tube's 20 deg [SET]; the horizontal one follows from the bay's aspect, because a
 * rectangle cannot show a circular tube's corners and squeezing the picture would be worse. */
inline constexpr float kNvisElHalfDeg = 20.0f;

inline FBMfdBayRect FBNvisRect(const FBMfdBayRect &body) {
  return {body.X0 + 10.f, body.Y0 + 22.f, body.X1 - 10.f, body.Y1 - 22.f};
}

inline float FBNvisAzHalfDeg(const FBMfdBayRect &r) {
  float h = r.Y1 - r.Y0;
  if (h <= 1.f) return kNvisElHalfDeg;
  float k = 3.14159265358979323846f / 180.f;
  return atanf(tanf(kNvisElHalfDeg * k) * (r.X1 - r.X0) / h) / k;
}

/* Does this page carry SENSOR VIDEO that a render stage puts UNDER the symbology? Asked twice — here,
 * so the bay leaves its opaque veil off, and in render/, so the stage knows which bay to fill. */
inline bool FBMfdPageHasVideo(FBMfdPage p, const FBState &s) {
  if (p == FBMfdPage::Fcr) return s.GroundMap.H.Readable() && s.GroundMap.Mapping;
  if (p == FBMfdPage::Irst) return s.Irst.H.Readable() && s.Irst.Powered;
  return false;
}

class FBDisplaySystem {
public:
  virtual ~FBDisplaySystem() = default;

  virtual void Run(const FBState &state, FBMasterMode mode, double dt) { (void)state; (void)mode; (void)dt; }

  /* Const: BuildHud LIEST Zustand, es besitzt keinen. */
  virtual void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const;

  /* DIE UNTERE RASTERREIHE: drei Schaechte, jeder mit der Seite, die FBMfdBlock fuer ihn ausweist.
   * Kein Reset() — es HAENGT AN, was BuildHud aufgebaut hat, damit beide in derselben Geometrie und
   * damit im selben Renderpass landen (die Pass-Zahl je Frame ist der Vertrag).
   * Der dritte Override-Punkt: ein Modul, dessen Seiten wirklich anders AUSSEHEN, ersetzt ihn; welche
   * Seiten es GIBT, sagt dagegen sein Katalog (systems/FBMfdSystem) und nicht diese Methode. */
  virtual void BuildMfd(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const;
};

} // namespace FlightBox::Systems
#endif
