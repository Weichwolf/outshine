#ifndef OUTSHINE_RENDER_SCENE_PIECEMESH_H
#define OUTSHINE_RENDER_SCENE_PIECEMESH_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include "Mat4.h"
#include "MaterialRegistration.h"
#include "StoredVertex.h"
#include "ClusterDag.h"

namespace outshine::Render {

struct PieceSurface {
  enum class Source { Geometry, Registered };
  uint32_t Index = 0;
  Source From = Source::Geometry;
  std::shared_ptr<const MaterialSource> MaterialOwner;

  [[nodiscard]] bool operator==(const PieceSurface &) const noexcept = default;

  PieceSurface() = default;

  explicit PieceSurface(uint32_t index) noexcept : Index(index) {}

  [[nodiscard]] static PieceSurface Registered(uint32_t index) noexcept {
    PieceSurface result(index);
    result.From = Source::Registered;
    return result;
  }

  [[nodiscard]] static PieceSurface Registered(const MaterialRegistration &registration,
                                               uint32_t offset = 0) noexcept {
    PieceSurface result = Registered(registration.At(offset));
    result.MaterialOwner = registration.Source;
    return result;
  }
};

struct PieceMesh {
  std::span<const float> Tangents;
  std::span<const StoredVertex> Verts;
  std::span<const uint32_t> Indices;
  std::span<const DagCluster> Clusters;
  std::span<const float> Colours;
  std::span<const float> Uv1;
  Mat4 Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  std::span<const Mat4> Instances;
  uint32_t MaxInstances = 0;
  PieceSurface Surface;
  bool Textured = false;
};

inline constexpr auto kPieceUv1Error =
    "piece second UVs require UV0 and exactly two finite floats per vertex";

[[nodiscard]] inline bool ValidPieceUv1(const PieceMesh &piece) noexcept {
  return piece.Uv1.empty() ||
         (piece.Textured && piece.Verts.size() <= std::numeric_limits<size_t>::max() / 2 &&
          piece.Uv1.size() == piece.Verts.size() * 2 &&
          std::ranges::all_of(piece.Uv1, [](float value) { return std::isfinite(value); }));
}

using PieceId = uint32_t;
inline constexpr PieceId kNoPiece = ~0u;

}

#endif
