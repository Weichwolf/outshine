/* FlightBox — FBSiteRadar: a ground set, and the ONE thing that makes it one — what sits behind the
 * antenna. Everything else a surface radar needs (the volume, the frame time, the Doppler notch, the
 * coast law, the emission derived from the pattern flown) is sensors/FBRadarSystem's already, and a
 * ground mount is geometrically an airframe with roll = pitch = 0.
 *
 * TWO INSTANCES, ONE CLASS: a position's acquisition set and its fire-control set differ in their
 * volume, their frame time and their emitter kind, and in nothing else — so they are two configured
 * instances rather than two classes, exactly as the nine positions are nine catalogue rows rather than
 * nine modules.
 *
 * THE PERCEPTION BOUNDARY DOES NOT GROW: this file names no registry. The scan lives in the base, and
 * tools/verify_layers.py's RESTRICTED list is unchanged. doc/modules/ground/module.md §Spec 7. */
#ifndef FBSITERADAR_H
#define FBSITERADAR_H

#include "FBRadarSystem.h"

namespace FlightBox::Modules {

class FBSiteRadar : public Sensors::FBRadarSystem {
public:
  /* Wiring, once, from the catalogue row. `notchRejects` is the base class's own hook and is passed
   * through rather than re-decided: a set whose source quantifies no detection threshold keeps the
   * tree's stated simplification. */
  void Configure(FBEmitterKind kind, double notchMs, bool notchRejects) {
    Kind_ = kind;
    NotchMs_ = notchMs;
    NotchRejects_ = notchRejects;
  }

  /* WHERE THE PENCIL POINTS. The fire-control set is TRAINED by the engagement machine onto the bearing
   * its acquisition set produced — a direction and nothing else crosses between the two, which is what
   * keeps them two sensors rather than one fused picture with an identity in it. */
  void SlewTo(double azDeg, double elDeg) {
    Sensors::FBRadarScanVolume v = SearchVolume();
    v.AzCenterDeg = azDeg;
    v.ElCenterDeg = elDeg;
    SetSearchVolume(v);
  }

protected:
  FBEmitterKind EmitterKind() const override { return Kind_; }
  double DopplerNotchMs(double rangeM) const override { (void)rangeM; return NotchMs_; }
  bool NotchRejectsDetection() const override { return NotchRejects_; }
  /* 0 = acquisition, 1 = fire control. The one telemetry label this class needs; a position's own
   * `site` source carries the rest. */
  int ModeOrdinal() const override { return Kind_ == FBEmitterKind::SurfaceFireControl ? 1 : 0; }

private:
  FBEmitterKind Kind_ = FBEmitterKind::SurfaceEarlyWarning;
  double NotchMs_ = 0.0;
  bool   NotchRejects_ = false;
};

} // namespace FlightBox::Modules
#endif
