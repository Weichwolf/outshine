#include "Check.h"
#include "CppAllocationProbe.h"
#include "RoadPortFieldQuery.h"
#include "RoadPortFrame.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <ratio>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kHeightM = 260;
constexpr double kExtentM = 10000;
constexpr RoadPlane kGradient{.SlopeE = .03, .SlopeN = -.01};
constexpr std::array<RoadPortField::Plane, 2> kPlanes{
    {{.Origin = {}, .HeightM = kHeightM, .Gradient = kGradient},
     {.Origin = {}, .HeightM = kHeightM + 10, .Gradient = kGradient}}};
constexpr std::array<RoadPortField::Patch, 2> kPatches{
    {{.Parents = {0, 0}, .CutCentres = {}, .InwardNormals = {}},
     {.Parents = {1, 1}, .CutCentres = {}, .InwardNormals = {}}}};
constexpr EastNorth kA{.EastM = -kExtentM, .NorthM = -kExtentM};
constexpr EastNorth kB{.EastM = kExtentM, .NorthM = -kExtentM};
constexpr EastNorth kC{.EastM = -kExtentM, .NorthM = kExtentM};
constexpr EastNorth kD{.EastM = kExtentM, .NorthM = kExtentM};
constexpr std::array<RoadPortFace, 2> kFaces{
    {{.Triangle = {kA, kB, kC}, .GradeFrame = {}}, {.Triangle = {kB, kD, kC}, .GradeFrame = {}}}};

void CheckOwnFrame(const RoadPortField &field, LongitudeLatitude anchor) {
  auto query = RoadPortFieldQuery::Make(field, kFaces);
  CHECK(query.has_value(), "the borrowed full original domain is valid");
  if (!query) { return; }
  auto target = anchor;
  target.LatitudeDeg += anchor.LatitudeDeg > 80 ? -.02 : .02;
  target.LongitudeDeg += .02;
  if (target.LongitudeDeg > 180) { target.LongitudeDeg -= 360; }
  auto end = target;
  end.LongitudeDeg -= .001;
  const RoadPortFrameRequest request{.SourceAnchor = anchor,
                                     .PortAnchor = target,
                                     .OriginalSegment = {end, target},
                                     .HalfWidthM = 2,
                                     .InitialModelHeightM = 0,
                                     .MaximumLongitudinalGrade = .1,
                                     .MaximumCrossGrade = .1};
  const auto result = SampleRoadPortFrame(*query, request);
  CHECK(result.has_value(), "an upward plane intersects the port's own WGS84 height ray");
  if (!result) { return; }
  const auto sourceFrame = TangentFrame::At(anchor);
  const auto portFrame = TangentFrame::At(target);
  const Vec3 normal = sourceFrame.UpEcef() - sourceFrame.EastEcef() * kGradient.SlopeE -
                      sourceFrame.NorthEcef() * kGradient.SlopeN;
  const double divisor = Dot(normal, portFrame.UpEcef());
  const double expectedHeight =
      (kHeightM + Dot(normal, sourceFrame.OriginEcef() - portFrame.OriginEcef())) / divisor;
  const RoadPlane expectedPlane{.SlopeE = -Dot(normal, portFrame.EastEcef()) / divisor,
                                .SlopeN = -Dot(normal, portFrame.NorthEcef()) / divisor};
  CHECK(std::abs(result->Port.Gate.GradeM - expectedHeight) < 1e-8 &&
            std::abs(result->Port.Plane.SlopeE - expectedPlane.SlopeE) < 1e-12 &&
            std::abs(result->Port.Plane.SlopeN - expectedPlane.SlopeN) < 1e-12,
        "the iterative query agrees with an independent ECEF plane-ray intersection");
  CHECK(result->Port.Gate.EastM == 0 && result->Port.Gate.NorthM == 0 &&
            result->Port.Gate.HalfWidthM == request.HalfWidthM && result->RayIterations == 2 &&
            std::abs(result->RayResidualM) <= 1e-8,
        "the contact remains anchored in its own frame with terminal ray residual");
  auto reverse = request;
  std::ranges::reverse(reverse.OriginalSegment);
  const auto reversed = SampleRoadPortFrame(*query, reverse);
  CHECK(reversed && reversed->Port.Gate.GradeM == result->Port.Gate.GradeM &&
            reversed->Port.Plane.SlopeE == result->Port.Plane.SlopeE &&
            reversed->Port.Plane.SlopeN == result->Port.Plane.SlopeN &&
            reversed->Port.Gate.OutE == -result->Port.Gate.OutE &&
            reversed->Port.Gate.OutN == -result->Port.Gate.OutN &&
            reversed->LongitudinalGrade == -result->LongitudinalGrade &&
            reversed->CrossfallRad == -result->CrossfallRad,
        "reversing the original source reverses both signed grades without changing the plane");
  CHECK(query->Work().Queries == 4 && query->Work().FaceBoundsTests == 8,
        "both ray solves charge actual original-domain work");
}

