#include "SubjectLights.h"
#include "UploadQueue.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr size_t kScratchBytes = size_t{16} * 1024 * 1024;
constexpr uint64_t kSphereBoxTests = 1'000'000;

bool Same(const SubjectLight &a, const SubjectLight &b) {
  return a.PositionEcefM == b.PositionEcefM && a.Light.Kind == b.Light.Kind &&
         a.Light.Colour == b.Light.Colour && a.Light.Intensity == b.Light.Intensity &&
         a.Light.Position == b.Light.Position && a.Light.Direction == b.Light.Direction &&
         a.Light.InnerConeRad == b.Light.InnerConeRad &&
         a.Light.OuterConeRad == b.Light.OuterConeRad && a.Light.RangeM == b.Light.RangeM;
}

Native::Result<void> Buffer(Native::Device &device,
                            std::unique_ptr<Native::Buffer> &buffer,
                            uint64_t bytes,
                            Native::Memory placement) {
  if (buffer && buffer->Description().Bytes >= bytes && buffer->Description().Bytes / 2 <= bytes) {
    return {};
  }
  const auto use = placement == Native::Memory::Upload
                       ? Native::UseMask(Native::BufferUse::CopySource)
                       : Native::BufferUse::Storage | Native::BufferUse::CopyDestination;
  auto created =
      device.CreateBuffer({.Bytes = bytes, .Use = use, .Placement = placement}, "SubjectLights");
  if (!created) { return std::unexpected(created.error()); }
  buffer = std::move(*created);
  return {};
}
}

Native::Result<void> SubjectLights::Attach(Native::Device &device, UploadQueue *uploads) {
  if (uploads != nullptr && &uploads->Device() != &device) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "light upload queue belongs to another device"});
  }
  if (Device_ == &device) {
    Uploads_ = uploads;
    return {};
  }
  if (Recorded_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "light generation has an unsubmitted reader"});
  }
  if (Device_ != nullptr) {
    for (const auto &slot : Slots_) {
      if (!slot.LastUse) { continue; }
      const auto status = Device_->Poll(*slot.LastUse);
      if (!status) { return std::unexpected(status.error()); }
      if (*status != Native::CompletionStatus::Ready) {
        return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                             .Message = "previous light device still has readers"});
      }
    }
  }
  SubjectLights replacement;
  replacement.Device_ = &device;
  replacement.Uploads_ = uploads;
  const auto accepted = replacement.Set(Sources());
  if (!accepted) { return accepted; }
  *this = std::move(replacement);
  return {};
}

std::span<const SubjectLight> SubjectLights::Sources() const noexcept {
  return Current_ ? std::span<const SubjectLight>(Slots_[*Current_].Sources) : Pending_;
}

Native::Result<size_t> SubjectLights::FreeSlot() {
  for (size_t slot = 0; slot < Slots_.size(); ++slot) {
    if (Current_ && slot == *Current_) { continue; }
    const auto &last = Slots_[slot].LastUse;
    if (!last) { return slot; }
    auto status = Device_->Poll(*last);
    if (!status) { return std::unexpected(status.error()); }
    if (*status == Native::CompletionStatus::Ready) { return slot; }
  }
  return std::unexpected(
      Native::Error{.Kind = Native::ErrorKind::Busy,
                    .Message = "all bounded light generation slots are in flight"});
}

Native::Result<void> SubjectLights::Set(std::span<const SubjectLight> sources) {
  if (std::ranges::equal(sources, Sources(), Same)) { return {}; }
  if (Recorded_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "light generation has an unsubmitted reader"});
  }
  if (Device_ == nullptr) {
    Pending_.assign(sources.begin(), sources.end());
    return {};
  }
  auto free = FreeSlot();
  if (!free) { return std::unexpected(free.error()); }
  Slot candidate;
  candidate.Sources.assign(sources.begin(), sources.end());
  if (sources.size() > kMaxSubjectLights) {
    const auto maximum = static_cast<size_t>(Device_->Limits().MaxStorageBufferBytes);
    auto cells = BuildLightCells(sources,
                                 {.EmitterBytes = maximum,
                                  .IndexBytes = maximum,
                                  .ScratchBytes = kScratchBytes,
                                  .SphereBoxTests = kSphereBoxTests});
    if (!cells) {
      return std::unexpected(
          Native::Error{.Kind = Native::ErrorKind::Invalid, .Message = cells.error()});
    }
    const auto &data = candidate.Cells.emplace(std::move(*cells));
    candidate.Emitters = std::move(Slots_[*free].Emitters);
    candidate.Index = std::move(Slots_[*free].Index);
    candidate.Upload = std::move(Slots_[*free].Upload);
    const auto uploaded = Upload(candidate, data);
    if (!uploaded) { return uploaded; }
  }
  Slots_[*free] = std::move(candidate);
  Current_ = *free;
  Pending_.clear();
  return {};
}

Native::Result<void> SubjectLights::Upload(Slot &slot, const LightCells &cells) {
  const auto emitters = std::as_bytes(std::span(cells.Emitters));
  const auto index = std::as_bytes(std::span(cells.Words));
  auto prepared = Buffer(*Device_, slot.Emitters, emitters.size_bytes(), Native::Memory::Device);
  if (!prepared) { return prepared; }
  prepared = Buffer(*Device_, slot.Index, index.size_bytes(), Native::Memory::Device);
  if (!prepared) { return prepared; }
  prepared = Buffer(
      *Device_, slot.Upload, emitters.size_bytes() + index.size_bytes(), Native::Memory::Upload);
  if (!prepared) { return prepared; }
  auto mapped =
      Device_->MapWrite(*slot.Upload, {.Bytes = emitters.size_bytes() + index.size_bytes()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), emitters.data(), emitters.size_bytes());
  std::memcpy(mapped->data() + emitters.size_bytes(), index.data(), index.size_bytes());
  prepared = Device_->Unmap(*slot.Upload);
  if (!prepared) { return prepared; }
  auto commands = Device_->Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  prepared = (*commands)->Copy(*slot.Upload, *slot.Emitters, {.Bytes = emitters.size_bytes()});
  if (!prepared) { return prepared; }
  prepared =
      (*commands)->Copy(*slot.Upload,
                        *slot.Index,
                        {.SourceOffset = emitters.size_bytes(), .Bytes = index.size_bytes()});
  if (!prepared) { return prepared; }
  auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  slot.LastUse = *submitted;
  return {};
}

bool SubjectLights::FullEnumeration() const noexcept {
  if (!Current_) { return false; }
  const auto &cells = Slots_[*Current_].Cells;
  return cells && cells->FullEnumeration;
}

Native::Buffer *SubjectLights::Emitters() const noexcept {
  return Current_ ? Slots_[*Current_].Emitters.get() : nullptr;
}

Native::Buffer *SubjectLights::Index() const noexcept {
  return Current_ ? Slots_[*Current_].Index.get() : nullptr;
}

uint64_t SubjectLights::EmitterBytes() const noexcept {
  if (!Current_) { return 0; }
  const auto &cells = Slots_[*Current_].Cells;
  return cells ? cells->Emitters.size() * sizeof(SubjectLightUniform) : 0;
}

uint64_t SubjectLights::IndexBytes() const noexcept {
  if (!Current_) { return 0; }
  const auto &cells = Slots_[*Current_].Cells;
  return cells ? cells->Words.size() * sizeof(uint32_t) : 0;
}

void SubjectLights::Submitted(Native::Completion completion) noexcept {
  if (Recorded_ && Current_) { Slots_[*Current_].LastUse = completion; }
  Recorded_ = false;
}
}
