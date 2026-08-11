/* WHAT THE GROUND ANSWERED, and WHY when it answered nothing. One sentinel for two states cost the
 * browser 59 of every 60 footprints: a stand whose DEM had not landed yet was dropped exactly like
 * one standing over a hole, and a dropped stand is never asked about again. Deferral has to be
 * written deliberately, so the two are different values.
 *
 * THE HEIGHT IS REACHABLE ONLY THROUGH THE STATE. A public number beside the state is a number that
 * gets read without it — that is exactly how the same defect reappeared in the forest after it had
 * been fixed for the buildings. There is no sentinel to compare against any more; TryAslM is the
 * only door, and ignoring its answer is a build error. */
#ifndef GROUNDSAMPLE_H
#define GROUNDSAMPLE_H

namespace outshine {

class GroundSample {
public:
  enum class State { Resolved, Pending, Hole };

  static GroundSample At(double aslM) { return GroundSample(State::Resolved, aslM); }
  static GroundSample Waiting() { return GroundSample(State::Pending); }
  static GroundSample Missing() { return GroundSample(State::Hole); }

  State Where() const { return Where_; }

  /* m ASL on the DEM's own datum, written only when there is one. */
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

} // namespace outshine
#endif
