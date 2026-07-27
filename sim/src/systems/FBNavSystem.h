/* FlightBox — FBNavSystem: ein aktiver Steerpoint + Bullseye-Referenz (der Ein-Punkt-Platzhalter, mit
 * dem jedes Modul startet), plus die Wegpunkt-Sequenzierung. doc/flightbox/systems.md, Abschnitt 7. */
#ifndef FBNAVSYSTEM_H
#define FBNAVSYSTEM_H

#include "FBFlightPlan.h"
#include "FBState.h"
#include "FBFdm.h"

namespace FlightBox {

class FBNavSystem {
public:
  virtual ~FBNavSystem() = default;

  /* elevFt: die EIGENE Bodenhoehe des Steerpoints (ASL) — Eingang fuer FBF16FireControls Slant-Range. */
  void SetSteerpoint(double lat, double lon, double elevFt) { StLat = lat; StLon = lon; StElevFt = elevFt; Have = true; }
  void SetBullseye(double lat, double lon) { BullLat = lat; BullLon = lon; HaveBull = true; }

  virtual void Run(FBState &state, const fb_fdm_state &fdm, double dt);

  /* Sequenzierung ist AKTEURS-Verhalten: das Modul ruft das selbst, nicht der Missions-Orchestrator.
   * Rueckgabe: der gerade erreichte Planindex, sonst -1. */
  int AdvanceWaypoint(FBFlightPlan &plan, double lat, double lon, double captureM = 500.0);

private:
  double StLat = 0.0, StLon = 0.0, StElevFt = 0.0;
  double BullLat = 0.0, BullLon = 0.0;
  bool Have = false, HaveBull = false;
};

} // namespace FlightBox
#endif
