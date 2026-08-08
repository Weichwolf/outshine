/* Wo das Auge steht, aus dem, was Daten sind. Der Boden kommt aus dem DEM, das Dach aus OSM, und
 * beide koennen ein Objektiv begraben — ein Auge im Boden ist kein Standpunkt. */
#ifndef STANDPOINT_H
#define STANDPOINT_H

namespace outshine::World {

class Standpoint {
public:
  /* [SET] the least clearance at which an eye is outside a body. Two metres is a standing person's
   * eye and it is under the DEM's own sample error, so it corrects the lens as little as it can. */
  static constexpr double kMinClearM = 2.0;
  static constexpr double kNoRoof = -1.0e30;

  void SetGroundAslM(double m) { Ground_ = m; }
  /* The scene's own declaration: a height above whatever ground answers. */
  void SetEyeAglM(double m) { Eye_ = m; Lens_ = kNoLens; }
  /* THE LENS ALTITUDE, which is what a camera operator publishes. The height above ground follows
   * from the DEM, and where the DEM buries the lens the eye is lifted instead of moved. */
  void SetLensAslM(double m) { Lens_ = m; }
  void SetRoofAslM(double m) { Roof_ = m; }

  double GroundAslM() const { return Ground_; }
  double EyeAglM() const {
    double e = Lens_ > kNoLens ? Lens_ - Ground_ : Eye_;
    if (Lens_ > kNoLens && e < kMinClearM) e = kMinClearM;
    if (Roof_ > -1.0e29 && Ground_ + e < Roof_ + kMinClearM) e = Roof_ + kMinClearM - Ground_;
    return e;
  }
  double AltAslM() const { return Ground_ + EyeAglM(); }
  /* What the correction cost. A camera whose lift is not zero stands on data that misses its point,
   * and the page says so instead of showing a grey box. */
  double LiftM() const { return EyeAglM() - (Lens_ > kNoLens ? Lens_ - Ground_ : Eye_); }
  bool LensDeclared() const { return Lens_ > kNoLens; }

private:
  static constexpr double kNoLens = -1.0e8;
  double Ground_ = 0.0, Eye_ = 0.0, Lens_ = kNoLens, Roof_ = kNoRoof;
};

} // namespace outshine::World
#endif
