#include "AudioOcclusion.h"
#include "scene/Geometry.h"
#include "math/Mat4.h"
#include "math/Vec3.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace outshine::Core {
namespace {
namespace Says {
constexpr auto kInvalidTopology =
    "audio occlusion requires complete triangles and valid vertex indices";
constexpr auto kTooLarge = "audio occlusion exceeds the BVH index capacity";
constexpr auto kInvalidPosition =
    "audio occlusion positions must remain finite in local float coordinates";
}

[[nodiscard]] std::expected<void, std::string> Append(std::span<const float> positions,
                                                      std::span<const uint32_t> indices,
                                                      const Mat4 &placement,
                                                      std::vector<float> &corners,
                                                      std::vector<uint32_t> &faces) {
  if (positions.size() % 3u != 0 || indices.size() % 3u != 0) {
    return std::unexpected(Says::kInvalidTopology);
  }
  const size_t firstVertex = corners.size() / 3u;
  const size_t vertices = positions.size() / 3u;
  constexpr size_t maxVertices = std::numeric_limits<uint32_t>::max();
  constexpr size_t maxIndices = static_cast<size_t>(kBvhLeafFirstMask) * 3u;
  if (vertices > maxVertices - firstVertex || indices.size() > maxIndices - faces.size()) {
    return std::unexpected(Says::kTooLarge);
  }
  for (const uint32_t index : indices) {
    if (index >= vertices) { return std::unexpected(Says::kInvalidTopology); }
  }
  for (size_t at = 0; at < positions.size(); at += 3u) {
    const Vec3 position =
        placement.TransformPoint({{positions[at], positions[at + 1u], positions[at + 2u]}});
    for (size_t axis = 0; axis < 3u; ++axis) {
      if (!std::isfinite(position[axis]) ||
          std::abs(position[axis]) > std::numeric_limits<float>::max()) {
        return std::unexpected(Says::kInvalidPosition);
      }
      corners.push_back(static_cast<float>(position[axis]));
    }
  }
  for (const uint32_t index : indices) {
    faces.push_back(static_cast<uint32_t>(firstVertex) + index);
  }
  return {};
}
}

std::expected<TriangleBvh, std::string>
BuildAudioOcclusion(const Geometry &geometry,
                    std::span<const float> groundPositionsM,
                    std::span<const uint32_t> groundIndices) {
  std::vector<float> corners;
  std::vector<uint32_t> faces;
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto appended = Append(geometry.positionsOf(part),
                                 geometry.trianglesOf(part),
                                 geometry.placementOf(part),
                                 corners,
                                 faces);
    if (!appended) { return std::unexpected(appended.error()); }
  }
  const auto appended = Append(groundPositionsM, groundIndices, Mat4{}, corners, faces);
  if (!appended) { return std::unexpected(appended.error()); }
  auto hierarchy = TriangleBvh::Over(corners, faces);
  if (!faces.empty() && hierarchy.Empty()) { return std::unexpected(Says::kInvalidPosition); }
  return hierarchy;
}
}
