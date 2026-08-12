/* HOW HIGH THE EYE IS, out of what is data — the vertical column under one standpoint and nothing
 * about where that standpoint is. The ground comes from the DEM and the roof from OSM, and either of
 * them can bury a lens; an eye inside the ground is not a place anyone stands. */
#ifndef EYECOLUMN_H
#define EYECOLUMN_H

namespace outshine::World {

class EyeColumn {
public:
  /* [SET] the least clearance at which an eye is outside a body. Two metres is a standing person's
   * eye and it is under the DEM's own sample error, so it corrects the lens as little as it can. */
  static constexpr double kMinClearM = 2.0;

  void SetGroundAslM(double m) { Ground_ = m; }
  /* The scene's own declaration: a height above whatever ground answers. */
  void SetEyeAglM(double m) { Eye_ = m; Lens_ = kNoLens; }
  /* THE LENS ALTITUDE, which is what a camera operator publishes. The height above ground follows
   * from the DEM, and where the DEM buries the lens the eye is lifted instead of moved. */
  void SetLensAslM(double m) { Lens_ = m; }
  /* Called only where a roof stands, which is why there is no value for "none". */
  void SetRoofAslM(double m) { Roof_ = m; Roofed_ = true; }

  double GroundAslM() const { return Ground_; }
  double EyeAglM() const {
    double e = Lens_ > kNoLens ? Lens_ - Ground_ : Eye_;
    if (Lens_ > kNoLens && e < kMinClearM) e = kMinClearM;
    if (Roofed_ && Ground_ + e < Roof_ + kMinClearM) e = Roof_ + kMinClearM - Ground_;
    return e;
  }
  double AltAslM() const { return Ground_ + EyeAglM(); }
  /* What the correction cost. A camera whose lift is not zero stands on data that misses its point,
   * and the page says so instead of showing a grey box. */
  double LiftM() const { return EyeAglM() - (Lens_ > kNoLens ? Lens_ - Ground_ : Eye_); }
  [[nodiscard]] bool LensDeclared() const { return Lens_ > kNoLens; }

private:
  static constexpr double kNoLens = -1.0e8;
  double Ground_ = 0.0, Eye_ = 0.0, Lens_ = kNoLens, Roof_ = 0.0;
  bool Roofed_ = false;
};

} // namespace outshine::World
#endif
