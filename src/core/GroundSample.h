#ifndef GROUNDSAMPLE_H
#define GROUNDSAMPLE_H

namespace outshine {

class GroundSample {
public:
  enum class State { Resolved, Pending, Hole };

  static GroundSample At(double aslM) { return GroundSample(State::Resolved, aslM); }
  static GroundSample Waiting() { return GroundSample(State::Pending); }
  static GroundSample Missing() { return GroundSample(State::Hole); }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryAslM(double *out) const {
    if (Where_ != State::Resolved) return false;
    *out = AslM_;
    return true;
  }

private:
  explicit GroundSample(State where, double aslM = 0.0) : Where_(where), AslM_(aslM) {}

  State Where_;
  double AslM_;
};

}
#endif
