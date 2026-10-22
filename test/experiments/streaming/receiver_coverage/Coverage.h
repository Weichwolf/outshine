#ifndef OUTSHINE_EXPERIMENT_RECEIVER_COVERAGE_H
#define OUTSHINE_EXPERIMENT_RECEIVER_COVERAGE_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ReceiverCoverage {
struct Rectangle {
  uint32_t Left;
  uint32_t Right;
  uint32_t Top;
  uint32_t Bottom;
};

static_assert(sizeof(Rectangle) == 16);

class FloatMin {
public:
  explicit FloatMin(uint32_t side)
      : Side_(side), Cells_(size_t{side} * side, std::numeric_limits<float>::infinity()) {}

  void Mark(Rectangle r) {
    for (uint32_t y = r.Top; y <= r.Bottom; ++y) {
      for (uint32_t x = r.Left; x <= r.Right; ++x) {
        auto &value = Cells_[size_t{y} * Side_ + x];
        value = std::min(value, 0.f);
      }
    }
  }

  void Finish() {}

  [[nodiscard]] bool Occupied(size_t index) const { return Cells_[index] == 0; }

  [[nodiscard]] size_t Bytes() const { return Cells_.size() * sizeof(float); }

private:
  uint32_t Side_;
  std::vector<float> Cells_;
};

class ByteFill {
public:
  explicit ByteFill(uint32_t side) : Side_(side), Cells_(size_t{side} * side) {}

  void Mark(Rectangle r) {
    for (uint32_t y = r.Top; y <= r.Bottom; ++y) {
      auto *row = Cells_.data() + size_t{y} * Side_;
      std::fill(row + r.Left, row + r.Right + 1, uint8_t{1});
    }
  }

  void Finish() {}

  [[nodiscard]] bool Occupied(size_t index) const { return Cells_[index] != 0; }

  [[nodiscard]] size_t Bytes() const { return Cells_.size(); }

private:
  uint32_t Side_;
  std::vector<uint8_t> Cells_;
};

class RowMask {
public:
  explicit RowMask(uint32_t side)
      : Side_(side), Stride_((side + WordBits - 1) / WordBits), Words_(size_t{Stride_} * side) {}

  void Mark(Rectangle r) {
    constexpr uint64_t All = std::numeric_limits<uint64_t>::max();
    const uint32_t first = r.Left / WordBits;
    const uint32_t last = r.Right / WordBits;
    const uint64_t left = All << (r.Left % WordBits);
    const uint64_t right = All >> (WordBits - 1 - r.Right % WordBits);
    for (uint32_t y = r.Top; y <= r.Bottom; ++y) {
      for (uint32_t word = first; word <= last; ++word) {
        const uint64_t mask = (word == first ? left : All) & (word == last ? right : All);
        Words_[size_t{y} * Stride_ + word] |= mask;
      }
    }
  }

  void Finish() {}

  [[nodiscard]] bool Occupied(size_t index) const {
    const size_t row = index / Side_;
    const size_t x = index % Side_;
    return ((Words_[row * Stride_ + x / WordBits] >> (x % WordBits)) & 1) != 0;
  }

  [[nodiscard]] size_t Bytes() const { return Words_.size() * sizeof(uint64_t); }

private:
  static constexpr uint32_t WordBits = 64;
  uint32_t Side_;
  uint32_t Stride_;
  std::vector<uint64_t> Words_;
};

class Difference {
public:
  explicit Difference(uint32_t side)
      : Side_(side), Stride_(side + 1), Cells_(size_t{Stride_} * Stride_) {}

  void Mark(Rectangle r) {
    ++Cells_[size_t{r.Top} * Stride_ + r.Left];
    --Cells_[size_t{r.Top} * Stride_ + r.Right + 1];
    --Cells_[size_t{r.Bottom + 1} * Stride_ + r.Left];
    ++Cells_[size_t{r.Bottom + 1} * Stride_ + r.Right + 1];
  }

  void Finish() {
    for (uint32_t y = 0; y < Side_; ++y) {
      int64_t running = 0;
      for (uint32_t x = 0; x < Side_; ++x) {
        const size_t index = size_t{y} * Stride_ + x;
        running += Cells_[index];
        Cells_[index] = running + (y != 0 ? Cells_[index - Stride_] : 0);
      }
    }
  }

  [[nodiscard]] bool Occupied(size_t index) const {
    return Cells_[index / Side_ * Stride_ + index % Side_] > 0;
  }

  [[nodiscard]] size_t Bytes() const { return Cells_.size() * sizeof(int64_t); }

private:
  uint32_t Side_;
  uint32_t Stride_;
  std::vector<int64_t> Cells_;
};

}
#endif
