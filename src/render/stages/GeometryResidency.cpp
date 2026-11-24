#include "GeometryResidency.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {

bool GeometryResidency::CanReuseImagesFrom(const GeometryResidency &source) const noexcept {
  return Device_ != nullptr && Device_ == source.Device_ && Uploads_ == source.Uploads_ &&
         source.UploadCommands_ == nullptr && source.StagedImages_.empty();
}

bool GeometryResidency::ReusePieceStreamsFrom(const GeometryResidency &source) {
  if (&source == this || Device_ != nullptr || source.Device_ == nullptr ||
      source.UploadCommands_ != nullptr || source.StagedCount_ != 0) {
    return false;
  }
  constexpr std::array streams{Stream::Vertex,
                               Stream::Normal,
                               Stream::Index,
                               Stream::Uv,
                               Stream::Uv1,
                               Stream::Colour,
                               Stream::Tangent};
  for (const auto stream : streams) {
    const auto index = static_cast<size_t>(stream);
    Buffers_[index] = source.Buffers_[index];
    Held_[index] = source.Held_[index];
  }
  Device_ = source.Device_;
  TopV_ = source.TopV_;
  TopI_ = source.TopI_;
  TopC_ = source.TopC_;
  TopT_ = source.TopT_;
  FreeV_ = source.FreeV_;
  FreeI_ = source.FreeI_;
  FreeC_ = source.FreeC_;
  FreeT_ = source.FreeT_;
  GiveVertices(source.MainVertexRange_);
  GiveIndices(source.MainIndexRange_);
  if (source.DrawIndexRange_.Count != 0 &&
      source.DrawIndexRange_.First + source.DrawIndexRange_.Count == TopI_) {
    TopI_ = source.DrawIndexRange_.First;
  } else {
    GiveIndices(source.DrawIndexRange_);
  }
  GiveColours(source.MainColourRange_);
  GiveTangents(source.MainTangentRange_);
  return true;
}

Native::Result<bool> GeometryResidency::ReserveDrawIndices(uint32_t words) {
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "draw index storage requires a device"});
  }
  const auto limit = std::min<uint64_t>(Device_->Limits().MaxStorageBufferBytes,
                                        std::numeric_limits<uint32_t>::max()) /
                     sizeof(uint32_t);
  const bool reuse = DrawIndexRange_.Count >= words;
  const bool tail =
      DrawIndexRange_.Count != 0 && DrawIndexRange_.First + DrawIndexRange_.Count == TopI_;
  const bool hole =
      std::ranges::any_of(FreeI_, [words](Range span) { return span.Count >= words; });
  const auto sourceWords = tail ? DrawIndexRange_.First : TopI_;
  const uint64_t count = reuse || hole ? TopI_ : uint64_t{sourceWords} + words;
  const auto &source = Buffer(Stream::Index);
  if (words == 0 || count > limit || (source && source.use_count() > 1)) {
    GiveIndices(std::exchange(DrawIndexRange_, {}));
    return false;
  }
  const auto grown = Resize(Stream::Index,
                            {.Usage = Native::BufferUse::Storage | Native::BufferUse::Index,
                             .Bytes = static_cast<uint32_t>(count * sizeof(uint32_t)),
                             .Deferred = true,
                             .PreserveBytes = uint64_t{sourceWords} * sizeof(uint32_t),
                             .Growth = CapacityGrowth::Exact},
                            true);
  if (!grown) { return std::unexpected(grown.error()); }
  if (reuse) { return true; }
  if (tail && !hole) {
    TopI_ = static_cast<uint32_t>(count);
    DrawIndexRange_.Count = words;
  } else {
    GiveIndices(std::exchange(DrawIndexRange_, TakeIndices(words)));
  }
  return true;
}

Native::Result<void> GeometryResidency::StandsOn(Native::Device *device) {
  if (device == nullptr || (UploadCommands_ != nullptr && device != Device_) ||
      (Device_ != nullptr && Device_ != device &&
       (HeldBytes() != 0 || StagedCount_ != 0 || !StagedImages_.empty()))) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "resident streams require their original device"});
  }
  if (Device_ != device) {
    ReleaseUploadScratch();
    DefaultImages_.clear();
  }
  Device_ = device;
  return {};
}

GeometryResidency::AllocationStats GeometryResidency::Allocations() const {
  AllocationStats result{.StreamBytes = Held_,
                         .TransferBytes = uint64_t{StagingBytes_} + BulkBytes_,
                         .VertexSlots = TopV_,
                         .IndexSlots = TopI_};
  for (const auto &image : StagedImages_) {
    result.TransferBytes += image.Staging->Description().Bytes;
  }
  for (const Range &range : FreeV_) { result.FreeVertexSlots += range.Count; }
  for (const Range &range : FreeI_) { result.FreeIndexSlots += range.Count; }
  for (const RetiredTransfer &transfer : Retired_) { result.TransferBytes += transfer.Bytes; }
  for (const auto &one : RetiredBuffers_) { result.RetiredDeviceBytes += one->Description().Bytes; }
  result.DeviceCopyBytes = DeviceCopyBytes_;
  return result;
}

size_t GeometryResidency::TakeUploadAttempts() {
  return std::exchange(UploadAttempts_, size_t{0});
}

size_t GeometryResidency::TotalUploadAttempts() const {
  return TotalUploadAttempts_;
}

size_t GeometryResidency::RecordedCrossings() const {
  return RecordedCrossings_;
}

size_t GeometryResidency::TakeUploadBytes() {
  return std::exchange(UploadBytes_, size_t{0});
}

size_t GeometryResidency::TakeBufferAllocationAttempts() {
  return std::exchange(BufferAttempts_, size_t{0});
}

size_t GeometryResidency::TakeStagingAllocationAttempts() {
  return std::exchange(StagingAttempts_, size_t{0});
}

GeometryResidency::Range
GeometryResidency::Take(std::vector<Range> &free, uint32_t count, uint32_t &top) {
  for (size_t at = 0; at < free.size(); ++at) {
    if (free[at].Count < count) { continue; }
    const Range taken{.First = free[at].First, .Count = count};
    free[at].First += count;
    free[at].Count -= count;
    if (free[at].Count == 0) { free.erase(free.begin() + static_cast<long>(at)); }
    return taken;
  }
  const Range taken{.First = top, .Count = count};
  top += count;
  return taken;
}

void GeometryResidency::Give(std::vector<Range> &free, Range back) {
  if (back.Count == 0) { return; }
  const auto after =
      std::ranges::lower_bound(free, back.First, {}, [](const Range &one) { return one.First; });
  const auto at = free.insert(after, back);
  const size_t here = static_cast<size_t>(at - free.begin());
  if (here + 1 < free.size() && free[here].First + free[here].Count == free[here + 1].First) {
    free[here].Count += free[here + 1].Count;
    free.erase(free.begin() + static_cast<long>(here) + 1);
  }
  if (here > 0 && free[here - 1].First + free[here - 1].Count == free[here].First) {
    free[here - 1].Count += free[here].Count;
    free.erase(free.begin() + static_cast<long>(here));
  }
}

}
