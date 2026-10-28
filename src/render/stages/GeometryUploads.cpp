#include "UploadQueue.h"
#include "GeometryResidency.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Render {
GeometryResidency::UploadRecording::UploadRecording(GeometryResidency &owner) noexcept
    : Owner_(&owner) {}

GeometryResidency::UploadRecording::UploadRecording(UploadRecording &&other) noexcept
    : Owner_(std::exchange(other.Owner_, nullptr)) {}

GeometryResidency::UploadRecording::~UploadRecording() {
  if (Owner_ != nullptr) { Owner_->UploadCommands_ = nullptr; }
}

Native::Result<GeometryResidency::UploadRecording>
GeometryResidency::RecordUploads(Native::Commands &commands) {
  if (Device_ == nullptr || UploadCommands_ != nullptr) {
    return std::unexpected(Native::Error{
        .Kind = Native::ErrorKind::Invalid,
        .Message = "upload recording requires a device and only one active recording"});
  }
  UploadFailure_.reset();
  UploadCommands_ = &commands;
  return UploadRecording(*this);
}

const Native::Error *GeometryResidency::UploadRecording::Failure() const noexcept {
  return Owner_ != nullptr && Owner_->UploadFailure_ ? &*Owner_->UploadFailure_ : nullptr;
}

namespace {
bool MatchesUsage(const Native::Buffer &buffer, Native::UseMask<Native::BufferUse> usage) {
  const auto wanted = static_cast<uint32_t>(usage);
  return (static_cast<uint32_t>(buffer.Description().Use) & wanted) == wanted;
}

std::optional<uint32_t> UploadBytes(std::span<const GeometryResidency::Crossing> crossings) {
  uint64_t total = 0;
  constexpr auto maximum = std::numeric_limits<uint32_t>::max();
  for (const auto &crossing : crossings) {
    if (static_cast<size_t>(crossing.Which) >= GeometryResidency::kStreams) { return std::nullopt; }
    if (crossing.Bytes == 0 || !crossing.HasSource()) { continue; }
    if (uint64_t{crossing.Offset} + crossing.Bytes > maximum ||
        crossing.Offset % sizeof(uint32_t) != 0 || crossing.Bytes % sizeof(uint32_t) != 0) {
      return std::nullopt;
    }
    total = (total + crossing.Bytes + 15u) & ~uint64_t{15};
    if (total > maximum) { return std::nullopt; }
  }
  return static_cast<uint32_t>(total);
}

Native::Result<std::unique_ptr<Native::Buffer>> UploadBuffer(Native::Device &device,
                                                             uint32_t bytes) {
  return device.CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
}
}

Native::Result<void> GeometryResidency::SubmitPendingUploads() {
  if (StagedCount_ == 0 && StagedImages_.empty()) { return {}; }
  if (Device_ == nullptr) {
    return UploadFailed(
        Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = "uploads require a device"});
  }
  if (UploadCommands_ != nullptr) {
    const auto copied = RecordCrossings(*UploadCommands_);
    if (copied) { CommitCrossings(); }
    return copied ? copied : UploadFailed(copied.error());
  }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return UploadFailed(commands.error()); }
  const auto copied = RecordCrossings(**commands);
  if (!copied) { return UploadFailed(copied.error()); }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return UploadFailed(submitted.error()); }
  CommitCrossings();
  return {};
}

