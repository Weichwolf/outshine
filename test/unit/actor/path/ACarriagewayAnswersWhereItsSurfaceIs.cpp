#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Carriageway.h"

using outshine::Curve;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::Stand;
using outshine::Standing;

namespace {

constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kLengthM = 1000.0;
constexpr double kBankRad = 0.06;
constexpr double kGradeAt = 0.05;
constexpr double kHalfWidthM = 3.5;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine line;
  std::string error;
  const std::vector<Segment> along = {{Curve::Arc, kLengthM, kCurvature, kCurvature}};
  if (!line.Lay(Placed{}, along, error) ||
      !line.Rise({{0.0, 0.0, kGradeAt}, {kLengthM, kGradeAt * kLengthM, kGradeAt}}, error) ||
      !line.Bank({{0.0, kBankRad, 0.0}, {kLengthM, kBankRad, 0.0}}, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  Placed on;
  if (!line.At(500.0, on)) { return Report(); }
  const double left[2] = {-std::sin(on.HeadingRad), std::cos(on.HeadingRad)};

  const Standing centre = Stand(line, on.EastM, on.NorthM, kHalfWidthM, 500.0, 40.0);
  Note("the station it stood at", centre.AlongM, "m");
  Note("how far across the road that is", centre.AcrossM, "m");
  Note("the surface height there", centre.HeightM, "m");
  Note("the reference line's own height", on.HeightM, "m");
  CHECK(centre.On, "a point on the centreline stands on the road");
  CHECK_NEAR(centre.HeightM, on.HeightM, 1.0e-9, "m",
             "at exactly the reference line's height, because the cross-slope pivots about the line "
             "and a point on it is where the two agree");

  const double outM = 3.0;
  const Standing outer = Stand(line, on.EastM + left[0] * outM, on.NorthM + left[1] * outM,
                               kHalfWidthM, 500.0, 40.0);
  Note("how far across the outer point is", outer.AcrossM, "m");
  Note("how much lower the bank puts it", outer.HeightM - on.HeightM, "m");
  Note("what 3 m of a 0.06 rad bank drops", -outM * std::tan(kBankRad), "m");
  CHECK_NEAR(outer.AcrossM, outM, 1.0e-6, "m", "three metres to the left of the line");
  CHECK_NEAR(outer.HeightM - on.HeightM, -outM * std::tan(kBankRad), 1.0e-6, "m",
             "**AND THE SURFACE FOLLOWS THE BANK, WHICH IS WHAT MAKES A CROSS-SLOPE A SURFACE AND "
             "NOT A LABEL.** A wheel on the outside of a banked curve stands lower than one on the "
             "line, and by exactly the tangent of the declared angle -- so a car leans because the "
             "road does, and nothing had to be told to lean it");

  const Standing off = Stand(line, on.EastM + left[0] * 20.0, on.NorthM + left[1] * 20.0,
                             kHalfWidthM, 500.0, 40.0);
  CHECK(!off.On && std::fabs(off.AcrossM - 20.0) < 1.0e-6,
        "and twenty metres out it says it is OFF the road while still answering where it is -- "
        "leaving the carriageway is a fact to report, not a query that fails");

  const double up[3] = {0.0, 1.0, 0.0};
  const double lean =
      centre.NormalM[0] * up[0] + centre.NormalM[1] * up[1] + centre.NormalM[2] * up[2];
  const double gradeSlope = kGradeAt;
  const double bankSlope = std::tan(kBankRad);
  const double exactRad =
      std::acos(1.0 / std::sqrt(1.0 + gradeSlope * gradeSlope + bankSlope * bankSlope));
  Note("the angle the surface normal makes with straight up", std::acos(lean), "rad");
  Note("what a plane sloping by both at once makes", exactRad, "rad");
  Note("what a naive product of the two cosines would say",
       std::acos(std::cos(kBankRad) * std::cos(std::atan(kGradeAt))), "rad");

  CHECK_NEAR(std::acos(lean), exactRad, 1.0e-9, "rad",
             "**AND THE NORMAL CARRIES BOTH TILTS AT ONCE**, at acos(1 / sqrt(1 + grade^2 + "
             "bank^2)) -- the normal of a plane that slopes by one along and the other across. That "
             "is what a contact needs, because gravity resolves onto the surface and not onto "
             "whichever of the two was thought of last");
  CHECK(std::fabs(std::acos(lean) -
                  std::acos(std::cos(kBankRad) * std::cos(std::atan(kGradeAt)))) > 1.0e-6,
        "**AND IT IS NOT THE PRODUCT OF THE TWO COSINES, which is the answer that looks right.** "
        "That form assumes the along and across directions are perpendicular; on a road that climbs "
        "AND banks they are not -- their dot product is -grade times bank, 0.0030 here -- and the "
        "shortcut is 5.7e-5 rad out. Small, wrong, and exactly the sort of thing that is only found "
        "by deriving the number twice");

  CHECK(centre.NormalM[1] > 0.0, "and it points up rather than down, always");

  Covers("I.9.9 a carriageway answers where its surface is: a station, a distance across, a height "
         "that follows the cross-slope, and one normal carrying the grade and the bank together -- "
         "which is the whole of what a contact has to be told about the ground");
  return Report();
}
