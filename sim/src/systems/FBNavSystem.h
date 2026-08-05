/* FlightBox — FBNavSystem: ein aktiver Steerpoint + Bullseye-Referenz (der Ein-Punkt-Platzhalter, mit
 * dem jedes Modul startet), plus die Wegpunkt-Sequenzierung. doc/systems.md, Abschnitt 7. */
#ifndef FBNAVSYSTEM_H
#define FBNAVSYSTEM_H

#include <cassert>

#include "FBFlightPlan.h"
#include "FBState.h"
#include "FBFdm.h"
#include "FBUnits.h"

namespace FlightBox::Systems {

class FBNavSystem {
public:
  virtual ~FBNavSystem() = default;

  /* elevFt: die EIGENE Bodenhoehe des Steerpoints (ASL) — Eingang fuer FBF16FireControls Slant-Range
   * UND fuer dessen Ballistik-Ebene. Der Wegpunkt-Ueberladung den Vorzug geben, wo es einen gibt: sie
   * nimmt die Hoehe aus dem gebrieften Fix statt aus einer Probe, die der Aufrufer waehlen darf. */
  void SetSteerpoint(double lat, double lon, double elevFt) { StLat = lat; StLon = lon; StElevFt = elevFt; Have = true; }
  void SetSteerpoint(const FBWaypoint &wp) {
    assert(FBElevationResolved(wp.GroundElevM) && "steerpoint from an unbriefed plan");
    SetSteerpoint(wp.LatDeg, wp.LonDeg, wp.GroundElevM * kMToFt);
  }
  void SetBullseye(double lat, double lon) { BullLat = lat; BullLon = lon; HaveBull = true; }

  virtual void Run(FBState &state, const Fdm::fb_fdm_state &fdm, double dt);

  /* Sequenzierung ist AKTEURS-Verhalten: das Modul ruft das selbst, nicht der Missions-Orchestrator.
   * Rueckgabe: der gerade erreichte Planindex, sonst -1. */
  int AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM = 500.0);

private:
  /* Die Anflug-Historie zum AKTIVEN Fix: wie oft ist das Flugzeug so nah heran, wie es kam, und wieder
   * aufgemacht. Zwei davon sind eine RUNDE — die Signatur eines Orbits, den kein Fangkreis beantwortet.
   * doc/systems.md, Abschnitt 7.5.1. */
  int NoteApproach(int idx, double distM, double captureM);

  double StLat = 0.0, StLon = 0.0, StElevFt = 0.0;
  double BullLat = 0.0, BullLon = 0.0;
  bool Have = false, HaveBull = false;

  int    AppIdx = -1;
  bool   AppClosing = true;
  double AppMinM = 0.0, AppMaxM = 0.0;
  int    AppFails = 0;
};

} // namespace FlightBox::Systems
#endif