Native::Result<void> GeometryResidency::Cross(std::span<Crossing> what, bool deferred) {
  deferred = deferred || UploadCommands_ != nullptr;
  const auto measured = UploadBytes(what);
  if (!measured || Device_ == nullptr) {
    return UploadFailed(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                      .Message = "invalid resident stream upload"});
  }
  const uint32_t total = *measured;
  constexpr uint32_t batchBytes = 16u * 1024u * 1024u;
  const bool full = deferred && uint64_t{PendingUploadBytes_} + total > batchBytes;
  if (!deferred || full) {
    const auto flushed = SubmitPendingUploads();
    if (!flushed) { return UploadFailed(flushed.error()); }
  }
  BufferChanges previous;
  const auto prepared = PrepareBuffers(what, previous);
  if (!prepared) { return UploadFailed(prepared.error()); }
  if (total == 0) {
    RetireBuffers(previous);
    return {};
  }
  const auto accepted = deferred ? StageUploads(what, total) : Submit(what, total);
  if (!accepted) {
    RestoreBuffers(previous);
  } else {
    RetireBuffers(previous);
  }
  return accepted ? accepted : UploadFailed(accepted.error());
}

Native::Result<void> GeometryResidency::PrepareBuffers(std::span<Crossing> what,
                                                       BufferChanges &previous) {
  std::array<std::unique_ptr<Native::Buffer>, kStreams> candidates;
  auto capacities = Held_;
  std::array<bool, kStreams> changed{};
  for (const auto &one : what) {
    const auto slot = static_cast<size_t>(one.Which);
    if (one.Bytes == 0 || !one.HasSource()) {
      candidates[slot].reset();
      capacities[slot] = 0;
      changed[slot] = true;
      continue;
    }
    const auto *present = changed[slot] ? candidates[slot].get() : Buffers_[slot].get();
    if (capacities[slot] < one.Offset + one.Bytes || present == nullptr ||
        !MatchesUsage(*present, one.Usage)) {
      ++BufferAttempts_;
      auto candidate = Device_->CreateBuffer(
          {.Bytes = uint64_t{one.Offset} + one.Bytes,
           .Use = one.Usage | Native::BufferUse::CopySource | Native::BufferUse::CopyDestination});
      if (!candidate) { return std::unexpected(candidate.error()); }
      candidates[slot] = std::move(*candidate);
      capacities[slot] = one.Offset + one.Bytes;
      changed[slot] = true;
    }
  }
  for (size_t slot = 0; slot < kStreams; ++slot) {
    if (!changed[slot]) { continue; }
    previous.Buffers[slot] = std::move(Buffers_[slot]);
    previous.Capacities[slot] = Held_[slot];
    previous.Changed[slot] = true;
    Buffers_[slot] = std::move(candidates[slot]);
    Held_[slot] = capacities[slot];
  }
  return {};
}

void GeometryResidency::RestoreBuffers(BufferChanges &previous) {
  for (size_t slot = 0; slot < kStreams; ++slot) {
    if (!previous.Changed[slot]) { continue; }
    Buffers_[slot] = std::move(previous.Buffers[slot]);
    Held_[slot] = previous.Capacities[slot];
  }
}

void GeometryResidency::RetireBuffers(BufferChanges &previous) {
  if (StagedCount_ == 0) { return; }
  for (auto &buffer : previous.Buffers) {
    if (buffer) { RetiredBuffers_.push_back(std::move(buffer)); }
  }
}

Native::Result<void> GeometryResidency::PreserveBuffer(Native::Buffer &source,
                                                       Native::Buffer &target,
                                                       uint32_t bytes,
                                                       bool deferred) {
  if (deferred) {
    if (StagedCount_ == Staged_.size()) { Staged_.push_back(Staged{}); }
    Staged_[StagedCount_++] = {
        .Into = &target, .From = 0, .Bytes = bytes, .Offset = 0, .Staging = &source};
    PendingUploadBytes_ += bytes;
  } else {
    auto commands = BeginUpload(*Device_, Uploads_);
    if (!commands) { return std::unexpected(commands.error()); }
    auto copied = (*commands)->Copy(source, target, {.Bytes = bytes});
    if (!copied) { return copied; }
    auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
    if (!submitted) { return std::unexpected(submitted.error()); }
  }
  DeviceCopyBytes_ += bytes;
  return {};
}

