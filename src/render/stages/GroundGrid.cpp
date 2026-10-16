#include "UploadQueue.h"
#include "GroundLattice.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr auto kSide = GroundLattice::kSide;
constexpr auto kNodes = GroundLattice::kNodes;
constexpr auto kVertices = GroundLattice::kVertices;
constexpr auto kIndices = GroundLattice::kIndices;
constexpr auto kGridFloats = GroundLattice::kGridFloats;

std::vector<float> Vertices(std::span<const float> fractions) {
  std::vector<float> grid;
  grid.reserve(static_cast<size_t>(kVertices) * kGridFloats);
  const auto step = [fractions](int k) {
    return std::cmp_less(k, fractions.size())
               ? fractions[static_cast<size_t>(k)]
               : static_cast<float>(k) / static_cast<float>(kSide - 1);
  };
  for (int j = 0; j < kSide; ++j) {
    for (int i = 0; i < kSide; ++i) { grid.insert(grid.end(), {step(i), step(j), 0.0f}); }
  }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {step(k), 0.0f, 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {step(k), 1.0f, 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {0.0f, step(k), 1.0f}); }
  for (int k = 0; k < kSide; ++k) { grid.insert(grid.end(), {1.0f, step(k), 1.0f}); }

  return grid;
}

std::vector<uint32_t> Indices() {
  std::vector<uint32_t> index;
  index.reserve(kIndices);
  const auto at = [](int i, int j) { return static_cast<uint32_t>(j * kSide + i); };
  for (int j = 0; j + 1 < kSide; ++j) {
    for (int i = 0; i + 1 < kSide; ++i) {
      index.insert(
          index.end(),
          {at(i, j), at(i, j + 1), at(i + 1, j + 1), at(i, j), at(i + 1, j + 1), at(i + 1, j)});
    }
  }
  const auto skirt = [&index](uint32_t edgeA, uint32_t edgeB, uint32_t dropA, uint32_t dropB) {
    index.insert(index.end(), {edgeA, dropA, edgeB, edgeB, dropA, dropB});
  };
  const uint32_t north = kNodes;
  const uint32_t south = kNodes + static_cast<uint32_t>(kSide);
  const uint32_t west = kNodes + 2u * static_cast<uint32_t>(kSide);
  const uint32_t east = kNodes + 3u * static_cast<uint32_t>(kSide);
  for (uint32_t k = 0; k + 1 < static_cast<uint32_t>(kSide); ++k) {
    skirt(at(static_cast<int>(k), 0), at(static_cast<int>(k + 1), 0), north + k, north + k + 1);
    skirt(at(static_cast<int>(k + 1), kSide - 1),
          at(static_cast<int>(k), kSide - 1),
          south + k + 1,
          south + k);
    skirt(at(0, static_cast<int>(k + 1)), at(0, static_cast<int>(k)), west + k + 1, west + k);
    skirt(at(kSide - 1, static_cast<int>(k)),
          at(kSide - 1, static_cast<int>(k + 1)),
          east + k,
          east + k + 1);
  }

  return index;
}
}

Native::Result<void> GroundLattice::BuildGrid(std::span<const float> fractions,
                                              std::unique_ptr<Native::Buffer> &into) {
  const auto grid = Vertices(fractions);
  const auto index = Index_ ? std::vector<uint32_t>{} : Indices();
  auto madeGrid = Device_->CreateBuffer(
      {.Bytes = grid.size() * sizeof(float),
       .Use = Native::BufferUse::Vertex | Native::BufferUse::CopyDestination});
  if (!madeGrid) { return std::unexpected(madeGrid.error()); }
  std::unique_ptr<Native::Buffer> madeIndex;
  if (!Index_) {
    auto buffer = Device_->CreateBuffer(
        {.Bytes = index.size() * sizeof(uint32_t),
         .Use = Native::BufferUse::Index | Native::BufferUse::CopyDestination});
    if (!buffer) { return std::unexpected(buffer.error()); }
    madeIndex = std::move(*buffer);
  }
  const auto gridBytes = std::as_bytes(std::span(grid));
  const auto indexBytes = std::as_bytes(std::span(index));
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  auto staging =
      Device_->CreateBuffer({.Bytes = gridBytes.size() + (madeIndex ? indexBytes.size() : 0),
                             .Use = Native::BufferUse::CopySource,
                             .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = Device_->MapWrite(**staging, {.Bytes = (*staging)->Description().Bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), gridBytes.data(), gridBytes.size());
  if (madeIndex) {
    std::memcpy(mapped->data() + gridBytes.size(), indexBytes.data(), indexBytes.size());
  }
  auto accepted = Device_->Unmap(**staging);
  if (!accepted) { return accepted; }
  accepted = (*commands)->Copy(**staging, **madeGrid, {.Bytes = gridBytes.size()});
  if (!accepted) { return accepted; }
  if (madeIndex) {
    accepted = (*commands)->Copy(
        **staging, *madeIndex, {.SourceOffset = gridBytes.size(), .Bytes = indexBytes.size()});
    if (!accepted) { return accepted; }
  }
  auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  if (madeIndex) { Index_ = std::move(madeIndex); }
  into = std::move(*madeGrid);
  return {};
}
}
