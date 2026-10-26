#include "Check.h"
#include "PlanarArrangement.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

void Rejected(const std::expected<PlanarArrangement, PlanarArrangementError> &result,
              PlanarArrangementError reason) {
  CHECK(!result && result.error() == reason, "a refused arrangement publishes no partial product");
}
}

int main() {
  using Error = PlanarArrangementError;
  const auto empty = ArrangePlanarDomains({}, {}, {});
  CHECK(empty && empty->Mesh.Faces.empty() && empty->FaceDomains.empty(),
        "an empty request is a valid empty product");
  const std::array square{PlanarDomain{.Rings = {{{0, 0}, {4, 0}, {4, 4}, {0, 4}}}}};
  const std::array cross{PlanarLine{{-1, 2}, {5, 2}}, PlanarLine{{2, -1}, {2, 5}}};
  const std::array bad{PlanarDomain{.Rings = {{{0, 0}, {4, 4}, {0, 4}, {4, 0}}}}};
  Rejected(ArrangePlanarDomains(bad, {}, {}), Error::InvalidInput);
  auto nonfinite = square;
  nonfinite[0].Rings[0][0][0] = std::numeric_limits<double>::quiet_NaN();
  Rejected(ArrangePlanarDomains(nonfinite, {}, {}), Error::InvalidInput);
  Rejected(ArrangePlanarDomains(square, {}, {.WeldDistanceM = -1}), Error::InvalidInput);
  Rejected(ArrangePlanarDomains({}, cross, {}), Error::InvalidInput);
  Rejected(
      ArrangePlanarDomains(
          std::vector<PlanarDomain>(std::numeric_limits<uint64_t>::digits + 1, square[0]), {}, {}),
      Error::CapacityExceeded);
  std::stop_source canceled;
  canceled.request_stop();
  Rejected(ArrangePlanarDomains(square, cross, {}, {}, canceled.get_token()), Error::Canceled);
  for (size_t which = 0; which < 6; ++which) {
    PlanarArrangementBudget budget;
    switch (which) {
      case 0: budget.InputSegments = 3; break;
      case 1: budget.NodedSegments = 3; break;
      case 2: budget.Cells = 1; break;
      case 3: budget.DomainTests = 1; break;
      case 4: budget.Mesh.Faces = 1; break;
      default: budget.Mesh.Vertices = 3; break;
    }
    Rejected(ArrangePlanarDomains(square, cross, {}, budget), Error::CapacityExceeded);
  }
  return Report();
}
