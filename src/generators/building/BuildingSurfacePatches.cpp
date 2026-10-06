#include "BuildingSurface.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace outshine::Generators {

size_t BuildingSurface::EstimatedTriangles(std::span<const Face> faces) const noexcept {
  size_t count = 0;
  for (const auto &face : faces) {
    if (face.Side >= 2) {
      count += 2;
      continue;
    }
    const auto &shape = Shapes_[face.Part];
    size_t corners = shape.Ring.size();
    for (const auto &hole : shape.Holes) { corners += hole.size() + 2; }
    count += corners > 2 ? corners - 2 : 0;
  }
  return count;
}

void BuildingSurface::MeshPatches(std::span<const BuildingSurfacePatch> patches,
                                  Raised &into) const {
  for (const auto &patch : patches) {
    const bool roof = IsRoof(patch.Sample);
    auto &vertices = roof ? into.RoofCorners : into.WallCorners;
    auto &run = roof ? into.RoofRun : into.WallRun;
    const auto first = static_cast<uint32_t>(vertices.size());
    const bool paint = !roof && (WallColour_ || !into.WallColours.empty());
    if (paint && into.WallColours.empty()) { into.WallColours.resize(vertices.size() * 4, 1.0f); }
    const Vec3f colour = ColourFactor(patch.Sample);
    for (const auto &corner : patch.Corners) {
      vertices.push_back(VertexAt(patch.Sample, corner));
      if (paint) {
        into.WallColours.insert(into.WallColours.end(), {colour[0], colour[1], colour[2], 1.0f});
      }
    }
    const Vec3 a = patch.Corners[1] - patch.Corners[0];
    const Vec3 b = patch.Corners[2] - patch.Corners[0];
    const Vec3 cross{
        {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}};
    const std::array<uint32_t, 6> indices = Dot(cross, patch.Sample.Normal) > 0.0
                                                ? std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}
                                                : std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2};
    for (const auto index : indices) { run.push_back(first + index); }
  }
}

}
