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
  float Agl;   /* m, ASL - DEM-Boden; AGL-Anzeige + Horizont-Dip-Fallback bei alt<=1 */
  bool Have;   /* Telemetrie vorhanden; false -> nur der NO-TELEMETRY-Fallback */
};

class FBDisplaySystem {
public:
  virtual ~FBDisplaySystem() = default;

  virtual void Run(const FBState &state, FBMasterMode mode, double dt) { (void)state; (void)mode; (void)dt; }

  /* Const: BuildHud LIEST Zustand, es besitzt keinen. */
  virtual void BuildHud(const FBState &state, const FBHudEnv &env, FBHudGeometry &out) const;
};

} // namespace FlightBox::Systems
#endif
