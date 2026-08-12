/* HOW DEEP THE WATER IS OVER A PLACE, and it cannot be negative.
 *
 * The level and the ground are two different measurements of one world: the level is read off the
 * shore of an OSM outline, the ground is the DEM's own surface, and inside a gorge the second stands
 * above the first. That answered -4.371 m through an optional<double> at the Ardeche, where a caller
 * that simply used the number got a river running through the rock. It is not a depth of any sign —
 * it is the two models disagreeing about this place, and that is a STATE.
 *
 * Metres out of here are non-negative by construction, in both states that carry one. */
#ifndef WATERDEPTH_H
#define WATERDEPTH_H

namespace outshine {

class WaterDepth {
public:
  enum class State { Dry, Standing, LevelBelowGround };

  static WaterDepth Dry() { return WaterDepth(State::Dry, 0.0); }
  /* Both arguments on the DEM's own datum. Which of the two wet states comes back is their order,
   * so a caller cannot construct a negative depth and cannot forget to test for one. */
  static WaterDepth Between(double levelAslM, double groundAslM) {
    return levelAslM >= groundAslM ? WaterDepth(State::Standing, levelAslM - groundAslM)
                                   : WaterDepth(State::LevelBelowGround, groundAslM - levelAslM);
  }

  [[nodiscard]] State Where() const { return Where_; }

  /* Metres of water over the ground, written only where water stands. */
  [[nodiscard]] bool TryDepthM(double *out) const {
    if (Where_ != State::Standing) return false;
    *out = M_;
    return true;
  }
  /* Metres the ground stands ABOVE the level, written only where the two models disagree. Its
   * caller is a diagnosis: the magnitude says whether the outline is off by a bank or by a cliff. */
  [[nodiscard]] bool TryDisagreementM(double *out) const {
    if (Where_ != State::LevelBelowGround) return false;
    *out = M_;
    return true;
  }

private:
  WaterDepth(State where, double m) : Where_(where), M_(m) {}

  State Where_;
  double M_;
};

} // namespace outshine
#endif
