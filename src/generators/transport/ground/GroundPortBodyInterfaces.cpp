#include "GroundPortBodyInterfaces.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using OwnedEdge = std::pair<uint64_t, World::TransportEdgeId>;

uint64_t Edge(uint32_t a, uint32_t b) {
  return (uint64_t{std::min(a, b)} << 32) | std::max(a, b);
}

uint64_t FieldEdge(const GroundPortField &field, const GroundPortBody &body, size_t wall) {
  const auto &geometry = body.Geometry;
  const auto parent = geometry.WallParents[wall];
  const auto top = geometry.Faces[parent];
  const auto original = field.Geometry().Surface.Faces[body.FaceParents[parent]];
  const auto edge = HeightSolidWallSurfaceEdge(geometry, wall);
  if (!edge) { return std::numeric_limits<uint64_t>::max(); }
  std::array<uint32_t, 2> points{};
  for (size_t at = 0; at < 2; ++at) {
    for (size_t corner = 0; corner < 3; ++corner) {
      if (top[corner] == (*edge)[at]) {
        points[at] = original[corner];
        break;
      }
    }
  }
  return Edge(points[0], points[1]);
}
}

size_t GroundFieldInterfaceBytesMost(size_t faces) noexcept {
  return faces * 6 * (sizeof(OwnedEdge) + sizeof(uint64_t));
}

std::expected<void, GroundPortBodyError> MarkGroundFieldInterfaces(const GroundPortField &field,
                                                                   GroundPortBodies &bodies,
                                                                   const std::stop_token &stop) {
  if (bodies.Bodies.size() < 2) { return {}; }
  std::vector<OwnedEdge> owners;
  std::vector<uint64_t> shared;
  const auto &faces = field.Geometry().Surface.Faces;
  owners.reserve(3 * faces.size());
  shared.reserve(3 * faces.size());
  for (size_t at = 0; at < faces.size(); ++at) {
    if (stop.stop_requested()) {
      return std::unexpected(GroundPortBodyError{RoadGroundError::Canceled});
    }
    const auto source = field.SourceEdgeOfFace(at);
    if (!source) { return std::unexpected(GroundPortBodyError{source.error()}); }
    const auto face = faces[at];
    for (size_t edge = 0; edge < 3; ++edge) {
      const auto key = Edge(face[edge], face[(edge + 1) % 3]);
      owners.emplace_back(key, *source);
      ++bodies.Work.SolidEdgeVisits;
    }
  }
  std::ranges::sort(owners, {}, &OwnedEdge::first);
  for (size_t at = 1; at < owners.size(); ++at) {
    if (owners[at - 1].first == owners[at].first && owners[at - 1].second != owners[at].second) {
      shared.push_back(owners[at].first);
    }
  }
  bodies.Work.ScratchCapacityBytes =
      std::max(bodies.Work.ScratchCapacityBytes,
               owners.capacity() * sizeof(OwnedEdge) + shared.capacity() * sizeof(uint64_t));
  for (auto &body : bodies.Bodies) {
    auto &geometry = body.Geometry;
    for (size_t wall = 0; wall < geometry.WallKinds.size(); ++wall) {
      if (stop.stop_requested()) {
        return std::unexpected(GroundPortBodyError{RoadGroundError::Canceled});
      }
      if (std::ranges::binary_search(shared, FieldEdge(field, body, wall))) {
        geometry.WallKinds[wall] = HeightSolidWallKind::Interface;
      }
    }
  }
  return {};
}
}
