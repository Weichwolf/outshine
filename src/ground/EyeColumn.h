#ifndef EYECOLUMN_H
#define EYECOLUMN_H

namespace outshine::World {

class EyeColumn {
public:

  static constexpr double kMinClearM = 2.0;

  void SetGroundAslM(double m) { Ground_ = m; }

  void SetEyeAglM(double m) { Eye_ = m; Lens_ = kNoLens; }

  void SetLensAslM(double m) { Lens_ = m; }

  void SetRoofAslM(double m) { Roof_ = m; Roofed_ = true; }

  double GroundAslM() const { return Ground_; }
  double EyeAglM() const {
    double e = Lens_ > kNoLens ? Lens_ - Ground_ : Eye_;
    if (Lens_ > kNoLens && e < kMinClearM) e = kMinClearM;
    if (Roofed_ && Ground_ + e < Roof_ + kMinClearM) e = Roof_ + kMinClearM - Ground_;
    return e;
  }
  double AltAslM() const { return Ground_ + EyeAglM(); }

  double LiftM() const { return EyeAglM() - (Lens_ > kNoLens ? Lens_ - Ground_ : Eye_); }
  [[nodiscard]] bool LensDeclared() const { return Lens_ > kNoLens; }

private:
  static constexpr double kNoLens = -1.0e8;
  double Ground_ = 0.0, Eye_ = 0.0, Lens_ = kNoLens, Roof_ = 0.0;
  bool Roofed_ = false;
};

}
#endif
