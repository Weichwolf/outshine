#include "src/base/curve/Ribbon.h"
#include "Check.h"

#include <array>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const double curvature : {0.0, 0.02}) {
    for (const double bank : {-0.12, 0.0, 0.12}) {
      ReferenceLine line;
      std::string error;
      CHECK(line.Lay({},
                     {{.Shape = curvature == 0 ? Curve::Straight : Curve::Arc,
                       .LengthM = 20,
                       .EntryCurvature = curvature,
                       .ExitCurvature = curvature}},
                     error),
            "a straight or curved reference is laid");
      CHECK(line.Rise({{.AlongM = 0, .Value = 0, .RatePerM = 0.1},
                       {.AlongM = 20, .Value = 2, .RatePerM = 0.1}},
                      error),
            "the shared height profile is declared");
      CHECK(line.Bank({{.AlongM = 0, .Value = bank}, {.AlongM = 20, .Value = bank}}, error),
            "the shared bank is declared");
      for (const double shoulder : {0.0, 1.0}) {
        const Section section{.HalfWidthM = 3, .ShoulderM = shoulder, .ThicknessM = 0.5};
        const auto closed = Sweep(line, section, 0, 20, 2);
        const auto surface = Sweep(line, section, 0, 20, 2, RibbonForm::Surface);
        CHECK(closed.Woven && surface.Woven, "both products accept the same valid alignment");
        CHECK(surface.OriginM == closed.OriginM && surface.Stations == closed.Stations,
              "surface selection preserves placement and sampling");
        CHECK(surface.Vertices == surface.Stations * kRibbonAcross,
              "only upper vertices are generated");
        CHECK(surface.Triangles == (surface.Stations - 1) * (shoulder > 0 ? 6u : 2u),
              "only the declared upper bands produce triangles");
        CHECK(surface.Vertices < closed.Vertices && surface.Triangles < closed.Triangles,
              "the surface avoids generating lower, wall and end geometry");
        for (size_t station = 0; station < surface.Stations; ++station) {
          for (size_t across = 0; across < kRibbonAcross; ++across) {
            const size_t upper = station * kRibbonAcross + across;
            const size_t solid = station * (kRibbonAcross * 2 + 4) + across;
            for (size_t axis = 0; axis < 3; ++axis) {
              CHECK(surface.PositionM[upper * 3 + axis] == closed.PositionM[solid * 3 + axis],
                    "every exposed position matches the closed model exactly");
              CHECK(surface.NormalM[upper * 3 + axis] == closed.NormalM[solid * 3 + axis],
                    "every exposed shading normal matches exactly");
            }
            CHECK(surface.AcrossM[upper] == closed.AcrossM[solid],
                  "cross-section coordinates match");
          }
        }
        std::vector<uint32_t> expected;
        const size_t bodyVertices = closed.Stations * (kRibbonAcross * 2 + 4);
        for (size_t triangle = 0; triangle < closed.Index.size(); triangle += 3) {
          bool top = true;
          for (size_t corner = 0; corner < 3; ++corner) {
            const auto vertex = closed.Index[triangle + corner];
            top = top && vertex < bodyVertices && vertex % (kRibbonAcross * 2 + 4) < kRibbonAcross;
          }
          if (!top) { continue; }
          for (size_t corner = 0; corner < 3; ++corner) {
            const auto vertex = closed.Index[triangle + corner];
            expected.push_back(
                static_cast<uint32_t>(vertex / (kRibbonAcross * 2 + 4) * kRibbonAcross +
                                      vertex % (kRibbonAcross * 2 + 4)));
          }
        }
        CHECK(surface.Index == expected, "all upper faces retain exact connectivity and winding");
      }
    }
  }
  return Report();
}
