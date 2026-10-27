#include "EarthworkStampIndex.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr int64_t kBias = 0x20000000LL;
constexpr double kCellM = 32;
constexpr uint64_t kMostDenseCells = 1u << 22u;
}

EarthworkStampIndex::EarthworkStampIndex(std::span<const EarthworkStamp> stamps,
                                         std::span<const double> widthsM) {
  for (size_t index = 0; index < stamps.size(); ++index) {
    const auto &stamp = stamps[index];
    const double reachM = widthsM[index] + stamp.PlanarSupportM.value_or(0.0);
    Spread({.EastM = stamp.LowE - reachM, .NorthM = stamp.LowN - reachM},
           {.EastM = stamp.HighE + reachM, .NorthM = stamp.HighN + reachM},
           static_cast<uint32_t>(index));
  }
  Build();
}

int64_t EarthworkStampIndex::CellOf(double metres) {
  return static_cast<int64_t>(std::floor(metres / kCellM));
}

uint64_t EarthworkStampIndex::KeyOf(int64_t east, int64_t north) {
  return (static_cast<uint64_t>(east + kBias) << 32u) | static_cast<uint64_t>(north + kBias);
}

void EarthworkStampIndex::Spread(EastNorth low, EastNorth high, uint32_t stamp) {
  const int64_t firstE = CellOf(low.EastM);
  const int64_t lastE = CellOf(high.EastM);
  const int64_t firstN = CellOf(low.NorthM);
  const int64_t lastN = CellOf(high.NorthM);
  for (int64_t east = firstE; east <= lastE; ++east) {
    for (int64_t north = firstN; north <= lastN; ++north) {
      Keys_.push_back(KeyOf(east, north));
      Seats_.push_back(stamp);
    }
  }
}

void EarthworkStampIndex::BuildSparse() {
  std::vector<uint32_t> order(Keys_.size());
  std::ranges::iota(order, uint32_t{0});
  std::ranges::sort(order, [this](uint32_t a, uint32_t b) {
    return std::pair{Keys_[a], a} < std::pair{Keys_[b], b};
  });
  for (auto &entry : order) { entry = Seats_[entry]; }
  Seats_.swap(order);
  std::ranges::sort(Keys_);
}

void EarthworkStampIndex::Build() {
  if (Keys_.empty()) { return; }
  LowE_ = LowN_ = std::numeric_limits<int64_t>::max();
  int64_t highE = std::numeric_limits<int64_t>::min();
  int64_t highN = std::numeric_limits<int64_t>::min();
  for (const uint64_t key : Keys_) {
    const auto east = static_cast<int64_t>(key >> 32u) - kBias;
    const auto north = static_cast<int64_t>(key & 0xffffffffu) - kBias;
    LowE_ = std::min(LowE_, east);
    LowN_ = std::min(LowN_, north);
    highE = std::max(highE, east);
    highN = std::max(highN, north);
  }
  const auto wide = static_cast<uint64_t>(highE - LowE_ + 1);
  const auto tall = static_cast<uint64_t>(highN - LowN_ + 1);
  if (wide > kMostDenseCells / tall) {
    BuildSparse();
    return;
  }
  Wide_ = static_cast<int64_t>(wide);
  const auto cells = static_cast<size_t>(wide * tall);
  First_.assign(cells + 1u, 0u);
  for (const uint64_t key : Keys_) { ++First_[SeatOf(key) + 1u]; }
  for (size_t cell = 1; cell < First_.size(); ++cell) { First_[cell] += First_[cell - 1u]; }
  const auto stamps = std::move(Seats_);
  Seats_.resize(Keys_.size());
  std::vector<uint32_t> next(First_.begin(), First_.end() - 1);
  for (size_t entry = 0; entry < Keys_.size(); ++entry) {
    Seats_[next[SeatOf(Keys_[entry])]++] = stamps[entry];
  }
  std::vector<uint64_t>{}.swap(Keys_);
}

size_t EarthworkStampIndex::SeatOf(uint64_t key) const {
  const int64_t east = static_cast<int64_t>(key >> 32u) - kBias - LowE_;
  const int64_t north = static_cast<int64_t>(key & 0xffffffffu) - kBias - LowN_;
  return static_cast<size_t>(north) * static_cast<size_t>(Wide_) + static_cast<size_t>(east);
}

std::span<const uint32_t> EarthworkStampIndex::At(EastNorth point) const {
  if (Seats_.empty()) { return {}; }
  if (Wide_ == 0) {
    const auto found =
        std::ranges::equal_range(Keys_, KeyOf(CellOf(point.EastM), CellOf(point.NorthM)));
    const auto first = static_cast<size_t>(found.begin() - Keys_.begin());
    return std::span(Seats_).subspan(first, static_cast<size_t>(found.size()));
  }
  const int64_t east = CellOf(point.EastM) - LowE_;
  const int64_t north = CellOf(point.NorthM) - LowN_;
  if (east < 0 || north < 0 || east >= Wide_) { return {}; }
  const size_t cell =
      static_cast<size_t>(north) * static_cast<size_t>(Wide_) + static_cast<size_t>(east);
  if (cell + 1u >= First_.size()) { return {}; }
  return {Seats_.data() + First_[cell], First_[cell + 1u] - First_[cell]};
}

size_t EarthworkStampIndex::HeapBytes() const noexcept {
  return Keys_.capacity() * sizeof(uint64_t) +
         (First_.capacity() + Seats_.capacity()) * sizeof(uint32_t);
}

}
