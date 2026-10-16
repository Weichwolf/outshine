#include "GroundLattice.h"
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
uint64_t GroundLattice::HeldBytes() const {
  uint64_t bytes = uint64_t{PageLayers_} * kPagesPerLayer * kPageNodes * sizeof(float);
  for (const auto *buffer : {Grid_.get(),
                             UniformGrid_.get(),
                             Index_.get(),
                             Instances_.get(),
                             Visible_.get(),
                             VisibleStaging_.get(),
                             PatchGrid_.get(),
                             PatchIndex_.get(),
                             PatchFractions_.get(),
                             PatchList_.get()}) {
    if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
  }
  return bytes;
}

Native::Result<std::unique_ptr<Native::Buffer>>
GroundLattice::PatchBuffer(std::span<const std::byte> bytes, Native::BufferUse use) {
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
GroundLattice::PatchFractions(std::span<const float> fractions) {
  std::array<float, size_t{2} * kSide> values{};
  for (size_t at = 0; at < static_cast<size_t>(kSide); ++at) {
    const float uniform = static_cast<float>(at) / static_cast<float>(kSide - 1);
    values[at] = fractions.empty() ? uniform : fractions[at];
    values[at + kSide] = uniform;
  }
  return PatchBuffer(std::as_bytes(std::span(values)), Native::BufferUse::Storage);
}

Native::Result<void> GroundLattice::BuildPatchGrid() {
  std::array<float, size_t{kPatchSide} * kPatchSide * kGridFloats> vertices{};
  for (uint32_t y = 0; y < kPatchSide; ++y) {
    for (uint32_t x = 0; x < kPatchSide; ++x) {
      const size_t at = static_cast<size_t>(y * kPatchSide + x) * kGridFloats;
      vertices[at] = static_cast<float>(x);
      vertices[at + 1] = static_cast<float>(y);
    }
  }
  std::array<uint32_t, kPatchIndices> indices{};
  size_t at = 0;
  for (uint32_t y = 0; y + 1u < kPatchSide; ++y) {
    for (uint32_t x = 0; x + 1u < kPatchSide; ++x) {
      const uint32_t a = y * kPatchSide + x;
      for (const uint32_t one :
           {a, a + kPatchSide, a + kPatchSide + 1u, a, a + kPatchSide + 1u, a + 1u}) {
        indices[at++] = one;
      }
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
