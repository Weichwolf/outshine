#include "Check.h"
#include "CppAllocationProbe.h"
#include "RoadGroundFacetAudit.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

PlanarMesh Mesh() {
  PlanarMesh mesh;
  mesh.Points = {{{0, 0}}, {{0, 1}}, {{1, 0}}, {{1, 1}}, {{2, 0}}, {{2, 1}}};
  mesh.Faces = {{{0, 2, 1}}, {{2, 3, 1}}, {{2, 4, 3}}, {{4, 5, 3}}};
  mesh.FaceParents = {0, 1, 2, 3};
  return mesh;
}

RoadGroundField Domain(const PlanarMesh &mesh, double inverseGrade) {
  RoadGroundField source;
  for (size_t at = 0; at < mesh.Faces.size(); ++at) {
    RoadPortFace face{.Patch = at < 2 ? uint32_t{0} : uint32_t{1},
                      .Triangle = {},
                      .GradeFrame = {inverseGrade, 0, 0, inverseGrade}};
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto &p = mesh.Points[mesh.Faces[at][corner]];
      face.Triangle[corner] = {.EastM = p[0], .NorthM = p[1]};
    }
    source.Faces.push_back(face);
  }
  return source;
}

auto Field(double rise) {
  const std::array<RoadPortField::Plane, 2> planes{
      {{.Origin = {}, .HeightM = 0, .Gradient = {}},
       {.Origin = {}, .HeightM = rise, .Gradient = {}}}};
  const std::array<RoadPortField::Patch, 2> patches{
      {{.Parents = {0, 1},
        .CutCentres = {{{}, {.EastM = 1}}},
        .InwardNormals = {{{.EastM = 1}, {.EastM = -1}}},
        .ScaleM = 1},
       {.Parents = {1, 1}, .CutCentres = {}, .InwardNormals = {}, .ScaleM = 1}}};
  return RoadPortField::Make(planes, patches);
}

void CheckContact() {
  const auto mesh = Mesh();
  const auto source = Domain(mesh, 20);
  const auto field = Field(.02);
  CHECK(field.has_value(), "two analytic patches share their entire C1 plane at x=1");
  if (!field) { return; }
  const auto blend = field->Evaluate(0, {.EastM = 1, .NorthM = .5});
  const auto hard = field->Evaluate(1, {.EastM = 1, .NorthM = .5});
  CHECK(blend && hard && blend->HeightM == hard->HeightM && blend->Gradient.SlopeE == 0 &&
            hard->Gradient.SlopeE == 0,
        "analytic heights and derivatives agree while coarse finite facets can disagree");
  const std::array heights{0., 0., .02, .02, .02, .02};
  const auto audit = AuditRoadGroundFacets(source, *field, mesh, heights);
  CHECK(audit && !audit->NumericalQualityMet(),
        "a valid continuous field cannot hide the finite shared-contact jump");
  if (!audit) { return; }
  CHECK(audit->MaximumNormalizedContinuousGrade <= 1 && audit->MaximumNormalizedFiniteGrade <= 1 &&
            audit->MaximumHeightChordErrorM <= .01,
        "this counterexample passes both gradient limits and the chord error limit");
  CHECK_NEAR(audit->MaximumContactGradientJump,
             .02,
             1e-14,
             "m/m",
             "the whole shared edge has the independently derived 0.02 finite gradient jump");
  CHECK(audit->SharedContactSegments == 1 && audit->BadFaces == 2 &&
            audit->Refinement == std::vector<uint8_t>({0, 1, 1, 0}),
        "both finite incident facets are marked without altering the field or observations");
}

