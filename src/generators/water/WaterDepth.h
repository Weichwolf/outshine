#ifndef OUTSHINE_GENERATORS_WATER_WATERDEPTH_H
#define OUTSHINE_GENERATORS_WATER_WATERDEPTH_H

#include <optional>

namespace outshine {

class WaterDepth {
public:
  enum class State { Dry, Fields, LevelBelowGround };

  static WaterDepth Dry() { return {State::Dry, 0.0}; }

  static WaterDepth Between(double levelAslM, double groundAslM) {
    return levelAslM >= groundAslM ? WaterDepth(State::Fields, levelAslM - groundAslM)
                                   : WaterDepth(State::LevelBelowGround, groundAslM - levelAslM);
  }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] std::optional<double> DepthM() const {
    if (Where_ != State::Fields) { return std::nullopt; }
    return M_;
  }

  [[nodiscard]] std::optional<double> DisagreementM() const {
    if (Where_ != State::LevelBelowGround) { return std::nullopt; }
    return M_;
  }

private:
  WaterDepth(State where, double m) : Where_(where), M_(m) {}

  State Where_;
  double M_;
};

} // namespace outshine
#endif
