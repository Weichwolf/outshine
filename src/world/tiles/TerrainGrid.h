/* A TERRARIUM DEM TILE, DECODED: an RGB PNG whose 24-bit pixels are packed height
 * (h = R*256 + G + B/256 - 32768, metres on the source's own datum) becomes a float field, and the
 * field becomes the node grid of one terrain tile in ENU metres.
 *
 * THE FIELD IS REACHABLE ONLY THROUGH THE STATE. "Not here" and "will not decode" are different
 * answers about a place — one is retried, the other is a hole — and a zeroed grid that carries both
 * is how a wait becomes permanent. */
#ifndef TERRAINGRID_H
#define TERRAINGRID_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "TileGeodesy.h"
#include "TileMath.h"

namespace outshine::World {

/* Row-major, row 0 = north, col 0 = west. Heights in metres on the source grid's own datum. */
class TerrainField {
 public:
  TerrainField() = default;
  TerrainField(uint32_t rows, uint32_t cols)
      : HeightsM_((size_t)rows * (size_t)cols, 0.0f), Rows_(rows), Cols_(cols) {}

  uint32_t Rows() const { return Rows_; }
  uint32_t Cols() const { return Cols_; }
  bool Meshable() const { return Rows_ >= 2 && Cols_ >= 2; }
  size_t Bytes() const { return HeightsM_.size() * sizeof(float); }

  const float *Data() const { return HeightsM_.data(); }
  float *Data() { return HeightsM_.data(); }

  float AtM(uint32_t row, uint32_t col) const { return HeightsM_[(size_t)row * Cols_ + col]; }
  void SetM(uint32_t row, uint32_t col, float m) { HeightsM_[(size_t)row * Cols_ + col] = m; }

  /* A texel is an AREA and not a lattice point (TileMath.h), so a posting is INTERPOLATED and never
   * indexed. Fractions are of the tile: col east from the west edge, row south from the north. */
  float InterpolatedM(double fracCol, double fracRow) const {
    return Bilinear(HeightsM_.data(), Cols_, Rows_, TexelIndex(fracCol, Cols_),
                    TexelIndex(fracRow, Rows_));
  }

 private:
  std::vector<float> HeightsM_;
  uint32_t Rows_ = 0, Cols_ = 0;
};

/* Postings per tile edge for a source edge and stride; the stride must divide (edge - 1). */
inline uint32_t PostingsPerEdge(uint32_t sourceEdge, uint32_t stride) {
  return (sourceEdge - 1u) / stride + 1u;
}

/* THE POSTING LATTICE. Posting k of n sits on this tile fraction, and the mesh's vertex, its uv and
 * the height oracle all take it from here — a lattice restated anywhere else drifts in the last bit
 * and the oracle stops standing on the drawn triangle. */
inline double PostingFrac(uint32_t k, uint32_t n) { return (double)k * (1.0 / (double)(n - 1u)); }

class TerrainGrid {
 public:
  enum class State { Decoded, NotHere, Undecodable };

  /* PNG must be RGB or RGBA (alpha ignored). */
  static TerrainGrid FromTerrariumPng(const uint8_t *png, size_t len);
  static TerrainGrid NotHere() { return TerrainGrid(State::NotHere, TerrainField()); }
  static TerrainGrid Undecodable() { return TerrainGrid(State::Undecodable, TerrainField()); }
  static TerrainGrid Holding(TerrainField &&field) {
    return TerrainGrid(State::Decoded, std::move(field));
  }

  [[nodiscard]] State Where() const { return Where_; }

  /* Non-owning, and null unless there is a field: a position, never ownership. */
  [[nodiscard]] const TerrainField *TryField() const {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }
  /* The stitch averages this field's edges against its neighbours' in place. */
  [[nodiscard]] TerrainField *TryFieldMutable() {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }

  size_t Bytes() const { return Field_.Bytes(); }

 private:
  TerrainGrid(State where, TerrainField &&field) : Where_(where), Field_(std::move(field)) {}

  State Where_;
  TerrainField Field_;
};

/* THE NODE GRID of a terrain tile, row-major north->south, in ENU metres around the frame's origin.
 * Topology is not carried: the drawn triangles are cut by ChunkQuadWinding (world/ChunkSurface.h),
 * which is the only place the diagonal is stated. */
class TerrainMesh {
 public:
  /* NoTile is a statement about the PLACE and is retried or cached as a hole; every other negative
   * state is a defect this run should say out loud. Conflating the two is how a wait becomes
   * permanent and how a broken source becomes an empty world. */
  enum class State { Built, NoTile, SourceUndecodable, FieldTooSmall, StrideDoesNotDivide,
                     FrameUnusable };

  static TerrainMesh Over(const TerrainField &field, const TileEnuMap &map, uint32_t stride);
  static TerrainMesh Nothing(State why) { return TerrainMesh(why); }

  [[nodiscard]] State Where() const { return Where_; }

  /* 3 floats per vertex, (e, n, u); null unless the mesh was built. */
  [[nodiscard]] const std::vector<float> *TryPositionsEnuM() const {
    return Where_ == State::Built ? &PositionsEnuM_ : nullptr;
  }
  uint32_t VertexCount() const { return (uint32_t)(PositionsEnuM_.size() / 3); }

 private:
  explicit TerrainMesh(State where) : Where_(where) {}

  State Where_;
  std::vector<float> PositionsEnuM_;
};

}  // namespace outshine::World
#endif
