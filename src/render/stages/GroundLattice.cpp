#include "GroundLattice.h"
#include "UploadQueue.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
Native::Result<void> Upload(Native::Device &device,
                            UploadQueue *uploads,
                            Native::Buffer &into,
                            std::span<const std::byte> data) {
  auto commands = BeginUpload(device, uploads);
  if (!commands) { return std::unexpected(commands.error()); }
  auto staging = device.CreateBuffer({.Bytes = data.size(),
                                      .Use = Native::BufferUse::CopySource,
                                      .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = device.MapWrite(**staging, {.Bytes = data.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), data.data(), data.size());
  auto copied = device.Unmap(**staging);
  if (!copied) { return copied; }
  copied = (*commands)->Copy(**staging, into, {.Bytes = data.size()});
  if (!copied) { return copied; }
  auto submitted = SubmitUpload(device, uploads, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  return {};
}
}

Native::Result<void> GroundLattice::SetGrid(std::span<const float> fractions) {
  if (Device_ == nullptr || fractions.size() != static_cast<size_t>(kSide)) {
    return std::unexpected(Native::Error{
        .Kind = Native::ErrorKind::Invalid,
        .Message = "ground grid requires a device and exactly one fraction per node"});
  }
  const auto built = BuildGrid(fractions, Grid_);
  if (!built) { return built; }
  ++Generation_;
  return {};
}

void GroundLattice::ReleasePage(PageId which) {
  if (which == kNoPage || which >= PagesMade_ || PagesInUse_[which] == 0) { return; }
  PagesInUse_[which] = 0;
  Spare_.push_back(which);
  --PagesLive_;
  ++Generation_;
}

Native::Result<void> GroundLattice::SetInstances(std::span<const GroundTile> real,
                                                 std::span<const GroundTile> virtual_) {
  const size_t maximum = std::numeric_limits<uint32_t>::max() / sizeof(GroundInstance);
  if (real.size() > maximum || virtual_.size() > maximum - real.size()) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "the ground lattice has too many instances"});
  }
  const size_t total = real.size() + virtual_.size();
  if (total == 0) {
    if (!Held_.empty()) { ++Generation_; }
    Held_.clear();
    Bounds_.clear();
    RealCount_ = 0;
    VirtualCount_ = 0;
    VisibleReal_ = 0;
    VisibleVirtual_ = 0;
    return {};
  }
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "the ground lattice has no device"});
  }
  std::vector<GroundInstance> instances;
  std::vector<std::array<float, 4>> bounds;
  instances.reserve(total);
  bounds.reserve(total);
  for (const std::span<const GroundTile> tiles : {real, virtual_}) {
    for (const GroundTile &tile : tiles) {
      const GroundInstance &one = tile.Instance;
      instances.push_back(one);
      float reach = 0.0f;
      for (size_t corner = 0; corner < 4; ++corner) {
        const float e = one.Corners[corner * 2u];
        const float n = one.Corners[corner * 2u + 1u];
        reach = std::max(reach, e * e + n * n);
      }
      const float mid = 0.5f * (tile.LowM + tile.HighM);
      const float half = 0.5f * (tile.HighM - tile.LowM);
      bounds.push_back({{one.Row[12] + one.Row[8] * mid,
                         one.Row[13] + one.Row[9] * mid,
                         one.Row[14] + one.Row[10] * mid,
                         std::sqrt(reach + half * half)}});
    }
  }

  const auto count = static_cast<uint32_t>(instances.size());
  uint32_t room = InstanceRoom_ > 0 ? InstanceRoom_ : 64u;
  while (room < count) {
    if (room > static_cast<uint32_t>(maximum / 2u)) {
      room = count;
      break;
    }
    room *= 2u;
  }
  std::unique_ptr<Native::Buffer> replacement;
  Native::Buffer *target = Instances_.get();
  if (!Instances_ || InstanceRoom_ < count) {
    auto created = Device_->CreateBuffer(
        {.Bytes = uint64_t{room} * sizeof(GroundInstance),
         .Use = Native::BufferUse::Vertex | Native::BufferUse::CopyDestination});
    if (!created) { return std::unexpected(created.error()); }
    replacement = std::move(*created);
    target = replacement.get();
  }
  const auto uploaded = Upload(*Device_, Uploads_, *target, std::as_bytes(std::span(instances)));
  if (!uploaded) { return uploaded; }
  if (replacement) { Instances_ = std::move(replacement); }
  Held_ = std::move(instances);
  Bounds_ = std::move(bounds);
  InstanceRoom_ = room;
  RealCount_ = static_cast<uint32_t>(real.size());
  VirtualCount_ = static_cast<uint32_t>(virtual_.size());
  VisibleReal_ = RealCount_;
  VisibleVirtual_ = VirtualCount_;
  ++Generation_;
  return {};
}
}
