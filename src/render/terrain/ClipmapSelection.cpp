#include "ClipmapRenderer.h"
#include "ClipmapVisibility.h"
#include <cstddef>
#include <cstdint>
#include <expected>

namespace outshine::Render {
size_t ClipmapRenderer::MeshIndex(const ClipmapFields &fields, size_t level) noexcept {
  return level == 0 ? 0
                    : 1u + static_cast<size_t>(fields.Rings[level - 1].Offsets[0] +
                                               2 * fields.Rings[level - 1].Offsets[1]);
}

Native::Result<void> ClipmapRenderer::Select(const ClipmapFields &fields,
                                             const Mat4f &clipFromRelative,
                                             ClipmapDrawList &into) const {
  into.Ranges.clear();
  into.Triangles = 0;
  const auto valid = Validate(fields, ClipmapPass::Depth);
  if (!valid) { return valid; }
  if (fields.HeightRanges.size() != fields.Rings.size()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "clipmap selection needs published height ranges"});
  }
  size_t capacity = 0;
  for (size_t level = 0; level < fields.Rings.size(); ++level) {
    capacity += Meshes_[MeshIndex(fields, level)].Blocks.size();
  }
  into.Ranges.reserve(capacity);
  const ClipmapFrustum frustum(clipFromRelative);
  for (size_t level = 0; level < fields.Rings.size(); ++level) {
    const auto &ring = fields.Rings[level];
    for (const auto &block : Meshes_[MeshIndex(fields, level)].Blocks) {
      if (!frustum.Intersects(block, ring, fields.HeightRanges[level], fields.HeightShift)) {
        continue;
      }
      into.Triangles += block.Count / 3;
      if (!into.Ranges.empty() && into.Ranges.back().Level == level &&
          into.Ranges.back().First + into.Ranges.back().Count == block.First) {
        into.Ranges.back().Count += block.Count;
      } else {
        into.Ranges.push_back(
            {.Level = static_cast<uint32_t>(level), .First = block.First, .Count = block.Count});
      }
    }
  }
  return {};
}
}
