/* FlightBox — FBMissileArSeeker: an anti-radiation round's head, and structurally the SAME CLASS a
 * pilot's warning receiver is (sensors/FBRwrSystem). The reason is the reason FBMissileIrSeeker derives
 * from the jet's infrared set: a missile is a unit of the world, so it may perceive the world only the
 * way anything here may — through a simulated sensor with stated limits.
 *
 * WHAT THAT BUYS, and it is the whole file: this round measures exactly what a warning receiver
 * measures — an arrival BEARING, an arrival ELEVATION and a RECEIVED POWER. It measures NO RANGE, no
 * closure, no identity and no team, because FBRwrThreat has a field for none of them and this class
 * adds none. It therefore homes on friendly emitters, and that is the acceptance criterion for "no
 * identity" rather than a defect.
 *
 * TWO OVERRIDES, BOTH EXISTING HOOKS, AND NO NEW READER. The cone is expressed through Blanked() —
 * FBMig29Rwr already uses that hook to blank a hemisphere; this uses it to KEEP one — and through
 * ElevCoverageDeg(), so the field of regard is a genuine cone around the round's own nose rather than a
 * hemisphere with a notch. sensors/FBRwrSystem.cpp holds the unit-registry include already, so
 * tools/verify_layers.py's reader count is unchanged by this derivation.
 *
 * IT LOOKS EVERY TICK, and that is physics rather than a favour: a wideband passive receiver has no
 * antenna to sweep, so it is the one seeker in the tree with no frame raster.
 * doc/air-to-ground.md §2.1. */
#ifndef FBMISSILEARSEEKER_H
#define FBMISSILEARSEEKER_H

#include "FBGeodesy.h"
#include "FBRwrSystem.h"
#include "FBStore.h"
#include <cmath>

namespace FlightBox::Modules {

class FBMissileArSeeker : public Sensors::FBRwrSystem {
public:
  FBMissileArSeeker();

  /* The round's own cone half-angle, from its catalogue entry. There is no FOV/gimbal split here and
   * that is the difference from the two steerable heads: nothing points this receiver. */
  void Configure(double fovHalfDeg) { FovHalfDeg_ = fovHalfDeg; }

  /* Which class of transmitter this round was programmed against — `set arm_class`, travelling with the
   * release. It is not a filter INSIDE the receiver (the receiver hears everything its cone covers);
   * it is what the guidance above is allowed to latch. */
  void SetTargetClass(FBArTargetClass c) { Class_ = c; }
  FBArTargetClass TargetClass() const { return Class_; }

  /* Does this threat's estimated kind satisfy the programming? Asked by the guidance for every entry in
   * the published block, in the block's own priority order. */
  bool Admissible(FBEmitterKind k) const {
    switch (Class_) {
      case FBArTargetClass::SurfaceFireControl: return k == FBEmitterKind::SurfaceFireControl;
      case FBArTargetClass::SurfaceEarlyWarning: return k == FBEmitterKind::SurfaceEarlyWarning;
      case FBArTargetClass::AnySurface: break;
    }
    return k == FBEmitterKind::SurfaceFireControl || k == FBEmitterKind::SurfaceEarlyWarning;
  }

protected:
  /* The two hooks that make a 360-degree warning receiver a forward-looking SEEKER. Nothing else about
   * the class changes: the power law, the hold time, the priority order and the table are the pilot's. */
  bool Blanked(double rxAzDeg) const override {
    return std::fabs(FBWrap180(rxAzDeg)) > FovHalfDeg_;
  }
  double ElevCoverageDeg() const override { return FovHalfDeg_; }

private:
  double FovHalfDeg_ = 0.0;
  FBArTargetClass Class_ = FBArTargetClass::AnySurface;
};

} // namespace FlightBox::Modules
#endif
