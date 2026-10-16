#include "GroundLattice.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
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
  std::array<float, 3> shift{};
  for (size_t axis = 0; axis < 3; ++axis) {
    shift[axis] = static_cast<float>(anchorM[static_cast<int>(axis)] +
                                     ctx.PreViewTranslation[static_cast<int>(axis)]);
  }
  const SidePlanes planes = SidePlanesOf(ctx.Mvp);
  Seen_.clear();
  for (size_t at = 0; at < Held_.size(); ++at) {
    const std::array<float, 4> &sphere = Bounds_[at];
    const float x = sphere[0] + shift[0];
    const float y = sphere[1] + shift[1];
    const float z = sphere[2] + shift[2];
    bool inside = true;
    for (const std::array<float, 4> &plane : planes) {
      if (plane[0] * x + plane[1] * y + plane[2] * z + plane[3] < -sphere[3]) {
        inside = false;
        break;
      }
    }
    if (!inside) { continue; }
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

Native::Result<void> GroundLattice::HandsVisible(Native::Commands &commands) {
  const auto count = static_cast<uint32_t>(Seen_.size());
  const uint64_t bytes = uint64_t{count} * sizeof(GroundInstance);
  if (!Visible_ || VisibleRoom_ < count) {
    const auto room = std::max(count, VisibleRoom_ > 0 ? VisibleRoom_ * 2u : 64u);
    auto created = Device_->CreateBuffer(
        {.Bytes = uint64_t{room} * sizeof(GroundInstance),
         .Use = Native::BufferUse::Vertex | Native::BufferUse::CopyDestination});
    if (!created) { return std::unexpected(created.error()); }
    Visible_ = std::move(*created);
    VisibleRoom_ = room;
  }
  Native::Result<std::span<std::byte>> mapped =
      std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy, .Message = "no staging"});
  if (VisibleStaging_ && VisibleStaging_->Description().Bytes >= bytes) {
    mapped = Device_->MapWrite(*VisibleStaging_, {.Bytes = bytes});
  }
  if (!mapped && mapped.error().Kind == Native::ErrorKind::Busy) {
    auto created = Device_->CreateBuffer({.Bytes = uint64_t{VisibleRoom_} * sizeof(GroundInstance),
                                          .Use = Native::BufferUse::CopySource,
                                          .Placement = Native::Memory::Upload});
    if (!created) { return std::unexpected(created.error()); }
    mapped = Device_->MapWrite(**created, {.Bytes = bytes});
    if (!mapped) { return std::unexpected(mapped.error()); }
    VisibleStaging_ = std::move(*created);
  }
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), Seen_.data(), static_cast<size_t>(bytes));
  const auto unmapped = Device_->Unmap(*VisibleStaging_);
  if (!unmapped) { return unmapped; }
  return commands.Copy(*VisibleStaging_, *Visible_, {.Bytes = bytes});
}
}
