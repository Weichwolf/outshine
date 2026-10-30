#include "Check.h"
#include "CppAllocationProbe.h"
#include "RoadGroundMesh.h"
#include "RoadGroundMeshInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::array<LongitudeLatitude, 3> kAxis{
    {{}, {.LongitudeDeg = .00045}, {.LongitudeDeg = .0009}}};

RoadGroundField Domain(const PlanarMesh &mesh) {
  RoadGroundField source;
  source.Triangles = mesh.Faces;
  for (const auto &p : mesh.Points) {
    source.Priors.push_back({.Patch = 0, .At = {.EastM = p[0], .NorthM = p[1]}, .HeightM = 0});
  }
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    RoadPortFace face{
        .Patch = static_cast<uint32_t>(at), .Triangle = {}, .GradeFrame = {1, 0, 0, 1}};
    for (size_t c = 0; c < 3; ++c) { face.Triangle[c] = source.Priors[mesh.Faces[at][c]].At; }
    source.Faces.push_back(face);
  }
  return source;
}

void CheckCoverageCounterexamples() {
  PlanarMesh original;
  original.Points = {{{0, 0}}, {{2, 0}}, {{0, 2}}};
  original.Faces = {{{0, 1, 2}}};
  const auto source = Domain(original);
  PlanarMesh children;
  children.Points = original.Points;
  children.Points.push_back({2. / 3, 2. / 3});
  children.Faces = {{{0, 1, 3}}, {{1, 2, 3}}, {{2, 0, 3}}};
  children.PointRemap = {0, 1, 2};
  children.FaceParents = {0, 0, 0};
  RoadGroundMeshWork work;
  CHECK(RoadGround::CheckMeshCoverage(source, children, work, {}).has_value(),
        "three positive centroid children retain the complete original triangle");
  children.Faces[1] = children.Faces[0];
  CHECK(!RoadGround::CheckMeshCoverage(source, children, work, {}),
        "equal total area and retained original anchors do not hide a local overlap and hole");
  CHECK(work.MaximumRootAreaErrorM2 <= 1e-6,
        "the overlap counterexample explicitly passes the weaker per-root area check");
  original.Points = {{{0, 0}}, {{1, 0}}, {{0, 1}}, {{1, 1}}};
  original.Faces = {{{0, 1, 2}}, {{1, 3, 2}}};
  const auto connected = Domain(original);
  children.Points = original.Points;
  children.Points.push_back({.5, .5});
  children.Faces = {{{0, 1, 4}}, {{0, 4, 2}}, {{1, 3, 4}}, {{3, 2, 4}}};
  children.PointRemap = {0, 1, 2, 3};
  children.FaceParents = {0, 0, 1, 1};
  CHECK(!RoadGround::CheckMeshCoverage(connected, children, work, {}),
        "valid subdivided coverage cannot replace a protected whole original shared port");
}

void CheckGeometry() {
  const auto strip = PrepareRoadGroundStrip(kAxis, 2, .05);
  CHECK(strip.has_value(), "the original bounded source axis is valid");
  if (!strip) { return; }
  std::vector<double> heights;
  for (const auto point : strip->TerrainQueries) {
    const double t = point.LongitudeDeg / kAxis.back().LongitudeDeg;
    heights.push_back(12 + 2 * std::min(t, 1 - t));
  }
  const auto source = BindRoadGroundField(*strip, heights);
  CHECK(source.has_value(), "all original terrain observations are bound");
  if (!source) { return; }
  const auto fit = FitRoadPortField(source->FitInput(.1), {});
  CHECK(fit.has_value(), "the original continuous C1 fit is valid");
  if (!fit) { return; }
  const auto mesh = BuildRoadGroundMesh(*source, fit->Field);
  CHECK(mesh && mesh->Audit.NumericalQualityMet(),
        "all finite gradient, chord and contact limits pass");
  if (!mesh) { return; }
  CHECK(mesh->Work.CoverageErrorM2 <= 1e-6 && mesh->Work.MaximumRootAreaErrorM2 <= 1e-6,
        "independent GEOS coverage and every original root area remain complete");
  for (size_t at = 0; at < source->Priors.size(); ++at) {
    const auto &prior = source->Priors[at];
    CHECK(mesh->Surface.PointRemap[at] == at &&
              mesh->Surface.Points[at] == PlanarPoint({prior.At.EastM, prior.At.NorthM}),
          "each original protected anchor retains its exact coordinates and index");
  }
  auto missing = mesh->Surface;
  missing.Faces.pop_back();
  missing.FaceParents.pop_back();
  RoadGroundMeshWork work;
  CHECK(!RoadGround::CheckMeshCoverage(*source, missing, work, {}),
        "a missing child face cannot pass original-domain coverage");
  auto duplicate = mesh->Surface;
  duplicate.Faces.push_back(duplicate.Faces.front());
  duplicate.FaceParents.push_back(duplicate.FaceParents.front());
  CHECK(!RoadGround::CheckMeshCoverage(*source, duplicate, work, {}),
        "a duplicated child cannot pass original-domain coverage");
  auto displaced = mesh->Surface;
  displaced.Points.front()[0] += .001;
  CHECK(!RoadGround::CheckMeshCoverage(*source, displaced, work, {}),
        "protected original anchors cannot be moved by numeric cleanup");
  auto remapped = mesh->Surface;
  remapped.PointRemap.front() = 1;
  CHECK(!RoadGround::CheckMeshCoverage(*source, remapped, work, {}),
        "protected original anchors cannot be silently remapped");
  BeginCppAllocationProbe();
  const auto capped = BuildRoadGroundMesh(*source, fit->Field, {.ContainerBytes = 0});
  const auto allocations = EndCppAllocationProbe();
  CHECK(!capped && capped.error() == RoadGroundError::CapacityExceeded && allocations.Calls == 0,
        "workspace admission happens before any slicing allocation");
  const auto insufficient = BuildRoadGroundMesh(*source, fit->Field, {.StationsPerPatch = 2});
  CHECK(!insufficient && insufficient.error() == RoadGroundError::CapacityExceeded,
        "insufficient parameter capacity publishes no unchecked mesh");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = BuildRoadGroundMesh(*source, fit->Field, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadGroundError::Canceled,
        "cancellation publishes no partial mesh");
  auto invalid = *source;
  invalid.Triangles.front()[0] = 99999;
  CHECK(!BuildRoadGroundMesh(invalid, fit->Field),
        "invalid original indices are refused before source access");
  invalid = *source;
  invalid.Priors.front().At.EastM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BuildRoadGroundMesh(invalid, fit->Field),
        "nonfinite source coordinates fail before parameter slicing");
}
}

int main() {
  CheckGeometry();
  CheckCoverageCounterexamples();
  return Report();
}
