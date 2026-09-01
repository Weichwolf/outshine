#ifndef OUTSHINE_WORLD_GROUND_GROUNDSAMPLE_H
#define OUTSHINE_WORLD_GROUND_GROUNDSAMPLE_H

#include "math/Vec3.h"
#include "math/Vec3.h"

namespace outshine {

class GroundSample {
public:
  enum class State { Resolved, Pending, Hole };

  static GroundSample At(double aslM) { return GroundSample(State::Resolved, aslM); }

  static GroundSample At(double aslM, const Vec3 &normal) {
    GroundSample out(State::Resolved, aslM);
    out.NormalM_[0] = normal[0];
    out.NormalM_[1] = normal[1];
    out.NormalM_[2] = normal[2];
    return out;
  }

  [[nodiscard]] GroundSample Coarser(int byZoomSteps) const {
    GroundSample out(*this);
    out.CoarseBy_ = byZoomSteps;
    return out;
  }

  static GroundSample Waiting() { return GroundSample(State::Pending); }

  static GroundSample Missing() { return GroundSample(State::Hole); }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryAslM(double *out) const {
    if (Where_ != State::Resolved) { return false; }
    *out = AslM_;
    return true;
  }

  [[nodiscard]] const Vec3 &NormalM() const { return NormalM_; }

  [[nodiscard]] int CoarseBy() const { return CoarseBy_; }

private:
  explicit GroundSample(State where, double aslM = 0.0) : Where_(where), AslM_(aslM) {}

  State Where_;
  double AslM_;
  Vec3 NormalM_ = {{0.0, 1.0, 0.0}};
  int CoarseBy_ = 0;
};

} // namespace outshine
#endif
