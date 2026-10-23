#include "SurfaceHeightTopologyInternal.h"
#include "GeosGeometry.h"
#include "GeosPolygon.h"
#include <geos_c.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::SurfaceHeights {
namespace {
struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();
  Context() = default;
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  ~Context() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

Box PointBox(PlanarPoint point) {
  return {.Min = {{point[0], point[1], 0}}, .Max = {{point[0], point[1], 0}}};
}

Checked<Box> PortBox(const SurfaceHeightPort &port,
                     double distance,
                     const JoinState &state,
                     size_t &points,
                     SurfaceHeightTopologyBudget budget) {
  if (port.Region.Rings.empty() || port.Domains.empty()) {
    return std::unexpected(Error::InvalidInput);
  }
  uint64_t bits = 0;
  for (const auto domain : port.Domains) {
    if (domain >= state.FaceCounts.size() || (bits & (uint64_t{1} << domain)) != 0) {
      return std::unexpected(Error::InvalidInput);
    }
    bits |= uint64_t{1} << domain;
  }
  Box box = PointBox(port.Region.Rings.front().empty() ? PlanarPoint{}
                                                       : port.Region.Rings.front().front());
  for (const auto &ring : port.Region.Rings) {
    if (ring.size() < 3) { return std::unexpected(Error::InvalidInput); }
    if (ring.size() > budget.PortPoints - points) {
      return std::unexpected(Error::CapacityExceeded);
    }
    points += ring.size();
    for (const auto &point : ring) {
      for (size_t axis = 0; axis < 2; ++axis) {
        if (!std::isfinite(point[axis])) { return std::unexpected(Error::InvalidInput); }
        box.Min[axis] = std::min(box.Min[axis], point[axis]);
        box.Max[axis] = std::max(box.Max[axis], point[axis]);
      }
    }
  }
  for (size_t axis = 0; axis < 2; ++axis) {
    box.Min[axis] -= distance;
    box.Max[axis] += distance;
    if (!std::isfinite(box.Min[axis]) || !std::isfinite(box.Max[axis])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return box;
}

void JoinVertex(uint32_t vertex, std::span<const uint32_t> domains, JoinState &state) {
  std::optional<uint32_t> first;
  for (const auto domain : domains) {
    if ((state.PointDomains[vertex] & (uint64_t{1} << domain)) == 0) { continue; }
    const uint32_t root = Root(state.Parents, KeyIndex(state.Keys, {domain, vertex}));
    if (!first) {
      first = root;
    } else if (*first != root) {
      state.Parents[root] = *first;
      ++state.Work.Joins;
    }
  }
}

Checked<void> JoinRegion(const Context &context,
                         const PlanarArrangement &arrangement,
                         const SurfaceHeightPort &port,
                         const PlanHierarchy &index,
                         Box box,
                         double distance,
                         JoinState &state,
                         SurfaceHeightTopologyBudget budget,
                         const std::stop_token &stop) {
  const auto polygon = Spatial::MakeGeosPolygon(context.Handle, port.Region.Rings);
  if (!polygon) { return std::unexpected(Error::LibraryFailure); }
  const char valid = GEOSisValid_r(context.Handle, polygon.get());
  if (valid == 2) { return std::unexpected(Error::LibraryFailure); }
  if (valid == 0) { return std::unexpected(Error::InvalidInput); }
  std::optional<Error> failed;
  const auto emit = [&](uint32_t vertex) {
    if (stop.stop_requested()) {
      failed = Error::Canceled;
      return false;
    }
    if (state.Work.PortCandidates == budget.PortCandidates) {
      failed = Error::CapacityExceeded;
      return false;
    }
    ++state.Work.PortCandidates;
    const auto xy = arrangement.Mesh.Points[vertex];
    const Spatial::GeosGeometry point{GEOSGeom_createPointFromXY_r(context.Handle, xy[0], xy[1]),
                                      {context.Handle}};
    if (!point) {
      failed = Error::LibraryFailure;
      return false;
    }
    double separation = 0;
    if (GEOSDistance_r(context.Handle, polygon.get(), point.get(), &separation) == 0 ||
        !std::isfinite(separation)) {
      failed = Error::LibraryFailure;
      return false;
    }
    if (separation <= distance) { JoinVertex(vertex, port.Domains, state); }
    return true;
  };
  if (!index.Query(box, emit)) { return std::unexpected(failed.value_or(Error::LibraryFailure)); }
  return {};
}
}

Checked<void> JoinPorts(const PlanarArrangement &arrangement,
                        std::span<const SurfaceHeightPort> ports,
                        double distanceM,
                        JoinState &state,
                        SurfaceHeightTopologyBudget budget,
                        const std::stop_token &stop) {
  if (ports.empty()) { return {}; }
  std::vector<Box> boxes;
  boxes.reserve(arrangement.Mesh.Points.size());
  for (const auto point : arrangement.Mesh.Points) { boxes.push_back(PointBox(point)); }
  const auto index = PlanHierarchy::Build(boxes);
  if (!index) { return std::unexpected(Error::InvalidInput); }
  state.Work.IndexHeapBytes = index->HeapBytes();
  const Context context;
  if (context.Handle == nullptr) { return std::unexpected(Error::LibraryFailure); }
  size_t points = 0;
  for (const auto &port : ports) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto box = PortBox(port, distanceM, state, points, budget);
    if (!box) { return std::unexpected(box.error()); }
    if (auto result =
            JoinRegion(context, arrangement, port, *index, *box, distanceM, state, budget, stop);
        !result) {
      return result;
    }
  }
  return {};
}
}