void CheckDomains(const RoadPortField &field) {
  auto faces = std::array{kFaces[0], kFaces[0]};
  faces[0].Patch = 1;
  auto query = RoadPortFieldQuery::Make(field, faces);
  CHECK(query.has_value(), "overlapping faces retain explicit field ownership");
  if (!query) { return; }
  const auto a = query->At({});
  std::ranges::reverse(faces);
  auto reverse = RoadPortFieldQuery::Make(field, faces);
  const auto b = reverse->At({});
  CHECK(a && b && a->HeightM == kHeightM && a->HeightM == b->HeightM,
        "the lowest owner wins exactly on a shared boundary regardless of face order");
  const auto outside = query->At({.EastM = kExtentM + 1});
  CHECK(!outside && outside.error() == RoadPortQueryError::OutsideDomain,
        "analytic continuation cannot invent a port outside the bounded original surface");
  auto capped = RoadPortFieldQuery::Make(field, kFaces, {.Queries = 1, .FaceBoundsTests = 2});
  CHECK(capped && capped->At({}).has_value(), "one complete query is admitted");
  const auto refused = capped->At({});
  CHECK(!refused && refused.error() == RoadPortQueryError::CapacityExceeded &&
            capped->Work().Queries == 1 && capped->Work().FaceBoundsTests == 2,
        "a refused query does not perform additional face work");
  auto faceCap = RoadPortFieldQuery::Make(field, kFaces, {.FaceBoundsTests = 1});
  CHECK(faceCap && !faceCap->At({}) && faceCap->Work().Queries == 0,
        "the whole face scan is admitted before any query work");
  faces[0].Triangle[2] = faces[0].Triangle[1];
  CHECK(!RoadPortFieldQuery::Make(field, faces), "a degenerate input face is refused");
  faces[0] = kFaces[0];
  faces[0].Patch = 3;
  CHECK(!RoadPortFieldQuery::Make(field, faces), "a missing field owner is refused");
}

void CheckFailures(const RoadPortField &field) {
  auto query = RoadPortFieldQuery::Make(field, kFaces);
  RoadPortFrameRequest request{.SourceAnchor = {},
                               .PortAnchor = {},
                               .OriginalSegment = {{{}, {.LongitudeDeg = .001}}},
                               .HalfWidthM = 1,
                               .InitialModelHeightM = kHeightM,
                               .MaximumLongitudinalGrade = .1};
  const auto baseline = SampleRoadPortFrame(*query, request);
  CHECK(baseline && std::abs(baseline->LongitudinalGrade - kGradient.SlopeE) < 1e-12 &&
            std::abs(baseline->CrossfallRad + std::atan(kGradient.SlopeN)) < 1e-12,
        "eastward source direction preserves the specified right-positive crossfall");
  const auto before = query->Work();
  std::stop_source stop;
  stop.request_stop();
  const auto canceled = SampleRoadPortFrame(*query, request, stop.get_token());
  CHECK(!canceled && canceled.error() == RoadPortQueryError::Canceled &&
            query->Work().Queries == before.Queries,
        "an already canceled port consumes no field query");
  request.InitialModelHeightM = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = SampleRoadPortFrame(*query, request);
  CHECK(!invalid && invalid.error() == RoadPortQueryError::InvalidInput &&
            query->Work().Queries == before.Queries,
        "missing bound initial heights fail before querying the field");
  request.InitialModelHeightM = kHeightM;
  request.MaximumLongitudinalGrade = .001;
  const auto excessive = SampleRoadPortFrame(*query, request);
  CHECK(!excessive && excessive.error() == RoadPortQueryError::GradeExceeded,
        "physical longitudinal limits are checked in the requested port frame");
  request.MaximumLongitudinalGrade = .1;
  request.OriginalSegment[1] = request.OriginalSegment[0];
  const auto missingDirection = SampleRoadPortFrame(*query, request);
  CHECK(!missingDirection && missingDirection.error() == RoadPortQueryError::UnqualifiedFrame,
        "a missing source direction cannot become an invented tangent");
}

void CheckWork(const RoadPortField &field) {
  constexpr size_t kPorts = 1000;
  const RoadPortFrameRequest request{.SourceAnchor = {},
                                     .PortAnchor = {},
                                     .OriginalSegment = {{{}, {.LongitudeDeg = .001}}},
                                     .HalfWidthM = 1,
                                     .InitialModelHeightM = kHeightM,
                                     .MaximumLongitudinalGrade = .1};
  const auto source = TangentFrame::At(request.SourceAnchor);
  auto query = RoadPortFieldQuery::Make(
      field, kFaces, {.Queries = kPorts, .FaceBoundsTests = kPorts * kFaces.size()});
  bool valid = query.has_value();
  if (!query) { return; }
  BeginCppAllocationProbe();
  const auto begin = std::chrono::steady_clock::now();
  for (size_t port = 0; port < kPorts; ++port) {
    const auto position = RoadPortGroundPosition(source, {.EastM = static_cast<double>(port)});
    const auto result = SampleRoadPortFrame(*query, request);
    valid = valid && position.has_value() && result.has_value();
  }
  const auto elapsed = std::chrono::steady_clock::now() - begin;
  const auto allocations = EndCppAllocationProbe();
  CHECK(valid && allocations.Calls == 0 && allocations.Bytes == 0,
        "prepared field queries, frame conversion and terrain positions allocate no C++ storage");
  CHECK(query->Work().Queries == kPorts && query->Work().FaceBoundsTests == kPorts * kFaces.size(),
        "the repeated native control preserves bounded exact query-work accounting");
  Note("native port and ground-position control",
       std::chrono::duration<double, std::nano>(elapsed).count() / kPorts,
       "ns/port");
}
}

int main() {
  const auto field = RoadPortField::Make(kPlanes, kPatches);
  CHECK(field.has_value(), "the common analytic field is valid");
  if (field) {
    for (const auto anchor :
         std::array<LongitudeLatitude, 4>{{{.LongitudeDeg = 16, .LatitudeDeg = 48},
                                           {.LongitudeDeg = 179.99, .LatitudeDeg = 0},
                                           {.LongitudeDeg = -179.99, .LatitudeDeg = -70},
                                           {.LongitudeDeg = 40, .LatitudeDeg = 89}}}) {
      CheckOwnFrame(*field, anchor);
    }
    CheckDomains(*field);
    CheckFailures(*field);
    CheckWork(*field);
  }
  return Report();
}
