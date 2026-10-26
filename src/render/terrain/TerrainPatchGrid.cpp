#include "TerrainLattice.h"
#include "TerrainResidency.h"
#include "TerrainPatchMesh.h"
#include <vector>
#include "UploadQueue.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
uint64_t TerrainResidency::HeldBytes() const {
  uint64_t bytes = uint64_t{PageLayers_} * TerrainLattice::kPagesPerLayer *
                   TerrainLattice::kPageNodes * sizeof(float);
  for (const auto *buffer : std::array<const Native::Buffer *, 7>{Grid_.get(),
                                                                  UniformGrid_.get(),
                                                                  Index_.get(),
                                                                  Instances_.get(),
                                                                  PatchGrid_.get(),
                                                                  PatchIndex_.get(),
                                                                  PatchFractions_.get()}) {
    if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
  }
  return bytes;
}

Native::Result<std::unique_ptr<Native::Buffer>>
TerrainResidency::PatchBuffer(std::span<const std::byte> bytes, Native::BufferUse use) {
  auto buffer = Device_->CreateBuffer(
      {.Bytes = bytes.size(), .Use = use | Native::BufferUse::CopyDestination});
  if (!buffer) { return std::unexpected(buffer.error()); }
  auto staging = Device_->CreateBuffer({.Bytes = bytes.size(),
                                        .Use = Native::BufferUse::CopySource,
                                        .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = Device_->MapWrite(**staging, {.Bytes = bytes.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), bytes.data(), bytes.size());
  auto accepted = Device_->Unmap(**staging);
  if (!accepted) { return std::unexpected(accepted.error()); }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  accepted = (*commands)->Copy(**staging, **buffer, {.Bytes = bytes.size()});
  if (!accepted) { return std::unexpected(accepted.error()); }
  auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  return std::move(*buffer);
}

Native::Result<std::unique_ptr<Native::Buffer>>
TerrainResidency::PatchFractions(std::span<const float> fractions) {
  std::array<float, size_t{2} * TerrainLattice::kSide> values{};
  for (size_t at = 0; at < static_cast<size_t>(TerrainLattice::kSide); ++at) {
    const float uniform = static_cast<float>(at) / static_cast<float>(TerrainLattice::kSide - 1);
    values[at] = fractions.empty() ? uniform : fractions[at];
    values[at + TerrainLattice::kSide] = uniform;
  }
  return PatchBuffer(std::as_bytes(std::span(values)), Native::BufferUse::Storage);
}

Native::Result<void> TerrainResidency::BuildPatchGrid() {
  std::array<float,
             size_t{TerrainLattice::kPatchSide} * TerrainLattice::kPatchSide *
                 TerrainLattice::kGridFloats>
      vertices{};
  for (uint32_t y = 0; y < TerrainLattice::kPatchSide; ++y) {
    for (uint32_t x = 0; x < TerrainLattice::kPatchSide; ++x) {
      const size_t at =
          static_cast<size_t>(y * TerrainLattice::kPatchSide + x) * TerrainLattice::kGridFloats;
      vertices[at] = static_cast<float>(x);
      vertices[at + 1] = static_cast<float>(y);
    }
  }
  std::vector<uint32_t> indices;
  for (const uint32_t stride : {1u, 2u, 4u, 8u}) {
    for (const auto &face : TerrainPatchFaces(stride)) {
      indices.insert(indices.end(), face.begin(), face.end());
    }
  }
  auto grid = PatchBuffer(std::as_bytes(std::span(vertices)), Native::BufferUse::Vertex);
  if (!grid) { return std::unexpected(grid.error()); }
  auto index = PatchBuffer(std::as_bytes(std::span(indices)), Native::BufferUse::Index);
  if (!index) { return std::unexpected(index.error()); }
  auto fractions = PatchFractions({});
  if (!fractions) { return std::unexpected(fractions.error()); }
  PatchGrid_ = std::move(*grid);
  PatchIndex_ = std::move(*index);
  PatchFractions_ = std::move(*fractions);
  return {};
}
}