Native::Result<std::span<std::byte>> GeometryResidency::PrepareStaging(uint32_t total) {
  if (Staging_ && uint64_t{StagingUsed_} + total <= StagingBytes_) {
    auto mapped = Device_->MapWrite(*Staging_, {.Bytes = StagingBytes_});
    if (mapped || mapped.error().Kind != Native::ErrorKind::Busy) { return mapped; }
  }
  const auto widened = std::max(total, StagingBytes_);
  ++StagingAttempts_;
  auto fresh = UploadBuffer(*Device_, widened);
  if (!fresh) { return std::unexpected(fresh.error()); }
  auto mapped = Device_->MapWrite(**fresh, {.Bytes = widened});
  if (!mapped) { return std::unexpected(mapped.error()); }
  if (Staging_ && StagedCount_ > 0) {
    Retired_.push_back({.Buffer = std::move(Staging_), .Bytes = StagingBytes_});
  }
  Staging_ = std::move(*fresh);
  StagingBytes_ = widened;
  StagingUsed_ = 0;
  return mapped;
}

void GeometryResidency::Fill(std::span<std::byte> mapped,
                             uint32_t at,
                             std::span<const Crossing> what) {
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.HasSource()) { continue; }
    if (one.Writes != nullptr) {
      one.Writes(one.Carrying,
                 reinterpret_cast<float *>(mapped.data() + at),
                 one.Bytes / static_cast<uint32_t>(sizeof(float)));
    } else {
      std::memcpy(mapped.data() + at, one.From, one.Bytes);
    }
    at = (at + one.Bytes + 15u) & ~15u;
  }
}

Native::Result<void> GeometryResidency::StageUploads(std::span<Crossing> what, uint32_t total) {
  const auto mapped = PrepareStaging(total);
  if (!mapped) { return std::unexpected(mapped.error()); }
  Fill(*mapped, StagingUsed_, what);
  const auto unmapped = Device_->Unmap(*Staging_);
  if (!unmapped) { return unmapped; }
  auto at = StagingUsed_;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.HasSource()) { continue; }
    if (StagedCount_ == Staged_.size()) { Staged_.push_back(Staged{}); }
    Staged_[StagedCount_++] = {.Into = Buffer(one.Which).get(),
                               .From = at,
                               .Bytes = one.Bytes,
                               .Offset = one.Offset,
                               .Staging = Staging_.get()};
    at = (at + one.Bytes + 15u) & ~15u;
  }
  StagingUsed_ = at;
  PendingUploadBytes_ += total;
  StagedThisFrame_ += total;
  return {};
}

Native::Result<std::span<std::byte>> GeometryResidency::PrepareBulk(uint32_t total) {
  if (Bulk_ && BulkBytes_ >= total) {
    auto mapped = Device_->MapWrite(*Bulk_, {.Bytes = total});
    if (mapped || mapped.error().Kind != Native::ErrorKind::Busy) { return mapped; }
  }
  const auto widened = std::max(BulkBytes_, total);
  ++StagingAttempts_;
  auto fresh = UploadBuffer(*Device_, widened);
  if (!fresh) { return std::unexpected(fresh.error()); }
  auto mapped = Device_->MapWrite(**fresh, {.Bytes = total});
  if (!mapped) { return std::unexpected(mapped.error()); }
  Bulk_ = std::move(*fresh);
  BulkBytes_ = widened;
  return mapped;
}

Native::Result<void> GeometryResidency::Submit(std::span<Crossing> what, uint32_t total) {
  if (Device_ == nullptr || total == 0) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "upload submission requires a device and bytes"});
  }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  ++UploadAttempts_;
  ++TotalUploadAttempts_;
  const auto mapped = PrepareBulk(total);
  if (!mapped) { return std::unexpected(mapped.error()); }
  Fill(*mapped, 0, what);
  const auto unmapped = Device_->Unmap(*Bulk_);
  if (!unmapped) { return unmapped; }
  uint32_t at = 0;
  for (const auto &one : what) {
    if (one.Bytes == 0 || !one.HasSource()) { continue; }
    const auto copied = (*commands)->Copy(
        *Bulk_,
        *Buffer(one.Which),
        {.SourceOffset = at, .DestinationOffset = one.Offset, .Bytes = one.Bytes});
    if (!copied) { return copied; }
    at = (at + one.Bytes + 15u) & ~15u;
  }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  UploadBytes_ += total;
  return {};
}

