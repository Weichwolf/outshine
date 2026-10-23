#include "PlanarRegionInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <limits>
#include <numeric>
#include <vector>

namespace outshine::Spatial::Regions {
namespace {
struct Ring {
  PolygonRing Points;
  std::vector<size_t> Previous, Next;
  std::vector<bool> Alive;
  std::deque<size_t> Queue;
  size_t Remaining = 0;
};

double ChainDistance(PlanarPoint point, PlanarPoint a, PlanarPoint b) {
  const PlanarPoint edge{b[0] - a[0], b[1] - a[1]};
  const double norm = edge[0] * edge[0] + edge[1] * edge[1];
  if (!std::isfinite(norm) || norm <= 0) { return std::numeric_limits<double>::infinity(); }
  const double ratio =
      std::clamp(((point[0] - a[0]) * edge[0] + (point[1] - a[1]) * edge[1]) / norm, 0.0, 1.0);
  const double x = point[0] - (a[0] + ratio * edge[0]);
  const double y = point[1] - (a[1] + ratio * edge[1]);
  return std::sqrt(x * x + y * y);
}

bool Removable(const Ring &ring, size_t at, const PlanarRegionBudget &budget) {
  const auto a = ring.Points[ring.Previous[at]];
  const auto b = ring.Points[ring.Next[at]];
  const auto p = ring.Points[at];
  const PlanarPoint edge{b[0] - a[0], b[1] - a[1]};
  const double norm = edge[0] * edge[0] + edge[1] * edge[1];
  if (!std::isfinite(norm) || norm <= 0) { return false; }
  const double ratio = ((p[0] - a[0]) * edge[0] + (p[1] - a[1]) * edge[1]) / norm;
  return ratio >= 0 && ratio <= 1 && ChainDistance(p, a, b) <= budget.CollinearityM;
}

Checked<Ring> ReadRing(const State &state, const GEOSGeometry *geometry) {
  const auto *coordinates = GEOSGeom_getCoordSeq_r(state.Context, geometry);
  unsigned int count = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(state.Context, coordinates, &count) == 0) {
    return std::unexpected(Error::LibraryFailure);
  }
  if (count < 4) { return std::unexpected(Error::InvalidInput); }
  const size_t size = count - 1;
  if (size > state.Budget.RingVertices) { return std::unexpected(Error::CapacityExceeded); }
  Ring ring;
  ring.Points.resize(size);
  ring.Previous.resize(size);
  ring.Next.resize(size);
  ring.Alive.assign(size, true);
  ring.Remaining = size;
  for (size_t at = 0; at < size; ++at) {
    auto &point = ring.Points[at];
    if (GEOSCoordSeq_getXY_r(
            state.Context, coordinates, static_cast<unsigned int>(at), point.data(), &point[1]) ==
        0) {
      return std::unexpected(Error::LibraryFailure);
    }
    ring.Previous[at] = (at + size - 1) % size;
    ring.Next[at] = (at + 1) % size;
    ring.Queue.push_back(at);
  }
  return ring;
}

Checked<void> Simplify(State &state, Ring &ring) {
  const size_t initialVisits = state.Output.Work.RingVisits;
  while (!ring.Queue.empty() && ring.Remaining > 3) {
    if (auto valid = state.Check(); !valid) { return valid; }
    if (state.Output.Work.RingVisits >= state.Budget.RingVisits ||
        state.Output.Work.RingVisits - initialVisits >= 8 * ring.Points.size()) {
      return std::unexpected(Error::CapacityExceeded);
    }
    ++state.Output.Work.RingVisits;
    const auto at = ring.Queue.front();
    ring.Queue.pop_front();
    if (!ring.Alive[at] || state.Protected.contains(ring.Points[at]) ||
        !Removable(ring, at, state.Budget)) {
      continue;
    }
    const auto a = ring.Previous[at];
    const auto b = ring.Next[at];
    ring.Alive[at] = false;
    --ring.Remaining;
    ring.Next[a] = b;
    ring.Previous[b] = a;
    ring.Queue.push_back(a);
    ring.Queue.push_back(b);
  }
  return {};
}

Checked<double> ChainError(const State &state, const Ring &ring) {
  double maximum = 0;
  for (size_t at = 0; at < ring.Points.size(); ++at) {
    if (!ring.Alive[at]) { continue; }
    if (auto valid = state.Check(); !valid) { return std::unexpected(valid.error()); }
    const auto end = ring.Next[at];
    for (size_t p = at; p != end; p = (p + 1) % ring.Points.size()) {
      maximum = std::max(maximum, ChainDistance(ring.Points[p], ring.Points[at], ring.Points[end]));
    }
  }
  if (maximum > state.Budget.CollinearityM) { return std::unexpected(Error::CoverageChanged); }
  return maximum;
}
}

Checked<PolygonRing> CleanRing(State &state, const GEOSGeometry *geometry) {
  auto ring = ReadRing(state, geometry);
  if (!ring) { return std::unexpected(ring.error()); }
  if (auto valid = Simplify(state, *ring); !valid) { return std::unexpected(valid.error()); }
  const auto error = ChainError(state, *ring);
  if (!error) { return std::unexpected(error.error()); }
  state.Output.Work.MaximumChainDeviationM =
      std::max(state.Output.Work.MaximumChainDeviationM, *error);
  state.Output.Work.RemovedVertices += ring->Points.size() - ring->Remaining;
  PolygonRing result;
  result.reserve(ring->Remaining);
  for (size_t at = 0; at < ring->Points.size(); ++at) {
    if (ring->Alive[at]) { result.push_back(ring->Points[at]); }
  }
  return result;
}
}
