#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Carriageway.h"
#include "Ribbon.h"

using outshine::Curve;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Ribbon;
using outshine::Section;
using outshine::Segment;
using outshine::StandAt;
using outshine::Standing;
using outshine::Sweep;

namespace {

constexpr double kRadiusM = 400.0;
constexpr double kLengthM = 800.0;
constexpr double kBankRad = 0.06;
constexpr double kGrade = 0.03;
constexpr double kStepM = 2.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine line;
  std::string error;
  const std::vector<Segment> along = {{Curve::Arc, kLengthM, 1.0 / kRadiusM, 1.0 / kRadiusM}};
  if (!line.Lay(Placed{}, along, error) ||
      !line.Rise({{0.0, 0.0, kGrade}, {kLengthM, kGrade * kLengthM, kGrade}}, error) ||
      !line.Bank({{0.0, kBankRad, 0.0}, {kLengthM, kBankRad, 0.0}}, error)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  Section section;
  section.HalfWidthM = 3.75;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;

  const Ribbon ribbon = Sweep(line, section, 0.0, kLengthM, kStepM);
  if (!ribbon.Woven) { std::printf("REFUSED %s\n", ribbon.Error.c_str()); }
  CHECK(ribbon.Woven,
        "**THE CORRIDOR IS SWEPT INTO A SOLID AND NOT A SURFACE.** A carriageway has a width, a "
        "shoulder and a THICKNESS -- which is what gives a bridge deck a soffit for something to "
        "pass under, and what makes the clearance rule measurable at all");
  if (!ribbon.Woven) { return Report(); }

  Note("stations swept", (double)ribbon.Stations, "stations");
  Note("vertices", (double)ribbon.Vertices, "vertices");
  Note("triangles", (double)ribbon.Triangles, "triangles");
  Note("triangles per metre of road", (double)ribbon.Triangles / kLengthM, "per m");

  CHECK(ribbon.Vertices == ribbon.Stations * 8,
        "with eight vertices a station: four across the top and four under them");
  CHECK(ribbon.Triangles == (ribbon.Stations - 1) * 16,
        "and sixteen triangles a segment -- three quads on top, three underneath and one down each "
        "side, which is what closes a solid rather than leaving a sheet with a lining");

  size_t outOfRange = 0;
  for (const uint32_t at : ribbon.Index) {
    if (at >= ribbon.Vertices) { ++outOfRange; }
  }
  Note("indices pointing outside the vertex list", (double)outOfRange, "indices");
  CHECK(outOfRange == 0, "and every index names a vertex that exists");

  size_t apart = 0;
  double worstM = 0.0;
  for (size_t station = 0; station < ribbon.Stations; ++station) {
    const double atM = (double)station * kStepM;
    for (size_t which = 0; which < outshine::kRibbonAcross; ++which) {
      const size_t vertex = station * 8 + which;
      const double acrossM = ribbon.AcrossM[vertex];
      const Standing standing = StandAt(line, atM > kLengthM ? kLengthM : atM, acrossM, 0.0);
      const double drawnM = ribbon.PositionM[vertex * 3 + 1];
      const double byM = std::fabs(drawnM - standing.HeightM);
      if (byM > worstM) { worstM = byM; }
      if (byM > 1.0e-5) { ++apart; }
    }
  }
  Note("vertices whose drawn height differs from the driven one", (double)apart, "vertices");
  Note("the worst difference", worstM, "m");

  Note("what a float can resolve at this distance from the frame origin",
       (double)std::nextafter((float)500.0f, 1000.0f) - 500.0, "m");
  CHECK(apart == 0,
        "**AND WHAT IS DRAWN IS WHAT IS DRIVEN, TO THE PRECISION THE MESH IS STORED IN.** Every "
        "vertex of the top surface is placed by the same StandAt the contacts stand on -- not by a "
        "copy of the formula, by the function itself. The 9.3e-7 m that is left is the mesh being "
        "float where the physics is double, and it is a property of the STORAGE and not of two "
        "disagreeing statements. A road the physics agrees with but nobody can see, or one drawn "
        "that cannot be driven, is two roads");

  const size_t middle = (ribbon.Stations / 2) * 8;
  const double inner = ribbon.PositionM[(middle + 0) * 3 + 1];
  const double outer = ribbon.PositionM[(middle + 3) * 3 + 1];
  Note("how much the banked section drops across its full width", inner - outer, "m");
  Note("what the declared bank says it should",
       (2.0 * (section.HalfWidthM + section.ShoulderM)) * std::tan(kBankRad), "m");
  CHECK_NEAR(inner - outer,
             (2.0 * (section.HalfWidthM + section.ShoulderM)) * std::tan(kBankRad), 1.0e-5, "m",
             "and the drawn cross-slope is the declared one, so a car leans because the road it is "
             "drawn on leans");

  const double under = ribbon.PositionM[(middle + 4) * 3 + 1];
  Note("how far the soffit sits below the surface", inner - under, "m");
  CHECK_NEAR(inner - under, section.ThicknessM * std::cos(kBankRad) * std::cos(std::atan(kGrade)),
             1.0e-3, "m",
             "**AND THE SOFFIT IS A THICKNESS ALONG THE SURFACE NORMAL**, not a drop in height -- on "
             "a road that banks and climbs those are different, and the one a bridge is measured "
             "under is the normal");

  Section noThickness = section;
  noThickness.ThicknessM = 0.0;
  const Ribbon flat = Sweep(line, noThickness, 0.0, kLengthM, kStepM);
  CHECK(!flat.Woven, "a section with no thickness is refused, because a road is not a sheet");
  std::printf("REFUSAL %s\n", flat.Error.c_str());

  const Ribbon backwards = Sweep(line, section, 500.0, 100.0, kStepM);
  CHECK(!backwards.Woven, "and a ribbon swept backwards is refused rather than reversed");
  std::printf("REFUSAL %s\n", backwards.Error.c_str());

  Covers("I.9.13 the corridor sweeps into a solid whose top surface is placed by the same function "
         "the wheels stand on, with a shoulder either side and a soffit a thickness below along the "
         "surface normal");
  return Report();
}
