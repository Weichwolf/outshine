#ifndef OUTSHINE_BASE_SPATIAL_TRIANGLEBVH_H
#define OUTSHINE_BASE_SPATIAL_TRIANGLEBVH_H

#include <cstdint>
#include <vector>

#include "Span.h"

namespace outshine {

constexpr uint32_t kBvhLeafFirstBits = 24;
constexpr uint32_t kBvhLeafFirstMask = (1u << kBvhLeafFirstBits) - 1u;
constexpr uint32_t kBvhInterior = 0u;

constexpr uint32_t kBvhNoEscape = 0xFFFFFFFFu;

constexpr uint32_t kBvhLeafTriangles = 4;

struct BvhNode {
  float MinM[3] = {0, 0, 0};

  uint32_t Escape = kBvhNoEscape;
  float MaxM[3] = {0, 0, 0};

  uint32_t Leaf = kBvhInterior;

  [[nodiscard]] bool IsLeaf() const { return Leaf != kBvhInterior; }

  [[nodiscard]] uint32_t FirstTriangle() const { return Leaf & kBvhLeafFirstMask; }

  [[nodiscard]] uint32_t TriangleCount() const { return Leaf >> kBvhLeafFirstBits; }
};

struct BvhTriangle {
  float V0[3] = {0, 0, 0};
  float E1[3] = {0, 0, 0};
  float E2[3] = {0, 0, 0};
};

static_assert(sizeof(BvhNode) == 32, "the traversal reads a 32-byte node");
static_assert(sizeof(BvhTriangle) == 36, "the intersection test reads a 36-byte triangle");

class TriangleBvh {
public:
  [[nodiscard]] static TriangleBvh Over(Span<const float> positionsM, Span<const uint32_t> indices);

  [[nodiscard]] bool
  Occludes(const float originM[3], const float direction[3], float nearM, float distanceM) const;

  [[nodiscard]] Span<const BvhNode> Nodes() const { return {Nodes_.data(), Nodes_.size()}; }

  [[nodiscard]] Span<const BvhTriangle> Triangles() const { return {Tris_.data(), Tris_.size()}; }

  [[nodiscard]] bool Refit(Span<const float> positionsM);

  [[nodiscard]] bool Empty() const { return Nodes_.empty(); }

  [[nodiscard]] uint32_t Depth() const { return Depth_; }

private:
  std::vector<BvhNode> Nodes_;
  std::vector<BvhTriangle> Tris_;

  std::vector<uint32_t> Corners_;
  uint32_t Depth_ = 0;
};

} // namespace outshine
#endif