void CheckBoundsAndPlane() {
  const auto mesh = Mesh();
  const auto source = Domain(mesh, 1);
  const auto curved = Field(.2);
  const auto plane = Field(0);
  CHECK(curved && plane, "curved and constant analytic controls are available");
  if (!curved || !plane) { return; }
  const std::array curvedHeights{0., 0., .2, .2, .2, .2};
  const auto audit = AuditRoadGroundFacets(source, *curved, mesh, curvedHeights);
  CHECK(audit && audit->MaximumHeightChordErrorM > .01 && audit->Refinement[0] &&
            audit->Refinement[1],
        "whole-face chord bounds reject the known 0.019245-m smoothstep chord deviation");
  const std::array zero{0., 0., 0., 0., 0., 0.};
  BeginCppAllocationProbe();
  const auto accepted = AuditRoadGroundFacets(source, *plane, mesh, zero);
  const auto allocation = EndCppAllocationProbe();
  Note("finite facet audit C++ allocations", static_cast<double>(allocation.Calls), "calls");
  Note("finite facet audit requested bytes", static_cast<double>(allocation.Bytes), "B");
  CHECK(accepted && accepted->NumericalQualityMet() && accepted->SharedContactSegments == 1 &&
            accepted->MaximumHeightChordErrorM <= 256 * std::numeric_limits<double>::epsilon() &&
            accepted->MaximumNormalizedFiniteGrade == 0 &&
            accepted->MaximumContactGradientJump == 0,
        "a flat shared surface passes with the existing Bernstein roundoff guard");
}

void CheckFiniteGradient() {
  PlanarMesh mesh;
  mesh.Points = {{{0, 0}}, {{1, 0}}, {{.25, .01}}};
  mesh.Faces = {{{0, 1, 2}}};
  mesh.FaceParents = {0};
  const auto source = Domain(mesh, 20);
  const auto field = Field(.02);
  CHECK(field.has_value(), "the slender-facet control has the same bounded analytic field");
  if (!field) { return; }
  const std::array heights{0., .02, .003125};
  const auto audit = AuditRoadGroundFacets(source, *field, mesh, heights);
  CHECK(audit && audit->MaximumNormalizedContinuousGrade <= 1 &&
            audit->MaximumNormalizedFiniteGrade > 3 && audit->BadFaces == 1 &&
            audit->SharedContactSegments == 0,
        "the independently derived finite gradient (0.02,-0.1875) exceeds the physical limit "
        "although the continuous gradient remains admissible");
  if (!audit) { return; }
  CHECK_NEAR(audit->MaximumNormalizedFiniteGrade,
             20 * std::hypot(.02, -.1875),
             1e-12,
             "ratio",
             "finite gradients use the actual three-dimensional triangle plane");
}

void CheckFailures() {
  CHECK(!RoadGroundFacetAudit{}.NumericalQualityMet(),
        "an unevaluated default report cannot claim numerical acceptance");
  auto mesh = Mesh();
  const auto source = Domain(mesh, 1);
  const auto field = Field(0);
  CHECK(field.has_value(), "the failure controls use an independently valid flat field");
  if (!field) { return; }
  std::array heights{0., 0., 0., 0., 0., 0.};
  BeginCppAllocationProbe();
  const auto capped = AuditRoadGroundFacets(source, *field, mesh, heights, {.ContainerBytes = 0});
  const auto allocation = EndCppAllocationProbe();
  CHECK(!capped && capped.error() == RoadGroundError::CapacityExceeded && allocation.Calls == 0,
        "workspace admission refuses the operation before any audit allocation");
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = AuditRoadGroundFacets(source, *field, mesh, heights, {}, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadGroundError::Canceled,
        "a canceled audit publishes no unchecked success");
  heights[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights), "nonfinite facet heights fail");
  heights[0] = .001;
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights),
        "finite geometry must sample the supplied field rather than invented caller heights");
  heights[0] = 0;
  mesh.Faces[0][0] = 9999;
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights), "invalid indices fail safely");
  mesh = Mesh();
  mesh.FaceParents[0] = 9999;
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights), "missing source ownership fails");
  mesh = Mesh();
  std::swap(mesh.Faces[0][0], mesh.Faces[0][1]);
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights), "reversed faces cannot pass");
  mesh = Mesh();
  mesh.Faces.push_back(mesh.Faces[0]);
  mesh.FaceParents.push_back(0);
  CHECK(!AuditRoadGroundFacets(source, *field, mesh, heights),
        "duplicate incident facets cannot pass a manifold edge check");
}
}

int main() {
  CheckContact();
  CheckBoundsAndPlane();
  CheckFiniteGradient();
  CheckFailures();
  return Report();
}