Native::Result<void> GeometryResidency::Grow(Stream which, Need need) {
  need.Deferred = need.Deferred || UploadCommands_ != nullptr;
  if (static_cast<size_t>(which) >= kStreams || Device_ == nullptr) {
    return UploadFailed(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                      .Message = "invalid resident stream growth"});
  }
  auto &held = Buffer(which);
  auto &stood = Held_[static_cast<size_t>(which)];
  if (held && stood >= need.Bytes && MatchesUsage(*held, need.Usage)) { return {}; }
  constexpr uint32_t batchBytes = 16u * 1024u * 1024u;
  if (!need.Deferred || uint64_t{PendingUploadBytes_} + stood > batchBytes) {
    const auto flushed = SubmitPendingUploads();
    if (!flushed) { return UploadFailed(flushed.error()); }
  }
  const uint32_t widened =
      stood >= need.Bytes
          ? stood
          : static_cast<uint32_t>(std::max(
                uint64_t{need.Bytes},
                std::min(uint64_t{stood} * 2u, uint64_t{std::numeric_limits<uint32_t>::max()})));
  ++BufferAttempts_;
  auto fresh = Device_->CreateBuffer(
      {.Bytes = widened,
       .Use = need.Usage | Native::BufferUse::CopySource | Native::BufferUse::CopyDestination});
  if (!fresh) { return UploadFailed(fresh.error()); }
  if (held && stood > 0 && need.Existing == ExistingContents::Preserve) {
    auto copied = PreserveBuffer(*held, **fresh, stood, need.Deferred);
    if (!copied) { return UploadFailed(copied.error()); }
  }
  if (held && StagedCount_ > 0) { RetiredBuffers_.push_back(std::move(held)); }
  held = std::move(*fresh);
  stood = widened;
  return {};
}

Native::Result<void> GeometryResidency::FlushCrossings(Native::Commands *commands) {
  if (StagedCount_ == 0 && StagedImages_.empty()) { return {}; }
  if (commands == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "pending uploads need commands"});
  }
  return RecordCrossings(*commands);
}

Native::Result<void> GeometryResidency::RecordCrossings(Native::Commands &commands) {
  for (const auto &image : StagedImages_) {
    const auto copied = RecordImage(commands, image);
    if (!copied) { return copied; }
  }
  for (size_t at = 0; at < StagedCount_; ++at) {
    const auto &staged = Staged_[at];
    const auto copied = commands.Copy(
        *staged.Staging,
        *staged.Into,
        {.SourceOffset = staged.From, .DestinationOffset = staged.Offset, .Bytes = staged.Bytes});
    if (!copied) { return copied; }
    ++RecordedCrossings_;
  }
  return {};
}

void GeometryResidency::CommitCrossings() {
  for (const auto &image : StagedImages_) { UploadBytes_ += image.Staging->Description().Bytes; }
  StagedImages_.clear();
  StagedImageAt_.clear();
  StagedCount_ = 0;
  StagingUsed_ = 0;
  PendingUploadBytes_ = 0;
  Retired_.clear();
  RetiredBuffers_.clear();
}

void GeometryResidency::ReleaseUploadScratch() {
  if (StagedCount_ != 0 || !StagedImages_.empty()) { return; }
  Bulk_.reset();
  BulkBytes_ = 0;
  Staging_.reset();
  StagingBytes_ = 0;
  Retired_.clear();
  RetiredBuffers_.clear();
}

}
