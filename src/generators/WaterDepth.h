#ifndef OUTSHINE_GENERATORS_WATERDEPTH_H
#define OUTSHINE_GENERATORS_WATERDEPTH_H

namespace outshine {

class WaterDepth {
public:
  enum class State { Dry, Fields, LevelBelowGround };

  static WaterDepth Dry() { return WaterDepth(State::Dry, 0.0); }

  static WaterDepth Between(double levelAslM, double groundAslM) {
    return levelAslM >= groundAslM ? WaterDepth(State::Fields, levelAslM - groundAslM)
                                   : WaterDepth(State::LevelBelowGround, groundAslM - levelAslM);
  }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] bool TryDepthM(double *out) const {
    if (Where_ != State::Fields) { return false; }
    *out = M_;
    return true;
  }

  [[nodiscard]] bool TryDisagreementM(double *out) const {
    if (Where_ != State::LevelBelowGround) { return false; }
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
