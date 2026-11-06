#include "ProfiledRoadMesher.h"
#include "RoadCrossSection.h"
#include "RoadRibbonMesh.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

bool Same(const RoadMeshBuffers &a, const RoadMeshBuffers &b) {
  return a.PositionM == b.PositionM && a.NormalM == b.NormalM && a.ColourRgba == b.ColourRgba &&
         a.Index == b.Index;
}

Vec3f Position(const RoadMeshBuffers &mesh, size_t vertex) {
  return {
      {mesh.PositionM[vertex * 3], mesh.PositionM[vertex * 3 + 1], mesh.PositionM[vertex * 3 + 2]}};
}

void CheckContacts(const RoadMeshBuffers &mesh, const RoadSweep &surface) {
  const auto contacts = RoadRibbonContacts(surface);
  for (size_t end = 0; end < 2; ++end) {
    const auto &contact = contacts.EndRings[end];
    CHECK(contact, "the source declares its complete native contact");
    if (!contact) { continue; }
    const auto &ring = *contact;
    for (size_t band = 0; band < 3; ++band) {
      size_t edges = 0;
      for (size_t at = 0; at < mesh.Index.size(); at += 3) {
        for (size_t side = 0; side < 3; ++side) {
          const auto a = Position(mesh, mesh.Index[at + side]);
          const auto b = Position(mesh, mesh.Index[at + (side + 1) % 3]);
          if ((a == ring[band] && b == ring[band + 1]) ||
              (b == ring[band] && a == ring[band + 1])) {
            ++edges;
          }
        }
      }
      CHECK(edges == 1, "every original contact segment is an exact boundary edge");
    }
  }
}

void CheckSourceWords(const RoadMeshBuffers &fine, const RoadMeshBuffers &coarse) {
  for (size_t at = 0; at < coarse.PositionM.size() / 3; ++at) {
    bool found = false;
    for (size_t source = 0; source < fine.PositionM.size() / 3; ++source) {
      if (Position(fine, source) != Position(coarse, at)) { continue; }
      found = true;
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK(fine.NormalM[source * 3 + axis] == coarse.NormalM[at * 3 + axis],
              "retained source vertices carry the original analytic or port normal");
      }
      for (size_t channel = 0; channel < 4; ++channel) {
        CHECK(fine.ColourRgba[source * 4 + channel] == coarse.ColourRgba[at * 4 + channel],
              "retained source vertices carry the original material attributes");
      }
      break;
    }
    CHECK(found, "a directly emitted vertex is an existing canonical stored source point");
  }
}

void CheckCanonicalCornerCoverage() {
  const std::array stations{
      RoadStation{
          .EastM = -384.96673660802441, .NorthM = 356.84986042874698, .GradeM = 18.735145607098065},
      RoadStation{
          .EastM = -392.16435385303549, .NorthM = 349.83097371155657, .GradeM = 18.852845473021773},
      RoadStation{
          .EastM = -400.09824685268768, .NorthM = 342.09411498157488, .GradeM = 18.717976277724915},
      RoadStation{
          .EastM = -401.63502804741125, .NorthM = 343.62422807081936, .GradeM = 18.55314777097475}};
  const RoadSweep surface{.HalfWidthM = .89999997615814209,
                          .Profile = RoadProfile::Kerbed,
                          .WearsLinear = {{.2f, .3f, .4f}},
                          .Form = RibbonForm::Surface};
  const auto samples = std::span(stations);
  const auto section = RoadSection(surface.HalfWidthM, surface.Profile);
  const std::array gates{RoadEndGate(samples.first(2), true, section),
                         RoadEndGate(samples.subspan(1, 2), false, section)};
  auto end = RoadPortPositions({.Gate = gates[0], .Plane = {}});
  std::ranges::reverse(end);
  CHECK(end == RoadPortPositions({.Gate = gates[1], .Plane = {}}),
        "the nearly straight source corner has exactly shared stored contact profiles");
  RoadMeshBuffers joint;
  ProfiledRoadMesher{}.Junction(gates, {}, surface.WearsLinear, joint);
  CHECK(joint.Index.empty(), "already coincident contacts need no extra corner triangles");
  RoadMeshBuffers fine;
  RoadMeshBuffers retained;
  const auto original = ProfiledRoadMesher{}.Sweep(stations, surface, fine);
  const RoadDisplayRequest request{.ErrorM = 1e-30,
                                   .Limits = {.StationsMost = 256, .PoseQueriesMost = 512},
                                   .Policy = RoadDisplayPolicy::StoredSurfaceEnvelope};
  const auto display = ProfiledRoadMesher(request).Sweep(stations, surface, retained);
  CHECK(original.Pieces == 3 && original.Cuts == 2 && original.Refused == 0,
        "the captured canonical counterexample splits at its tight corners");
  CHECK(display.Pieces == original.Pieces && display.Cuts == original.Cuts &&
            display.Refused == original.Refused && Same(fine, retained),
        "coincident contacts preserve complete source coverage without requiring a spurious fan");
  auto upper = request;
  upper.ErrorM = 1;
  upper.Policy = RoadDisplayPolicy::UpperOnly;
  RoadMeshBuffers continuous;
  const auto upperStats = ProfiledRoadMesher(upper).Sweep(stations, surface, continuous);
  CHECK(upperStats.Pieces == original.Pieces && upperStats.Cuts == original.Cuts &&
            upperStats.Refused == 0 && !continuous.Index.empty(),
        "the continuous model also recognizes the exact stored join instead of requiring a fan");
}
}

