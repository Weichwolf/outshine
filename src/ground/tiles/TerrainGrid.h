#ifndef OUTSHINE_GROUND_TILES_TERRAINGRID_H
#define OUTSHINE_GROUND_TILES_TERRAINGRID_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "TileGeodesy.h"
#include "TileMath.h"

namespace outshine::Ground {

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

  [[nodiscard]] float PostingM(double fracCol, double fracRow) const {
    const double gx = Cols_ < 2u ? 0.0 : fracCol * (double)(Cols_ - 1u);
    const double gy = Rows_ < 2u ? 0.0 : fracRow * (double)(Rows_ - 1u);
    return Bilinear(HeightsM_.data(), Cols_, Rows_, gx, gy);
  }

 private:
  std::vector<float> HeightsM_;
  uint32_t Rows_ = 0, Cols_ = 0;
};

inline uint32_t PostingsPerEdge(uint32_t sourceEdge, uint32_t stride) {
  return (sourceEdge - 1u) / stride + 1u;
}

inline double PostingFrac(uint32_t k, uint32_t n) {
  return n < 2u ? 0.0 : (double)k * (1.0 / (double)(n - 1u));
}

class TerrainGrid {
 public:

  enum class State { Decoded, NotHere, Undecodable, Deferred, Refused };

  static TerrainGrid FromTerrariumPng(const uint8_t *png, size_t len);
  static TerrainGrid NotHere() { return TerrainGrid(State::NotHere, TerrainField()); }
  static TerrainGrid Undecodable() { return TerrainGrid(State::Undecodable, TerrainField()); }
  static TerrainGrid Deferred() { return TerrainGrid(State::Deferred, TerrainField()); }
  static TerrainGrid Refused() { return TerrainGrid(State::Refused, TerrainField()); }
  static TerrainGrid Holding(TerrainField &&field) {
    return TerrainGrid(State::Decoded, std::move(field));
  }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] const TerrainField *TryField() const {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }

  [[nodiscard]] TerrainField *TryFieldMutable() {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }

  size_t Bytes() const { return Field_.Bytes(); }

 private:
  TerrainGrid(State where, TerrainField &&field) : Where_(where), Field_(std::move(field)) {}

  State Where_;
  TerrainField Field_;
};

class TerrainMesh {
 public:

  enum class State { Built, NoTile, SourceUndecodable, FieldTooSmall, StrideDoesNotDivide,
                     FrameUnusable, Deferred, SourceRefused };

  static TerrainMesh Over(const TerrainField &field, const TileEnuMap &map, uint32_t stride);
  static TerrainMesh Nothing(State why) { return TerrainMesh(why); }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] const std::vector<float> *TryPositionsEnuM() const {
    return Where_ == State::Built ? &PositionsEnuM_ : nullptr;
  }
  uint32_t VertexCount() const { return (uint32_t)(PositionsEnuM_.size() / 3); }

 private:
  explicit TerrainMesh(State where) : Where_(where) {}

  State Where_;
  std::vector<float> PositionsEnuM_;
};

}
#endif
