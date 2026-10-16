#include "GroundLattice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
using SidePlanes = std::array<std::array<float, 4>, 4>;

[[nodiscard]] SidePlanes SidePlanesOf(const Mat4f &mvp) {
  const auto row = [&mvp](size_t i, size_t k) { return mvp[k * 4u + i]; };
  SidePlanes planes{};
  for (size_t k = 0; k < 4; ++k) {
    planes[0][k] = row(3, k) + row(0, k);
    planes[1][k] = row(3, k) - row(0, k);
    planes[2][k] = row(3, k) + row(1, k);
    planes[3][k] = row(3, k) - row(1, k);
  }
  for (std::array<float, 4> &plane : planes) {
    const float length = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
    if (length > 0.0f) {
      for (float &c : plane) { c /= length; }
    }
  }
  return planes;
}

[[nodiscard]] bool
Intersects(const SidePlanes &planes, const std::array<float, 4> &sphere, const Vec4f &shift) {
  const float x = sphere[0] + shift[0];
  const float y = sphere[1] + shift[1];
  const float z = sphere[2] + shift[2];
  return std::ranges::all_of(planes, [x, y, z, &sphere, &shift](const auto &plane) {
    const float a = plane[0] * x;
    const float b = plane[1] * y;
    const float c = plane[2] * z;
    const float allowance = 8.0f * std::numeric_limits<float>::epsilon() *
                            (std::abs(plane[0]) * (std::abs(sphere[0]) + std::abs(shift[0])) +
                             std::abs(plane[1]) * (std::abs(sphere[1]) + std::abs(shift[1])) +
                             std::abs(plane[2]) * (std::abs(sphere[2]) + std::abs(shift[2])) +
                             std::abs(plane[3]) + sphere[3]);
    return !(a + b + c + plane[3] < -sphere[3] - allowance);
  });
}

}

Native::Result<void>
GroundLattice::Cull(const FrameContext &ctx, const Vec3 &anchorM, Native::Commands &commands) {
  VisibleReal_ = 0;
  VisibleVirtual_ = 0;
  if (Held_.empty()) { return {}; }
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground visibility requires a device"});
  }
  Vec4f shift{};
  for (size_t axis = 0; axis < 3; ++axis) {
    shift[axis] = static_cast<float>(anchorM[static_cast<int>(axis)] +
                                     ctx.PreViewTranslation[static_cast<int>(axis)]);
  }
  const SidePlanes planes = SidePlanesOf(ctx.Mvp);
  Seen_.clear();
  for (size_t at = 0; at < Held_.size(); ++at) {
    if (!Intersects(planes, Bounds_[at], shift)) { continue; }
    Seen_.push_back(Held_[at]);
    if (at < RealCount_) {
      ++VisibleReal_;
    } else {
      ++VisibleVirtual_;
    }
  }

  if (!Seen_.empty()) {
    const auto copied = HandsVisible(commands);
    if (!copied) {
      VisibleReal_ = 0;
      VisibleVirtual_ = 0;
      return copied;
    }
  }
  return {};
}

Native::Result<void> GroundLattice::Cast(const PassRecording &into,
                                         const Mat4f &projection,
                                         const Vec4f &shift,
                                         std::span<const Native::Binding> bindings) const {
  if (!DrawEnabled_) { return {}; }
  const SidePlanes planes = SidePlanesOf(projection);
  const auto total = static_cast<uint32_t>(Held_.size());
  uint32_t at = 0;
  while (at < total) {
    if (!Intersects(planes, Bounds_[at], shift)) {
      ++at;
      continue;
    }
    const uint32_t first = at;
    const bool real = at < RealCount_;
    const uint32_t end = real ? RealCount_ : total;
    do { ++at; } while (at < end && Intersects(planes, Bounds_[at], shift));
    const uint32_t count = at - first;
    const auto drawn = Draw(
        into, Pipelines_->Depth(), Instances_, real ? count : 0, real ? 0 : count, bindings, first);
    if (!drawn) { return drawn; }
  }
  return {};
}

Native::Result<void> GroundLattice::HandsVisible(Native::Commands &commands) {
  const auto count = static_cast<uint32_t>(Seen_.size());
  const uint64_t tileBytes = uint64_t{count} * sizeof(GroundInstance);
  const uint64_t patchBytes = uint64_t{count} * kPatchCount * sizeof(uint32_t);
  const uint64_t bytes = tileBytes + patchBytes;
  if (!Visible_ || VisibleRoom_ < count) {
    const auto room = std::max(count, VisibleRoom_ > 0 ? VisibleRoom_ * 2u : 64u);
    auto created =
        Device_->CreateBuffer({.Bytes = uint64_t{room} * sizeof(GroundInstance),
                               .Use = Native::BufferUse::Vertex | Native::BufferUse::Storage |
                                      Native::BufferUse::CopyDestination});
    if (!created) { return std::unexpected(created.error()); }
    auto patches =
        Device_->CreateBuffer({.Bytes = uint64_t{room} * kPatchCount * sizeof(uint32_t),
                               .Use = Native::BufferUse::Vertex | Native::BufferUse::Storage |
                                      Native::BufferUse::CopyDestination});
    if (!patches) { return std::unexpected(patches.error()); }
    Visible_ = std::move(*created);
    PatchList_ = std::move(*patches);
    VisibleRoom_ = room;
  }
  Native::Result<std::span<std::byte>> mapped =
      std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy, .Message = "no staging"});
  if (VisibleStaging_ && VisibleStaging_->Description().Bytes >= bytes) {
    mapped = Device_->MapWrite(*VisibleStaging_, {.Bytes = bytes});
  }
  if (!mapped && mapped.error().Kind == Native::ErrorKind::Busy) {
    auto created =
        Device_->CreateBuffer({.Bytes = uint64_t{VisibleRoom_} *
                                        (sizeof(GroundInstance) + kPatchCount * sizeof(uint32_t)),
                               .Use = Native::BufferUse::CopySource,
                               .Placement = Native::Memory::Upload});
    if (!created) { return std::unexpected(created.error()); }
    mapped = Device_->MapWrite(**created, {.Bytes = bytes});
    if (!mapped) { return std::unexpected(mapped.error()); }
    VisibleStaging_ = std::move(*created);
  }
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), Seen_.data(), static_cast<size_t>(tileBytes));
  for (uint32_t tile = 0; tile < count; ++tile) {
    for (uint32_t patch = 0; patch < kPatchCount; ++patch) {
      const uint32_t code =
          (tile << kGroundPatchBits) | patch | (tile >= VisibleReal_ ? kGroundUniformBit : 0u);
      const size_t offset = static_cast<size_t>(tileBytes) +
                            (static_cast<size_t>(tile) * kPatchCount + patch) * sizeof(code);
      std::memcpy(mapped->data() + offset, &code, sizeof(code));
    }
  }
  const auto unmapped = Device_->Unmap(*VisibleStaging_);
  if (!unmapped) { return unmapped; }
  auto copied = commands.Copy(*VisibleStaging_, *Visible_, {.Bytes = tileBytes});
  if (!copied) { return copied; }
  return commands.Copy(
      *VisibleStaging_, *PatchList_, {.SourceOffset = tileBytes, .Bytes = patchBytes});
}
}
