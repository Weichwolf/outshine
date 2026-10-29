#include "TransportRoadPageMeshing.h"
#include "TransportPlanFixture.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void Triangle(RoadMeshBuffers &mesh) {
  mesh.PositionM.insert(mesh.PositionM.end(), {0, 0, 0, 1, 0, 0, 0, 0, -1});
  mesh.Index.insert(mesh.Index.end(), {0, 1, 2});
}

class RecordingMesher final : public RoadMesher {
public:
  explicit RecordingMesher(const TransportPlan &plan) : Plan(plan) {}

  RoadMeshingStats Sweep(std::span<const RoadStation> stations,
                         RoadSweep how,
                         RoadMeshBuffers &mesh) const override {
    ++Sweeps;
    const auto &path = Plan.Paths[1];
    const auto &port = how.EndPorts[1];
    Borrowed = Borrowed && stations.data() == Plan.Stations.data() + path.FirstStation &&
               stations.size() == path.StationCount && how.HalfWidthM == path.HalfWidthM && port &&
               port->Gate.EastM == Plan.Junctions[0].Gates[1].EastM;
    Triangle(mesh);
    return {.Pieces = 1, .Rejections = {}};
  }

  void Junction(std::span<const RoadGate> gates,
                RoadPlane plane,
                const Vec3f &wearsLinear,
                RoadMeshBuffers &mesh) const override {
    ++Junctions;
    Borrowed = Borrowed && gates.data() == Plan.Junctions[0].Gates.data() &&
               gates.size() == Plan.Junctions[0].Gates.size() &&
               plane.SlopeE == Plan.Junctions[0].Plane.SlopeE && wearsLinear[0] == 1;
    Triangle(mesh);
  }

  const TransportPlan &Plan;
  mutable size_t Sweeps = 0;
  mutable size_t Junctions = 0;
  mutable bool Borrowed = true;
};
}

int main() {
  auto plan = TransportPlanFixture();
  CHECK(plan.has_value(), "the complete stored-data fixture is available");
  if (!plan) { return Report(); }
  const RecordingMesher mesher(*plan);
  std::array selected{RoadPagePartAppearance{
      .Part = {.Kind = RoadPagePartKind::Interval, .Index = 1}, .WearsLinear = {{1, 1, 1}}}};
  const auto road = MeshTransportRoadPage(mesher, *plan, selected);
  CHECK(road && mesher.Sweeps == 1 && mesher.Junctions == 0 && mesher.Borrowed,
        "only the selected interval is swept directly from immutable station storage");
  selected[0].Part = {.Kind = RoadPagePartKind::Junction, .Index = 0};
  const auto junction = MeshTransportRoadPage(mesher, *plan, selected);
  CHECK(junction && mesher.Sweeps == 1 && mesher.Junctions == 1 && mesher.Borrowed,
        "junction gates are borrowed while unselected intervals are never swept");
  plan->Paths[1].Body = 0;
  selected[0].Part = {.Kind = RoadPagePartKind::Interval, .Index = 1};
  const auto body = MeshTransportRoadPage(mesher, *plan, selected);
  CHECK(body && !body->Mesh.Index.empty() && mesher.Sweeps == 1 && mesher.Junctions == 1,
        "a stored closed body emits directly without rebuilding or invoking the sweep");
  selected[0].Part.Index = std::numeric_limits<uint32_t>::max();
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected), "unknown path references remain errors");
  selected[0].Part.Index = 1;
  const std::array duplicate{selected[0], selected[0]};
  CHECK(!MeshTransportRoadPage(mesher, *plan, duplicate),
        "duplicate selections cannot duplicate work");
  std::stop_source stopped;
  stopped.request_stop();
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected, stopped.get_token()),
        "cancellation prevents selected geometry from being emitted");
  plan->Coverage = TransportCoverage::Partial;
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected),
        "unqualified partial data cannot fulfill a complete functional page request");
  constexpr auto subset = TransportRepresentation::FunctionalSubset;
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected, {}, subset),
        "partial coverage under a complete representation does not become an explicit subset");
  plan->Definition.Representation = subset;
  plan->Definition.RepresentationVersion = 1;
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected, {}, subset),
        "a subset without its declared omissions cannot satisfy the mesher contract");
  plan->Omissions.push_back({.SourceId = "fixture/omitted",
                             .Reason = TransportOmissionReason::UnsupportedRepresentation});
  const auto subsetBody = MeshTransportRoadPage(mesher, *plan, selected, {}, subset);
  CHECK(subsetBody && body && subsetBody->Mesh.PositionM == body->Mesh.PositionM &&
            subsetBody->Mesh.NormalM == body->Mesh.NormalM &&
            subsetBody->Mesh.Index == body->Mesh.Index,
        "an explicitly qualified subset emits its original body with identical geometry");
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected),
        "a valid explicit subset still cannot satisfy the default complete request");
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected, stopped.get_token(), subset),
        "the explicit subset obeys the same cancellation boundary");
  CHECK(!MeshTransportRoadPage(mesher, *plan, duplicate, {}, subset),
        "the explicit subset cannot duplicate selected geometry");
  plan->Network.reset();
  CHECK(!MeshTransportRoadPage(mesher, *plan, selected, {}, subset),
        "a geometry-only partial plan cannot satisfy a functional subset request");
  return Report();
}
