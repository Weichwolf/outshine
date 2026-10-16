#include "GroundPatchSelection.h"
#include "GroundLattice.h"
#include "ComputeShaders.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
constexpr uint32_t kScanLanes = FindComputeShader(ComputeShaderId::GroundPatchScan)->Shape.GroupX;
constexpr uint64_t kRangeBytes = 2u * sizeof(uint32_t);
}

uint64_t GroundPatchSelection::HeldBytes() const {
  uint64_t bytes = 0;
  for (const auto *buffer : {Bounds_.get(),
                             Staging_.get(),
                             Kept_.get(),
                             Slots_.get(),
                             Selected_.get(),
                             Batch_.get(),
                             Arguments_.get(),
                             Ranges_.get(),
                             Totals_.get(),
                             Offsets_.get()}) {
    if (buffer != nullptr) { bytes += buffer->Description().Bytes; }
  }
  return bytes;
}

Native::Result<void> GroundPatchSelection::Reserve(Native::Device &device, uint32_t count) {
  if (Room_ >= count) { return {}; }
  const uint32_t room = std::max(
      count,
      Room_ <= std::numeric_limits<uint32_t>::max() / 2u ? std::max(64u, Room_ * 2u) : count);
  const uint64_t counter = std::max(uint64_t{4}, device.Limits().StorageBufferAlignment);
  const uint32_t blocks = room / kScanLanes + static_cast<uint32_t>(room % kScanLanes != 0u);
  using U = Native::BufferUse;
  const std::array descriptions{
      Native::BufferDescription{.Bytes = uint64_t{room} * sizeof(Bounds),
                                .Use = U::Storage | U::CopyDestination},
      Native::BufferDescription{.Bytes = uint64_t{room} * sizeof(uint32_t), .Use = U::Storage},
      Native::BufferDescription{.Bytes = uint64_t{room} * sizeof(uint32_t), .Use = U::Storage},
      Native::BufferDescription{.Bytes = uint64_t{room} * sizeof(uint32_t),
                                .Use = U::Storage | U::Vertex | U::CopySource},
      Native::BufferDescription{.Bytes = 2u * sizeof(uint32_t),
                                .Use = U::Storage | U::CopyDestination},
      Native::BufferDescription{.Bytes = counter + 4u * sizeof(uint32_t),
                                .Use =
                                    U::Storage | U::Indirect | U::CopyDestination | U::CopySource},
      Native::BufferDescription{.Bytes = uint64_t{blocks} * kRangeBytes,
                                .Use = U::Storage | U::CopyDestination},
      Native::BufferDescription{.Bytes = uint64_t{blocks} * sizeof(uint32_t), .Use = U::Storage},
      Native::BufferDescription{.Bytes = uint64_t{blocks} * sizeof(uint32_t), .Use = U::Storage}};
  std::array<std::unique_ptr<Native::Buffer>, descriptions.size()> buffers;
  for (size_t i = 0; i < buffers.size(); ++i) {
    auto made = device.CreateBuffer(descriptions[i]);
    if (!made) { return std::unexpected(made.error()); }
    buffers[i] = std::move(*made);
  }
  auto &[bounds, kept, slots, selected, batch, arguments, ranges, totals, offsets] = buffers;
  Bounds_ = std::move(bounds);
  Kept_ = std::move(kept);
  Slots_ = std::move(slots);
  Selected_ = std::move(selected);
  Batch_ = std::move(batch);
  Arguments_ = std::move(arguments);
  Ranges_ = std::move(ranges);
  Totals_ = std::move(totals);
  Offsets_ = std::move(offsets);
  Room_ = room;
  CounterOffset_ = counter;
  ArgumentOffset_ = counter - sizeof(uint32_t);
  return {};
}

Native::Result<void> GroundPatchSelection::Upload(Native::Device &device,
                                                  Native::Commands &commands) {
  if (Seen_.empty()) { return {}; }
  if (Seen_.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "too many ground patch candidates"});
  }
  const auto count = static_cast<uint32_t>(Seen_.size());
  const auto room = Reserve(device, count);
  if (!room) { return room; }
  const uint64_t boundsBytes = Seen_.size() * sizeof(Bounds);
  const uint64_t argumentBytes = Arguments_->Description().Bytes;
  const uint64_t batchBytes = kRangeBytes;
  Blocks_ = count / kScanLanes + static_cast<uint32_t>(count % kScanLanes != 0u);
  const uint64_t rangeBytes = uint64_t{Blocks_} * kRangeBytes;
  const uint64_t bytes = boundsBytes + argumentBytes + batchBytes + rangeBytes;
  Native::Result<std::span<std::byte>> mapped = std::unexpected(
      Native::Error{.Kind = Native::ErrorKind::Busy, .Message = "no patch staging"});
  if (Staging_ && Staging_->Description().Bytes >= bytes) {
    mapped = device.MapWrite(*Staging_, {.Bytes = bytes});
  }
  if (!mapped && mapped.error().Kind == Native::ErrorKind::Busy) {
    auto made = device.CreateBuffer({.Bytes = uint64_t{Room_} * sizeof(Bounds) + argumentBytes +
                                              batchBytes + Ranges_->Description().Bytes,
                                     .Use = Native::BufferUse::CopySource,
                                     .Placement = Native::Memory::Upload});
    if (!made) { return std::unexpected(made.error()); }
    mapped = device.MapWrite(**made, {.Bytes = bytes});
    if (!mapped) { return std::unexpected(mapped.error()); }
    Staging_ = std::move(*made);
  }
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), Seen_.data(), static_cast<size_t>(boundsBytes));
  std::memset(mapped->data() + boundsBytes, 0, static_cast<size_t>(argumentBytes));
  const uint32_t indices = GroundLattice::kPatchIndices;
  std::memcpy(mapped->data() + boundsBytes + ArgumentOffset_, &indices, sizeof(indices));
  const std::array batch{uint32_t{0}, Blocks_};
  std::memcpy(mapped->data() + boundsBytes + argumentBytes, batch.data(), sizeof(batch));
  for (uint32_t block = 0; block < Blocks_; ++block) {
    const uint32_t first = block * kScanLanes;
    const std::array range{first, std::min(kScanLanes, count - first)};
    std::memcpy(mapped->data() + boundsBytes + argumentBytes + batchBytes + block * kRangeBytes,
                range.data(),
                sizeof(range));
  }
  auto accepted = device.Unmap(*Staging_);
  if (!accepted) { return accepted; }
  accepted = commands.Copy(*Staging_, *Bounds_, {.Bytes = boundsBytes});
  if (!accepted) { return accepted; }
  accepted =
      commands.Copy(*Staging_, *Arguments_, {.SourceOffset = boundsBytes, .Bytes = argumentBytes});
  if (!accepted) { return accepted; }
  accepted = commands.Copy(
      *Staging_, *Batch_, {.SourceOffset = boundsBytes + argumentBytes, .Bytes = batchBytes});
  if (!accepted) { return accepted; }
  return commands.Copy(
      *Staging_,
      *Ranges_,
      {.SourceOffset = boundsBytes + argumentBytes + batchBytes, .Bytes = rangeBytes});
}
}
