#include "EarthworkStampIndex.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr int64_t kBias = 0x20000000LL;
constexpr double kCellM = 32;
constexpr uint32_t kQuadrantOffset = 28;
constexpr uint32_t kStampMask = (1u << kQuadrantOffset) - 1u;
constexpr uint64_t kMostDenseCells = 1u << 22u;
constexpr uint64_t kSupportEntriesMost = 64;
constexpr uint32_t kMostCellShift = std::numeric_limits<int64_t>::digits;

int64_t CellOf(double metres, uint32_t baseShift) {
  return static_cast<int64_t>(std::floor(metres / (baseShift == 0 ? kCellM : kCellM / 2)));
}

int64_t CoarseCell(int64_t fine, uint32_t shift) {
  if (fine >= 0) { return static_cast<int64_t>(static_cast<uint64_t>(fine) >> shift); }
  return -1 - static_cast<int64_t>(static_cast<uint64_t>(-(fine + 1)) >> shift);
}

uint64_t KeyOf(int64_t east, int64_t north) {
  return (static_cast<uint64_t>(east + kBias) << 32u) | static_cast<uint64_t>(north + kBias);
}
}

uint32_t EarthworkStampIndex::ShiftFor(const CellBounds &bounds, uint32_t firstShift) {
  for (uint32_t shift = firstShift; shift < kMostCellShift; ++shift) {
    const auto wide = static_cast<uint64_t>(CoarseCell(bounds.HighE, shift)) -
                      static_cast<uint64_t>(CoarseCell(bounds.LowE, shift));
    const auto tall = static_cast<uint64_t>(CoarseCell(bounds.HighN, shift)) -
                      static_cast<uint64_t>(CoarseCell(bounds.LowN, shift));
    if (wide < kSupportEntriesMost && tall < kSupportEntriesMost &&
        wide + 1 <= kSupportEntriesMost / (tall + 1)) {
      return shift;
    }
  }
  return kMostCellShift;
}

EarthworkStampIndex::EarthworkStampIndex(std::span<const EarthworkStamp> stamps,
                                         std::span<const double> widthsM,
                                         Support support)
    : Base_(support == Support::Aprons && stamps.size() <= size_t{kStampMask} + 1 ? 1u : 0u) {
  for (size_t index = 0; index < stamps.size(); ++index) {
    const auto &stamp = stamps[index];
    const double apronM = widthsM[index] + stamp.PlanarSupportM.value_or(0.0);
    const double reachM = support == Support::Rejection && RejectsWithinPolygonOnly(stamp)
                              ? std::min(apronM, kEarthworkContactToleranceM)
                              : apronM;
    const auto cell = [&](double metres, int64_t edge) {
      if (Base_.Shift != 0 && stamp.Profile) { return 2 * CellOf(metres, 0) + edge; }
      return CellOf(metres, Base_.Shift);
    };
    const CellBounds bounds{.LowE = cell(stamp.LowE - reachM, 0),
                            .HighE = cell(stamp.HighE + reachM, 1),
                            .LowN = cell(stamp.LowN - reachM, 0),
                            .HighN = cell(stamp.HighN + reachM, 1),
                            .Stamp = static_cast<uint32_t>(index)};
    const uint32_t shift = ShiftFor(bounds, Base_.Shift);
    if (shift == Base_.Shift) {
      Base_.Add(bounds, bounds.Stamp, Base_.Shift != 0);
      continue;
    }
    const auto entry = static_cast<uint32_t>(CoarseSupports_.size());
    CoarseSupports_.push_back(bounds);
    const auto found = std::ranges::find(CoarseLevels_, shift, &CellLevel::Shift);
    if (found == CoarseLevels_.end()) {
      CoarseLevels_.emplace_back(shift).Add(bounds, entry);
    } else {
      found->Add(bounds, entry);
    }
  }
  Base_.Build();
  for (auto &level : CoarseLevels_) { level.Build(); }
}

uint32_t EarthworkStampIndex::QuadrantMask(const CellBounds &bounds, std::array<int64_t, 2> cell) {
  const auto [east, north] = cell;
  const uint32_t columns = static_cast<uint32_t>(bounds.LowE <= 2 * east) |
                           (static_cast<uint32_t>(bounds.HighE > 2 * east) << 1u);
  const uint32_t lower = bounds.LowN <= 2 * north ? columns : 0u;
  const uint32_t upper = bounds.HighN > 2 * north ? columns : 0u;
  return (lower | (upper << 2u)) << kQuadrantOffset;
}

void EarthworkStampIndex::CellLevel::Add(const CellBounds &bounds,
                                         uint32_t stamp,
                                         bool packQuadrants) {
  const int64_t firstE = CoarseCell(bounds.LowE, Shift);
  const int64_t lastE = CoarseCell(bounds.HighE, Shift);
  const int64_t firstN = CoarseCell(bounds.LowN, Shift);
  const int64_t lastN = CoarseCell(bounds.HighN, Shift);
  for (int64_t east = firstE; east <= lastE; ++east) {
    for (int64_t north = firstN; north <= lastN; ++north) {
      CellKeys.push_back(KeyOf(east, north));
      StampIndices.push_back(packQuadrants ? stamp | QuadrantMask(bounds, {east, north}) : stamp);
    }
  }
}

void EarthworkStampIndex::CellLevel::BuildSparse() {
  std::vector<uint32_t> order(CellKeys.size());
  std::ranges::iota(order, uint32_t{0});
  std::ranges::sort(order, [this](uint32_t a, uint32_t b) {
    return std::pair{CellKeys[a], a} < std::pair{CellKeys[b], b};
  });
  for (auto &entry : order) { entry = StampIndices[entry]; }
  StampIndices.swap(order);
  std::ranges::sort(CellKeys);
}

