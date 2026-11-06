#include "GeometryAsset.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

std::optional<uint64_t> GeometryAsset::ContentMetrics::BufferBytes() const noexcept {
  constexpr auto most = std::numeric_limits<uint64_t>::max();
  if (Indices > most / sizeof(uint32_t) || Clusters > most / sizeof(DagCluster)) {
    return std::nullopt;
  }
  uint64_t total = 0;
  for (const auto bytes :
       {AttributeBytes, ImageBytes, Indices * sizeof(uint32_t), Clusters * sizeof(DagCluster)}) {
    if (bytes > most - total) { return std::nullopt; }
    total += bytes;
  }
  return total;
}

GeometryAsset::ContentMetrics
GeometryAsset::MeasureContent(const Geometry &geometry,
                              std::span<const std::vector<DagCluster>> clusters) {
  ContentMetrics result;
  for (int part = 0; part < geometry.parts(); ++part) {
    result.Vertices += geometry.positionsOf(part).size() / 3;
    result.Indices += geometry.trianglesOf(part).size();
    for (const auto values : {geometry.positionsOf(part),
                              geometry.normalsOf(part),
                              geometry.textureOf(part),
                              geometry.textureOf(part, UvSet::Uv1),
                              geometry.tangentsOf(part),
                              geometry.coloursOf(part)}) {
      result.AttributeBytes += values.size_bytes();
    }
  }
  for (int image = 0; image < geometry.images(); ++image) {
    const auto view = geometry.imageAt(image);
    result.ImageBytes += view.Rgba.size_bytes();
    for (const auto &mips : view.LowerMips) {
      if (mips) { result.ImageBytes += mips->size_bytes(); }
    }
  }
  for (const auto &part : clusters) { result.Clusters += part.size(); }
  return result;
}

}
