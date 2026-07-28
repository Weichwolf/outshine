/* FlightBox — FBAirRadar: a catalogue aircraft's set. Everything a radar DOES is
 * sensors/FBRadarSystem's already; what a row states is its volume, its frame time, its clutter gate
 * and — for two rows, decisively — how far it can see BELOW itself.
 *
 * ONE CLASS, TEN CONFIGURED INSTANCES, exactly as modules/ground/FBSiteRadar is one class and two.
 * THE PERCEPTION BOUNDARY DOES NOT GROW: this file names no registry, so tools/verify_layers.py's
 * PERCEPTION_READERS list stays at six. doc/modules/air/module.md §Spec 6. */
#ifndef FBAIRRADAR_H
#define FBAIRRADAR_H

#include "FBAircraft.h"
#include "FBRadarSystem.h"

namespace FlightBox::Modules {

class FBAirRadar : public Sensors::FBRadarSystem {
public:
  void Configure(const FBAirRadarSpec &spec) {
    Spec_ = spec;
    Sensors::FBRadarScanVolume v;
    v.AzCenterDeg = 0.0;
    v.AzHalfDeg = spec.AzHalfDeg;
    v.ElCenterDeg = spec.ElCenterDeg;
    v.ElHalfDeg = spec.ElHalfDeg;
    v.RangeM = spec.SearchRangeM;
    v.FrameS = spec.FrameS;
    v.AutoAcquire = false;
    v.SingleTarget = false;
    v.Active = spec.SearchRangeM > 0.0;
    SetSearchVolume(v);
    Track_ = v;
    Track_.RangeM = spec.TrackRangeM > 0.0 ? spec.TrackRangeM : spec.SearchRangeM;
    Track_.AzHalfDeg = spec.AzHalfDeg;
    Track_.ElHalfDeg = spec.ElHalfDeg;
    Track_.FrameS = 0.5;
    Track_.AutoAcquire = true;
    Track_.SingleTarget = true;
  }

  /* WHERE THE CONTROLLER SAID TO LOOK — the whole of what a cue buys, and it is applied to the SEARCH
   * volume's centre and nothing else. Not the range gate, not the frame time, not a track file entry:
   * the member's own set must still detect, firm over kHitsToFirm looks and pass its own gate. */
  void RecentreOn(double azDeg, double elDeg) {
    Sensors::FBRadarScanVolume v = SearchVolume();
    v.AzCenterDeg = azDeg;
    v.ElCenterDeg = elDeg;
    SetSearchVolume(v);
    Track_.AzCenterDeg = azDeg;
    Track_.ElCenterDeg = elDeg;
  }

  /* The single-target pattern, entered by a DESIGNATION and left by losing it — one volume swap, which
   * is all a mode set is (doc/sensors.md §4.5). */
  void SetSingleTarget(bool on) { Stt_ = on; }

protected:
  const Sensors::FBRadarScanVolume &ActiveVolume() const override {
    return Stt_ && Locked() ? Track_ : SearchVolume();
  }
  int ModeOrdinal() const override { return Stt_ && Locked() ? 1 : 0; }
  double DopplerNotchMs(double rangeM) const override { (void)rangeM; return Spec_.DopplerNotchMs; }
  bool NotchRejectsDetection() const override { return Spec_.NotchRejects; }

  /* THE LOOK-DOWN GATE, and on the two eastern interceptor rows it is the aircraft. The argument is the
   * target's elevation angle in the WORLD frame, because a look-down limit is about ground clutter
   * under the beam and therefore about the horizon rather than about where the nose points.
   *   LookDownRangeM < 0   no limit  — every western set, and the sourced Cyrano IV
   *   LookDownRangeM = 0   the RP-22: "it couldn't intercept targets flying under the MiG" [T4]
   *   LookDownRangeM > 0   the reduced reach a set manages under the horizon (the N003E: 14 km
   *                        head-on against a 52 km search range, so a factor of 0.27) */
  double LookDownFactor(double elevAngleDeg) const override {
    if (Spec_.LookDownRangeM < 0.0 || elevAngleDeg >= 0.0 || Spec_.SearchRangeM <= 0.0) return 1.0;
    return Spec_.LookDownRangeM / Spec_.SearchRangeM;
  }

private:
  FBAirRadarSpec Spec_{};
  Sensors::FBRadarScanVolume Track_{};
  bool Stt_ = false;
};

} // namespace FlightBox::Modules
#endif
