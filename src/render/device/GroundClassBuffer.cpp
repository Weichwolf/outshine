#include "GroundClassBuffer.h"
#include "ClassStructure.h"
#include "Digest.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <ratio>
#include <span>

namespace outshine::Render {
namespace {
struct ClassHeader {
  uint32_t FineAt, CoarseAt;
  int32_t UnmappedRow;
  uint32_t Reserved;
};

struct GridHeader {
  uint32_t Width, Height;
  float OriginEastM, OriginNorthM, CellM;
  uint32_t CellsAt, SeedsAt, RefsAt, EdgesAt;
  std::array<uint32_t, 3> Reserved;
};

constexpr size_t kHeaderWords = 4;
constexpr size_t kGridWords = 12;
constexpr size_t kWordBytes = sizeof(uint32_t);
constexpr size_t kCellsWord = 5, kSeedsWord = 6, kRefsWord = 7, kEdgesWord = 8, kReservedWord = 9;

static_assert(kWordBytes == 4);
static_assert(sizeof(ClassHeader) == kHeaderWords * kWordBytes &&
              alignof(ClassHeader) == kWordBytes && offsetof(ClassHeader, FineAt) == 0 &&
              offsetof(ClassHeader, CoarseAt) == kWordBytes &&
              offsetof(ClassHeader, UnmappedRow) == 2 * kWordBytes &&
              offsetof(ClassHeader, Reserved) == 3 * kWordBytes);
static_assert(sizeof(GridHeader) == kGridWords * kWordBytes && alignof(GridHeader) == kWordBytes &&
              offsetof(GridHeader, Width) == 0 && offsetof(GridHeader, Height) == kWordBytes &&
              offsetof(GridHeader, OriginEastM) == 2 * kWordBytes &&
              offsetof(GridHeader, OriginNorthM) == 3 * kWordBytes &&
              offsetof(GridHeader, CellM) == 4 * kWordBytes &&
              offsetof(GridHeader, CellsAt) == kCellsWord * kWordBytes &&
              offsetof(GridHeader, SeedsAt) == kSeedsWord * kWordBytes &&
              offsetof(GridHeader, RefsAt) == kRefsWord * kWordBytes &&
              offsetof(GridHeader, EdgesAt) == kEdgesWord * kWordBytes &&
              offsetof(GridHeader, Reserved) == kReservedWord * kWordBytes);
static_assert(sizeof(float) == kWordBytes && std::numeric_limits<float>::is_iec559);

}

GroundClassBuffer::GroundClassBuffer(const ClassStructure &classes) {
  const auto began = std::chrono::steady_clock::now();
  const std::array<const ClassStructure::Grid *, 2> grids{&classes.Fine(), &classes.Coarse()};
  size_t words = kHeaderWords + grids.size() * kGridWords;
  for (const auto *grid : grids) {
    if (grid->W > 0) {
      words += grid->Cells.size() + grid->Seeds.size() + grid->Refs.size() + grid->Edges.size();
    }
  }
  assert(words <= std::numeric_limits<uint32_t>::max());
  Words_.reserve(words);
  Words_.resize(kHeaderWords + grids.size() * kGridWords);
  const ClassHeader header{
      .FineAt = grids[0]->W > 0 ? static_cast<uint32_t>(kHeaderWords) : 0u,
      .CoarseAt = grids[1]->W > 0 ? static_cast<uint32_t>(kHeaderWords + kGridWords) : 0u,
      .UnmappedRow = classes.UnmappedRow(),
      .Reserved = 0};
  const auto headerWords = std::bit_cast<std::array<uint32_t, kHeaderWords>>(header);
  std::ranges::copy(headerWords, Words_.data());
  const auto append = [this](std::span<const uint32_t> values) {
    const auto at = static_cast<uint32_t>(Words_.size());
    Words_.insert(Words_.end(), values.begin(), values.end());
    return at;
  };
  for (size_t tier = 0; tier < grids.size(); ++tier) {
    const auto &grid = *grids[tier];
    if (grid.W == 0) { continue; }
    const GridHeader packed{.Width = static_cast<uint32_t>(grid.W),
                            .Height = static_cast<uint32_t>(grid.H),
                            .OriginEastM = static_cast<float>(grid.OrgE),
                            .OriginNorthM = static_cast<float>(grid.OrgN),
                            .CellM = static_cast<float>(grid.CellM),
                            .CellsAt = append(grid.Cells),
                            .SeedsAt = append(grid.Seeds),
                            .RefsAt = append(grid.Refs),
                            .EdgesAt = static_cast<uint32_t>(Words_.size()),
                            .Reserved = {}};
    const size_t edgeAt = Words_.size();
    Words_.resize(edgeAt + grid.Edges.size());
    if (!grid.Edges.empty()) {
      std::memcpy(Words_.data() + edgeAt, grid.Edges.data(), grid.Edges.size() * sizeof(float));
    }
    const auto packedWords = std::bit_cast<std::array<uint32_t, kGridWords>>(packed);
    std::ranges::copy(packedWords, Words_.data() + kHeaderWords + tier * kGridWords);
  }
  Digest_ = kDigestBasis;
  for (const uint32_t word : Words_) { Digest_ = (Digest_ ^ word) * kDigestPrime; }
  PackMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
}
}
