#include "Check.h"
#include "SurfaceHeightTopology.h"
#include <array>
#include <cstddef>
#include <expected>
#include <limits>
#include <stop_token>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

void Rejected(const std::expected<SurfaceHeightTopology, SurfaceHeightTopologyError> &result,
              SurfaceHeightTopologyError reason) {
  CHECK(!result && result.error() == reason,
        "a refused height topology never exposes partial port unions");
}
}

int main() {
  using Error = SurfaceHeightTopologyError;
  const auto empty = BuildSurfaceHeightTopology({}, 0, {}, 0);
  CHECK(empty && empty->Points.empty() && empty->Sources.empty(),
        "an empty request retains the neutral empty result");
  PlanarArrangement mesh;
  mesh.Mesh.Points = {{0, 0}, {4, 0}, {0, 4}};
  mesh.Mesh.Faces = {{0, 1, 2}};
  mesh.FaceDomains = {3};
  const std::array ports{SurfaceHeightPort{
      .Region = {.Rings = {{{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}}}, .Domains = {0, 1}}};
  Rejected(BuildSurfaceHeightTopology(mesh, 1, ports, 0), Error::InvalidInput);
  Rejected(BuildSurfaceHeightTopology(mesh, 2, ports, -1), Error::InvalidInput);
  auto bad = ports;
  bad[0].Domains = {0, 0};
  Rejected(BuildSurfaceHeightTopology(mesh, 2, bad, 0), Error::InvalidInput);
  bad = ports;
  bad[0].Region.Rings[0] = {{0, 0}, {1, 1}, {0, 1}, {1, 0}};
  Rejected(BuildSurfaceHeightTopology(mesh, 2, bad, 0), Error::InvalidInput);
  auto invalid = mesh;
  invalid.Mesh.Points[0][0] = std::numeric_limits<double>::quiet_NaN();
  Rejected(BuildSurfaceHeightTopology(invalid, 2, ports, 0), Error::InvalidInput);
  std::stop_source canceled;
  canceled.request_stop();
  Rejected(BuildSurfaceHeightTopology(mesh, 2, ports, 0, {}, canceled.get_token()),
           Error::Canceled);
  for (size_t which = 0; which < 5; ++which) {
    SurfaceHeightTopologyBudget budget;
    switch (which) {
      case 0: budget.PlanarVertices = 2; break;
      case 1: budget.SourceVertices = 5; break;
      case 2: budget.SourceFaces = 1; break;
      case 3: budget.PortPoints = 3; break;
      default: budget.PortCandidates = 0; break;
    }
    Rejected(BuildSurfaceHeightTopology(mesh, 2, ports, 0, budget), Error::CapacityExceeded);
  }
  return Report();
}
