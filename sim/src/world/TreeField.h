/* Where trees stand. A FUNCTION of the place, not a list: the same arithmetic answers "what stands in
 * this cell" for the picture and "is there a trunk here" for a body, and neither side stores it. */
#ifndef TREEFIELD_H
#define TREEFIELD_H

#include <cstdint>
#include <vector>

namespace outshine::World {

class ClassStructure;
class VegetationTemplates;

class TreeField {
public:
  /* east, north, foot above eye height, yaw, size factor — five values per tree, the layout TreeStage
   * expects. `sizeSigma` is the species' relative spread of stand height. */
  /* `dist` comes out beside `out` and `out` comes out SORTED BY IT, near to far: a level of detail is
   * then a contiguous range of instances and choosing it is a binary search instead of a per-frame
   * pass over the field. */
  /* NO LENS SITS INSIDE A CROWN. `crown` is the species' crown in tree heights — half width, bottom,
   * top — and every stand whose crown contains the eye is dropped. A pedestrian at 1.7 m is below any
   * crown base and loses nothing; a webcam on a 14 m mast stands in the middle of one, and in reality
   * there is no tree there. */
  struct Crown {
    float HalfWidth = 0.0f, Bottom = 0.0f, Top = 0.0f, HeightM = 0.0f;
  };

  /* THE GROUND, WITH THE STEP IT ANSWERS ON. `At` gives the height at a place in ENU metres; without
   * it every stand would sit at the camera's eye height — on a slope that means inside the rock or up
   * in the air. `PostM` is that surface's own node spacing: the slope stencil has to step exactly one
   * posting, and a stencil that steps less re-reads one triangle and reads its slope as zero. The two
   * travel together because a spacing that belongs to a different surface than `At` is a wrong
   * number that still compiles. */
  struct GroundOracle {
    double (*At)(void *user, double eastM, double northM) = nullptr;
    void *User = nullptr;
    double PostM = 0.0;
  };

  /* EVERY CELL THE SCATTER LOOKS AT LEAVES THROUGH EXACTLY ONE OF THESE. A refusal that is not one of
   * them cannot be written: the decision is one function and its return type is this. */
  enum class Refusal {
    Placed, OutsideRadius, NoTemplate, ZeroDensity, DensityDraw, NoGround, AboveTreeline, TooSteep,
    WoodyDraw, InCrown, Count
  };
  static const char *RefusalName(Refusal r);

  /* `camLatDeg` places the treeline: it falls 58.8 m per degree of latitude (world/AlpineLimit.h) and
   * over the 900 m radius that is 0.5 m, so the camera's own latitude carries the whole disc. */
  void Scatter(const ClassStructure &cls, const VegetationTemplates &veg, double radiusM,
               double eyeE, double eyeN, double eyeAsl, double camLatDeg, float sizeSigma,
               const Crown &crown, const GroundOracle &ground,
               std::vector<float> &out, std::vector<float> &dist) const;

  static constexpr int kStandFloats = 5;

  long Count() const { return Counts_[(int)Refusal::Placed]; }
  long Cells(Refusal r) const { return Counts_[(int)r]; }
  double HighestStandAslM() const { return HighestAsl_; }

private:
  mutable long Counts_[(int)Refusal::Count] = {};
  mutable double HighestAsl_ = -1.0e30;
};

} // namespace outshine::World
#endif
