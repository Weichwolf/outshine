/* FlightBox — FBDisplaySystem: der Anzeigen-Slot. Run() ist die periodische Anzeigenlogik (20 Hz),
 * BuildHud das generische MIL-STD-1787-artige Default-HUD (1x je gerendertem Frame) und der zweite
 * Override-Punkt. doc/systems.md, Abschnitt 8. */
#ifndef FBDISPLAYSYSTEM_H
#define FBDISPLAYSYSTEM_H

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
