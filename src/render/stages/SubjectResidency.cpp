#include "SubjectResidency.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {

Native::Result<void> SubjectResidency::StandsOn(Native::Device *device) {
  if (device == nullptr ||
      (Device_ != nullptr && Device_ != device && (HeldBytes() != 0 || StagedCount_ != 0))) {
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

SubjectResidency::AllocationStats SubjectResidency::Allocations() const {
  AllocationStats result{.StreamBytes = Held_,
                         .TransferBytes = uint64_t{StagingBytes_} + BulkBytes_,
                         .VertexSlots = TopV_,
                         .IndexSlots = TopI_};
  for (const Range &range : FreeV_) { result.FreeVertexSlots += range.Count; }
  for (const Range &range : FreeI_) { result.FreeIndexSlots += range.Count; }
  for (const RetiredTransfer &transfer : Retired_) { result.TransferBytes += transfer.Bytes; }
  for (const auto &one : RetiredBuffers_) { result.RetiredDeviceBytes += one->Description().Bytes; }
  result.DeviceCopyBytes = DeviceCopyBytes_;
  return result;
}

size_t SubjectResidency::TakeUploadAttempts() {
  return std::exchange(UploadAttempts_, size_t{0});
}

size_t SubjectResidency::TotalUploadAttempts() const {
  return TotalUploadAttempts_;
}

size_t SubjectResidency::RecordedCrossings() const {
  return RecordedCrossings_;
}

size_t SubjectResidency::TakeUploadBytes() {
  return std::exchange(UploadBytes_, size_t{0});
}

size_t SubjectResidency::TakeBufferAllocationAttempts() {
  return std::exchange(BufferAttempts_, size_t{0});
}

size_t SubjectResidency::TakeStagingAllocationAttempts() {
  return std::exchange(StagingAttempts_, size_t{0});
}

SubjectResidency::Range
SubjectResidency::Take(std::vector<Range> &free, uint32_t count, uint32_t &top) {
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

void SubjectResidency::Give(std::vector<Range> &free, Range back) {
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
