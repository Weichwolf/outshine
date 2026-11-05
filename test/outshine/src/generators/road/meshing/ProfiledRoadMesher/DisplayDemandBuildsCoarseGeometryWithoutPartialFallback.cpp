#include "ProfiledRoadMesher.h"
#include "RoadRibbonMesh.h"
#include "RoadCrossSection.h"
#include "Check.h"
#include <array>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array stations{RoadStation{.EastM = 0, .NorthM = 0, .GradeM = 0},
                            RoadStation{.EastM = 1024, .NorthM = 0, .GradeM = 0}};
  RoadSweep surface{.HalfWidthM = 3,
                    .Profile = RoadProfile::Simple,
                    .WearsLinear = {{0.2f, 0.3f, 0.4f}},
                    .Form = RibbonForm::Surface};
  const Section section = RoadSection(surface.HalfWidthM, surface.Profile);
  for (size_t end = 0; end < 2; ++end) {
    surface.EndPorts[end] = RoadPort{.Gate = RoadEndGate(stations, end != 0, section), .Plane = {}};
  }
  RoadMeshBuffers fine;
  const auto original = ProfiledRoadMesher{}.Sweep(stations, surface, fine);
  CHECK(original.Pieces == 1 && original.Refused == 0, "the fine reference remains valid");
  const ProfiledRoadMesher display(
      RoadDisplayRequest{.ErrorM = 1, .Limits = {.StationsMost = 8, .PoseQueriesMost = 256}});
  RoadMeshBuffers coarse;
  const auto generated = display.Sweep(stations, surface, coarse);
  CHECK(generated.Pieces == 1 && generated.Refused == 0 && generated.Cuts == 0,
        "the existing mesher directly generates the complete requested display product");
  CHECK(coarse.PositionM.size() == 2 * kRibbonAcross * 3 && coarse.Index.size() == 18,
        "the coarse straight Upper has only two rings and six triangles");
  CHECK(fine.Index.size() == 32 * coarse.Index.size(),
        "this exact 1024m straight control removes fixed 32m subdivision without fine reduction");
  const auto contacts = RoadRibbonContacts(surface);
  for (size_t end = 0; end < 2; ++end) {
    const auto &ring = contacts.EndRings[end];
    CHECK(ring.has_value(), "both complete contacts exist");
    if (!ring || coarse.PositionM.size() != 2 * kRibbonAcross * 3) { continue; }
    for (size_t across = 0; across < kRibbonAcross; ++across) {
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK(coarse.PositionM[(end * kRibbonAcross + across) * 3 + axis] == (*ring)[across][axis],
              "coarse output retains the exact native contact words");
      }
    }
  }
  const std::array corner{RoadStation{.EastM = 0, .NorthM = 0, .GradeM = 0},
                          RoadStation{.EastM = 100, .NorthM = 0, .GradeM = 0},
                          RoadStation{.EastM = 100, .NorthM = 100, .GradeM = 0}};
  RoadSweep cornerSurface = surface;
  cornerSurface.EndPorts = {};
  const ProfiledRoadMesher limited(
      RoadDisplayRequest{.ErrorM = 1, .Limits = {.StationsMost = 2, .PoseQueriesMost = 256}});
  RoadMeshBuffers preserved = fine;
  const auto rejected = limited.Sweep(corner, cornerSurface, preserved);
  CHECK(rejected.Cuts > 0, "the budget countercase reaches a split after its first valid piece");
  CHECK(rejected.Refused > 0 && rejected.Pieces == 0,
        "exhaustion after a fitted piece cannot publish a partial coarse replacement");
  CHECK(preserved.PositionM == fine.PositionM && preserved.NormalM == fine.NormalM &&
            preserved.ColourRgba == fine.ColourRgba && preserved.Index == fine.Index,
        "all previously published mesh streams survive a failed display append unchanged");
  surface.Form = RibbonForm::ClosedShell;
  CHECK(display.Sweep(stations, surface, preserved).Refused > 0 && preserved.Index == fine.Index,
        "unqualified closed-body display requests keep the original instead of silently emitting "
        "Fine");
  return Report();
}
