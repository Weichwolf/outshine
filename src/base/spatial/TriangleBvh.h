#ifndef OUTSHINE_BASE_SPATIAL_TRIANGLEBVH_H
#define OUTSHINE_BASE_SPATIAL_TRIANGLEBVH_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"

namespace outshine {

struct Ray {
  Vec3f OriginM;
  Vec3f Toward;
};

constexpr size_t kBvhTriangleBytes = 36;

constexpr uint32_t kBvhLeafFirstBits = 24;
constexpr uint32_t kBvhLeafFirstMask = (1u << kBvhLeafFirstBits) - 1u;
constexpr uint32_t kBvhInterior = 0u;

constexpr uint32_t kBvhNoEscape = 0xFFFFFFFFu;

constexpr uint32_t kBvhLeafTriangles = 4;

struct BvhNode {
  Vec3f MinM;

  uint32_t Escape = kBvhNoEscape;
  Vec3f MaxM;

  uint32_t Leaf = kBvhInterior;

  [[nodiscard]] bool IsLeaf() const { return Leaf != kBvhInterior; }

  [[nodiscard]] uint32_t FirstTriangle() const { return Leaf & kBvhLeafFirstMask; }

  [[nodiscard]] uint32_t TriangleCount() const { return Leaf >> kBvhLeafFirstBits; }
};

struct BvhTriangle {
  Vec3f V0;
  Vec3f E1;
  Vec3f E2;
};

static_assert(sizeof(BvhNode) == 32, "the traversal reads a 32-byte node");
static_assert(sizeof(BvhTriangle) == kBvhTriangleBytes,
              "the intersection test reads a 36-byte triangle");

class TriangleBvh {
public:
  [[nodiscard]] static TriangleBvh Over(std::span<const float> positionsM,
                                        std::span<const uint32_t> indices);

  [[nodiscard]] bool Occludes(const Ray &along, float nearM, float distanceM) const;

  [[nodiscard]] std::span<const BvhNode> Nodes() const { return {Nodes_.data(), Nodes_.size()}; }

  [[nodiscard]] std::span<const BvhTriangle> Triangles() const {
    return {Tris_.data(), Tris_.size()};
  }

  [[nodiscard]] bool Refit(std::span<const float> positionsM);

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