void EarthworkStampIndex::CellLevel::Build() {
  if (CellKeys.empty()) { return; }
  LowE = LowN = std::numeric_limits<int64_t>::max();
  int64_t highE = std::numeric_limits<int64_t>::min();
  int64_t highN = std::numeric_limits<int64_t>::min();
  for (const uint64_t key : CellKeys) {
    const auto east = static_cast<int64_t>(key >> 32u) - kBias;
    const auto north = static_cast<int64_t>(key & 0xffffffffu) - kBias;
    LowE = std::min(LowE, east);
    LowN = std::min(LowN, north);
    highE = std::max(highE, east);
    highN = std::max(highN, north);
  }
  const auto wide = static_cast<uint64_t>(highE - LowE + 1);
  const auto tall = static_cast<uint64_t>(highN - LowN + 1);
  if (wide > std::min(kMostDenseCells, uint64_t{2} * CellKeys.size()) / tall) {
    BuildSparse();
    return;
  }
  ColumnCount = static_cast<int64_t>(wide);
  const auto cells = static_cast<size_t>(wide * tall);
  CellOffsets.assign(cells + 1u, 0u);
  for (const uint64_t key : CellKeys) { ++CellOffsets[CellOffset(key) + 1u]; }
  for (size_t cell = 1; cell < CellOffsets.size(); ++cell) {
    CellOffsets[cell] += CellOffsets[cell - 1u];
  }
  const auto stamps = std::move(StampIndices);
  StampIndices.resize(CellKeys.size());
  std::vector<uint32_t> next(CellOffsets.begin(), CellOffsets.end() - 1);
  for (size_t entry = 0; entry < CellKeys.size(); ++entry) {
    StampIndices[next[CellOffset(CellKeys[entry])]++] = stamps[entry];
  }
  std::vector<uint64_t>{}.swap(CellKeys);
}

size_t EarthworkStampIndex::CellLevel::CellOffset(uint64_t key) const {
  const int64_t east = static_cast<int64_t>(key >> 32u) - kBias - LowE;
  const int64_t north = static_cast<int64_t>(key & 0xffffffffu) - kBias - LowN;
  return static_cast<size_t>(north) * static_cast<size_t>(ColumnCount) + static_cast<size_t>(east);
}

std::span<const uint32_t> EarthworkStampIndex::CellLevel::At(std::array<int64_t, 2> cell) const {
  if (StampIndices.empty()) { return {}; }
  const int64_t east = CoarseCell(cell[0], Shift);
  const int64_t north = CoarseCell(cell[1], Shift);
  if (ColumnCount == 0) {
    const auto found = std::ranges::equal_range(CellKeys, KeyOf(east, north));
    const auto first = static_cast<size_t>(found.begin() - CellKeys.begin());
    return std::span(StampIndices).subspan(first, static_cast<size_t>(found.size()));
  }
  if (east < LowE || north < LowN || east - LowE >= ColumnCount) { return {}; }
  const size_t offset = static_cast<size_t>(north - LowN) * static_cast<size_t>(ColumnCount) +
                        static_cast<size_t>(east - LowE);
  if (offset + 1u >= CellOffsets.size()) { return {}; }
  return {StampIndices.data() + CellOffsets[offset],
          CellOffsets[offset + 1u] - CellOffsets[offset]};
}

std::span<const uint32_t> EarthworkStampIndex::At(EastNorth point,
                                                  EarthworkStampQuery &scratch) const {
  const std::array cell{CellOf(point.EastM, Base_.Shift), CellOf(point.NorthM, Base_.Shift)};
  auto base = Base_.At(cell);
  if (Base_.Shift != 0) {
    const uint32_t quadrant =
        (static_cast<uint32_t>(cell[0]) & 1u) | ((static_cast<uint32_t>(cell[1]) & 1u) << 1u);
    const uint32_t bit = 1u << (kQuadrantOffset + quadrant);
    scratch.BaseCandidates.clear();
    for (const uint32_t candidate : base) {
      if ((candidate & bit) != 0) { scratch.BaseCandidates.push_back(candidate & kStampMask); }
    }
    base = scratch.BaseCandidates;
  }
  if (CoarseLevels_.empty()) { return base; }
  scratch.CoarseCandidates.clear();
  for (const auto &level : CoarseLevels_) {
    for (const uint32_t index : level.At(cell)) {
      const auto &support = CoarseSupports_[index];
      if (support.LowE <= cell[0] && cell[0] <= support.HighE && support.LowN <= cell[1] &&
          cell[1] <= support.HighN) {
        scratch.CoarseCandidates.push_back(support.Stamp);
      }
    }
  }
  if (scratch.CoarseCandidates.empty()) { return base; }
  std::ranges::sort(scratch.CoarseCandidates);
  scratch.Candidates.clear();
  std::ranges::merge(base, scratch.CoarseCandidates, std::back_inserter(scratch.Candidates));
  return scratch.Candidates;
}

size_t EarthworkStampIndex::CellLevel::HeapBytes() const noexcept {
  return CellKeys.capacity() * sizeof(uint64_t) +
         (CellOffsets.capacity() + StampIndices.capacity()) * sizeof(uint32_t);
}

size_t EarthworkStampIndex::HeapBytes() const noexcept {
  size_t bytes = Base_.HeapBytes() + CoarseLevels_.capacity() * sizeof(CellLevel) +
                 CoarseSupports_.capacity() * sizeof(CellBounds);
  for (const auto &level : CoarseLevels_) { bytes += level.HeapBytes(); }
  return bytes;
}

}
