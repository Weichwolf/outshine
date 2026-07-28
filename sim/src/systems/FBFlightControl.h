/* FlightBox — FBFlightControl: die FBW-Innenschleife. Guidance + JSBSim-Zustand -> normierte Stick-/
 * Throttle-Werte. Zwei innere Arten hinter dem `Flcs`-Flag (Konfiguration, keine Subklasse: Tuning
 * statt Verhalten). Gesetz und Gain-Tabellen: doc/systems.md, Abschnitt 3. */
#ifndef FBFLIGHTCONTROL_H
#define FBFLIGHTCONTROL_H

#include "FBAutopilot.h"
#include "FBTelemetry.h"

namespace FlightBox::Systems {

struct FBControls { double Roll, Pitch, Yaw, Thr; };

class FBFlightControl : public FBTelemetrySource {
public:
  FBFlightControl();
  virtual ~FBFlightControl() = default;

  /* Ein 100-Hz-Schritt; mutiert die Integratoren. Der EINE Override-Punkt. */
  virtual FBControls Run(const FBGuidance &g, const Fdm::fb_fdm_state &s);

  const char *TelemetryName() const override { return "flightcontrol"; }
  void DeclareTelemetry(FBTelemetrySchema &schema) const override;
  void SampleTelemetry(FBTelemetryRow &row) const override;

  void Reset(void);                      /* alle Integratoren nullen (neuer Flug) */
  static FBFlightControl F16(void);      /* das geflogene F-16-Preset */

  /* Gains — public Config-Block; Defaults im ctor, F-16-Werte via F16(). */
  int    Flcs;                           /* 1 = FLCS kommandieren, 0 = rohe Ruder-PD */
  double RollStickMax;
  double KRollRate, KG, KGi, KVs2g, KVsi, KNy, KNyi, KTi, KpSpd, ThrTrim;
  double KpRoll, KdRoll, KpPitch, KdPitch, KdYaw, KCoord, PitchMaxDeg, KAltRaw;
  double NzSlew;                         /* max. g-Kommandoaenderung je Sekunde */

  double GetGIterm(void) const { return GIterm; }
  double GetVsIterm(void) const { return VsIterm; }

private:
  double GIterm, VsIterm, NyIterm, ThrIterm, NzPrev;
  FBControls LastControls_{0.0, 0.0, 0.0, 0.0};
};

} // namespace FlightBox::Systems
#endif