int main() {
  CheckCanonicalCornerCoverage();
  const std::array stations{RoadStation{.EastM = 10000, .NorthM = 16000, .GradeM = 17},
                            RoadStation{.EastM = 11024, .NorthM = 16000, .GradeM = 17}};
  RoadSweep surface{.HalfWidthM = 3,
                    .Profile = RoadProfile::Simple,
                    .WearsLinear = {{.2f, .3f, .4f}},
                    .Form = RibbonForm::Surface};
  const auto section = RoadSection(surface.HalfWidthM, surface.Profile);
  for (size_t end = 0; end < 2; ++end) {
    surface.EndPorts[end] = RoadPort{.Gate = RoadEndGate(stations, end != 0, section), .Plane = {}};
  }
  RoadMeshBuffers fine;
  CHECK(ProfiledRoadMesher{}.Sweep(stations, surface, fine).Pieces == 1,
        "the canonical source is a valid complete surface");
  const RoadDisplayRequest request{.ErrorM = .01,
                                   .Limits = {.StationsMost = 128, .PoseQueriesMost = 256},
                                   .Policy = RoadDisplayPolicy::StoredSurfaceEnvelope};
  RoadMeshBuffers coarse;
  const auto built = ProfiledRoadMesher(request).Sweep(stations, surface, coarse);
  CHECK(built.Pieces == 1 && built.Refused == 0, "the complete stored-source product is generated");
  CHECK(fine.Index.size() == size_t{32} * 18 && coarse.Index.size() == size_t{32} * 6 + 12,
        "longitudinal stations remain; only transverse interior bands are removed");
  CHECK(fine.PositionM.size() == size_t{33} * 4 * 3 &&
            coarse.PositionM.size() == (size_t{33} * 2 + 4) * 3,
        "only needed source vertices and complete run ends are allocated");
  CheckContacts(coarse, surface);
  CheckSourceWords(fine, coarse);

  auto strict = request;
  strict.ErrorM = 1e-30;
  RoadMeshBuffers original;
  CHECK(ProfiledRoadMesher(strict).Sweep(stations, surface, original).Pieces == 1 &&
            Same(fine, original),
        "uncertified spans directly emit the identical original, without a coverage hole");
  strict.Limits.StationsMost = 2;
  auto preserved = coarse;
  const auto failed = ProfiledRoadMesher(strict).Sweep(stations, surface, preserved);
  CHECK(failed.Refused != 0 && failed.Pieces == 0 && Same(preserved, coarse),
        "a rejected request preserves the previously published complete streams");

  surface.Form = RibbonForm::ClosedShell;
  RoadMeshBuffers closed;
  RoadMeshBuffers retained;
  CHECK(ProfiledRoadMesher{}.Sweep(stations, surface, closed).Pieces == 1 &&
            ProfiledRoadMesher(request).Sweep(stations, surface, retained).Pieces == 1 &&
            Same(closed, retained),
        "the finite-surface policy retains closed bodies as the exact original");
  return Report();
}
