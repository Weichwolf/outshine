#ifndef OUTSHINE_WORLD_GROUND_TILES_TERRAINGRID_H
#define OUTSHINE_WORLD_GROUND_TILES_TERRAINGRID_H

#include <cstddef>
#include <cstdint>
#include <mdspan>
#include <vector>

#include "TileGeodesy.h"
#include "TileMath.h"

namespace outshine::Ground {

struct GridFraction {
  double Col = 0.0;
  double Row = 0.0;
};

class TerrainField {
public:
  TerrainField() = default;

  TerrainField(uint32_t rows, uint32_t cols)
      : HeightsM_(static_cast<size_t>(rows) * static_cast<size_t>(cols), 0.0f),
        Rows_(rows),
        Cols_(cols) {}

  [[nodiscard]] uint32_t Rows() const { return Rows_; }

  [[nodiscard]] uint32_t Cols() const { return Cols_; }

  [[nodiscard]] bool Meshable() const { return Rows_ >= 2 && Cols_ >= 2; }

  [[nodiscard]] size_t Bytes() const { return HeightsM_.size() * sizeof(float); }

  [[nodiscard]] const float *Data() const { return HeightsM_.data(); }

  [[nodiscard]] float *Data() { return HeightsM_.data(); }

  using Writable = std::mdspan<float, std::dextents<size_t, 2>>;

  [[nodiscard]] Postings Field() const { return Postings(HeightsM_.data(), Rows_, Cols_); }

  [[nodiscard]] Writable Field() { return Writable(HeightsM_.data(), Rows_, Cols_); }

  [[nodiscard]] float AtM(uint32_t row, uint32_t col) const { return Field()[row, col]; }

  void SetM(uint32_t row, uint32_t col, float m) { Field()[row, col] = m; }

  [[nodiscard]] float PostingM(GridFraction at) const {
    const double gx = Cols_ < 2u ? 0.0 : at.Col * static_cast<double>(Cols_ - 1u);
    const double gy = Rows_ < 2u ? 0.0 : at.Row * static_cast<double>(Rows_ - 1u);
    return Bilinear(Field(), gx, gy);
  }

private:
  std::vector<float> HeightsM_;
  uint32_t Rows_ = 0, Cols_ = 0;
};

inline uint32_t PostingsPerEdge(uint32_t sourceEdge, uint32_t stride) {
  return (sourceEdge - 1u) / stride + 1u;
}

inline double PostingFrac(uint32_t k, uint32_t n) {
  return n < 2u ? 0.0 : static_cast<double>(k) * (1.0 / static_cast<double>(n - 1u));
}

class TerrainGrid {
public:
  enum class State { Decoded, NotHere, Undecodable, Deferred, Refused };

  static TerrainGrid FromTerrariumPng(const uint8_t *png, size_t len);

  static TerrainGrid NotHere() { return {State::NotHere, TerrainField()}; }

  static TerrainGrid Undecodable() { return {State::Undecodable, TerrainField()}; }

  static TerrainGrid Deferred() { return {State::Deferred, TerrainField()}; }

  static TerrainGrid Refused() { return {State::Refused, TerrainField()}; }

  static TerrainGrid Holding(TerrainField &&field) { return {State::Decoded, std::move(field)}; }

  [[nodiscard]] State Where() const { return Where_; }

  [[nodiscard]] const TerrainField *TryField() const {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }

  [[nodiscard]] TerrainField *TryFieldMutable() {
    return Where_ == State::Decoded ? &Field_ : nullptr;
  }

  [[nodiscard]] size_t Bytes() const { return Field_.Bytes(); }

private:
  TerrainGrid(State where, TerrainField &&field) : Where_(where), Field_(std::move(field)) {}

  State Where_;
  TerrainField Field_;
};

} // namespace outshine::Ground
#endif
