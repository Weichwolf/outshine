#include "ClipmapSurface.h"
#include "ClipmapHeightUpload.h"
#include "ClipmapVisibility.h"
#include <array>
#include <cmath>
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
Native::Error Invalid(const char *message) {
  return {.Kind = Native::ErrorKind::Invalid, .Message = message};
}
}

ClipmapSurface::ClipmapSurface(Native::Device &device,
                               ClipmapHeightCache heights,
                               ClipmapHeightCache normals)
    : Device_(&device),
      Heights_(std::move(heights)),
      NormalHeights_(std::move(normals)),
      HeightRanges_(
          Heights_.Grid().Levels,
          {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()}) {}

Native::Result<std::shared_ptr<ClipmapSurface>>
ClipmapSurface::Make(Native::Device &device, ClipmapGrid grid, size_t bytesMost) {
  if (4 * uint64_t{grid.HalfCells} + 3 > device.Limits().MaxTextureExtent ||
      grid.Levels > device.Limits().MaxTextureLayers) {
    return std::unexpected(Invalid("clipmap normal grid extent overflow"));
  }
  const ClipmapGrid normalGrid{
      .HalfCells = grid.HalfCells * 2, .Levels = grid.Levels, .CellM = grid.CellM * 0.5};
  const auto heightBytes = ClipmapHeightCache::StorageBytes(grid);
  const auto normalBytes = ClipmapHeightCache::StorageBytes(normalGrid);
  if (!heightBytes || !normalBytes) {
    return std::unexpected(Invalid("invalid clipmap field layout"));
  }
  const uint64_t geometrySide = 2 * uint64_t{grid.HalfCells} + 3;
  const uint64_t normalSide = 4 * uint64_t{grid.HalfCells} + 3;
  const uint64_t geometryNodes = geometrySide * geometrySide * grid.Levels;
  const uint64_t normalNodes = normalSide * normalSide * grid.Levels;
  const uint64_t half = grid.HalfCells;
  const uint64_t triangles = 8 * half * half + (grid.Levels - 1) * (6 * half * half + 4 * half);
  const uint64_t held =
      *heightBytes + *normalBytes + geometryNodes * sizeof(float) +
      normalNodes * (sizeof(float) + 8) +
      uint64_t{grid.Levels} * (sizeof(ClipmapRing) * 3 + sizeof(std::array<float, 2>) * 2);
  if (held > bytesMost || triangles > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "clipmap fields exceed admitted storage or device extent"});
  }
  auto heights = ClipmapHeightCache::Make(grid, *heightBytes);
  auto normals = ClipmapHeightCache::Make(normalGrid, *normalBytes);
  if (!heights || !normals) {
    return std::unexpected(Invalid("clipmap CPU field allocation failed"));
  }
  std::shared_ptr<ClipmapSurface> result(
      new ClipmapSurface(device, std::move(*heights), std::move(*normals)));
  const auto ready = result->Allocate();
  if (!ready) { return std::unexpected(ready.error()); }
  return result;
}

Native::Result<void> ClipmapSurface::Allocate() {
  const auto image = [this](uint32_t side, Native::Format format, Native::TextureUse write) {
    return Device_->CreateTexture({.Width = side,
                                   .Height = side,
                                   .Layers = Heights_.Grid().Levels,
                                   .Type = Native::TextureType::Image2DArray,
                                   .Texels = format,
                                   .Use = Native::TextureUse::Sampled | write});
  };
  auto height =
      image(Heights_.Side(), Native::Format::R32Float, Native::TextureUse::CopyDestination);
  if (!height) { return std::unexpected(height.error()); }
  auto normalHeight =
      image(NormalHeights_.Side(), Native::Format::R32Float, Native::TextureUse::CopyDestination);
  if (!normalHeight) { return std::unexpected(normalHeight.error()); }
  auto normal =
      image(NormalHeights_.Side(), Native::Format::Rgba16Float, Native::TextureUse::Storage);
  if (!normal) { return std::unexpected(normal.error()); }
  const auto configured = NormalPass_.Configure(*Device_);
  if (!configured) { return configured; }
  HeightImage_ = std::move(*height);
  NormalHeightImage_ = std::move(*normalHeight);
  NormalImage_ = std::move(*normal);
  return {};
}

Native::Result<void> ClipmapSurface::Publish(const ClipmapHeightUpdate &heights,
                                             const ClipmapHeightUpdate &normals,
                                             size_t stagingBytesMost) {
  if (!Heights_.Validate(heights) || !NormalHeights_.Validate(normals)) {
    return std::unexpected(Invalid("clipmap publication has stale or invalid samples"));
  }
  if (heights.Samples().empty() && normals.Samples().empty()) { return {}; }
  if (heights.Samples().size_bytes() > stagingBytesMost ||
      normals.Samples().size_bytes() > stagingBytesMost - heights.Samples().size_bytes()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Busy,
                      .Message = "clipmap publication exceeds staging admission"});
  }
  auto commands = Device_->Begin();
  if (!commands) { return std::unexpected(commands.error()); }
  auto encoded = UploadClipmapHeights(
      *Device_, **commands, *HeightImage_, Heights_, heights, stagingBytesMost);
  if (encoded) {
    encoded = UploadClipmapHeights(
        *Device_, **commands, *NormalHeightImage_, NormalHeights_, normals, stagingBytesMost);
  }
  if (encoded) {
    encoded =
        NormalPass_.Encode(**commands, *NormalHeightImage_, *NormalImage_, NormalHeights_, normals);
  }
  if (!encoded) { return encoded; }
  const auto submitted = Device_->Submit(std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  const auto committedHeight = Heights_.Commit(heights);
  const auto committedNormal = NormalHeights_.Commit(normals);
  if (!committedHeight || !committedNormal) {
    return std::unexpected(Invalid("accepted clipmap publication lost its CPU generation"));
  }
  if (!heights.Samples().empty()) {
    for (uint32_t level = 0; level < Heights_.Grid().Levels; ++level) {
      HeightRanges_[level] = ClipmapHeightRange(Heights_, level);
    }
  }
  Rings_.clear();
  if (!heights.Samples().empty()) { ++Generation_; }
  return {};
}

Native::Result<void> ClipmapSurface::Prepare(const ClipmapView &view) {
  if (View_ == view && !Rings_.empty()) { return {}; }
  const auto origin = Heights_.Origin(0);
  if (!origin) { return std::unexpected(Invalid("clipmap height window is unpublished")); }
  Vec2 focus;
  const auto &grid = Heights_.Grid();
  const double reach = (grid.HalfCells - 2) * grid.CellM;
  for (size_t axis = 0; axis < 2; ++axis) {
    focus[axis] = static_cast<double>((*origin)[axis] + grid.HalfCells + 1) * grid.CellM;
    if (!std::isfinite(view.Camera[axis]) || std::abs(view.Camera[axis] - focus[axis]) > reach) {
      return std::unexpected(Invalid("camera left the prepared clipmap window"));
    }
  }
  auto sampled = view;
  sampled.Camera = sampled.PreviousCamera = focus;
  auto rings = MakeClipmapRings(Heights_, NormalHeights_, sampled);
  if (!rings) { return std::unexpected(Invalid("clipmap fields do not cover the current view")); }
  const auto bytes = std::as_bytes(std::span(*rings));
  if (Rings_.size() == rings->size() &&
      std::memcmp(Rings_.data(), bytes.data(), bytes.size()) == 0) {
    Shift_ = view.Shift;
    LodFocus_ = focus;
    View_ = view;
    return {};
  }
  auto upload = Device_->CreateBuffer({.Bytes = bytes.size(),
                                       .Use = Native::BufferUse::Storage,
                                       .Placement = Native::Memory::Upload});
  if (!upload) { return std::unexpected(upload.error()); }
  auto mapped = Device_->MapWrite(**upload, {.Bytes = bytes.size()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), bytes.data(), bytes.size());
  const auto unmapped = Device_->Unmap(**upload);
  if (!unmapped) { return unmapped; }
  RingBuffer_ = std::move(*upload);
  Rings_ = std::move(*rings);
  Shift_ = view.Shift;
  LodFocus_ = focus;
  View_ = view;
  return {};
}

bool ClipmapSurface::Complete() const noexcept {
  for (uint32_t level = 0; level < Heights_.Grid().Levels; ++level) {
    if (!Heights_.Complete(level) || !NormalHeights_.Complete(level)) { return false; }
  }
  return true;
}

ClipmapFields ClipmapSurface::Fields() const noexcept {
  return {.Heights = HeightImage_.get(),
          .Normals = NormalImage_.get(),
          .RingBuffer = RingBuffer_.get(),
          .Rings = Rings_,
          .HeightRanges = HeightRanges_,
          .HeightShift = static_cast<float>(Shift_[1])};
}

uint64_t ClipmapSurface::HeldBytes() const noexcept {
  return Heights_.HeapBytes() + NormalHeights_.HeapBytes() +
         Rings_.capacity() * sizeof(ClipmapRing) +
         HeightRanges_.capacity() * sizeof(std::array<float, 2>) + DeviceBytes();
}

uint64_t ClipmapSurface::DeviceBytes() const noexcept {
  const uint64_t geometryNodes =
      uint64_t{Heights_.Side()} * Heights_.Side() * Heights_.Grid().Levels;
  const uint64_t normalNodes =
      uint64_t{NormalHeights_.Side()} * NormalHeights_.Side() * NormalHeights_.Grid().Levels;
  return geometryNodes * sizeof(float) + normalNodes * (sizeof(float) + 8) +
         (RingBuffer_ ? RingBuffer_->Description().Bytes : 0);
}

uint32_t ClipmapSurface::Triangles() const noexcept {
  const uint64_t half = Heights_.Grid().HalfCells;
  return static_cast<uint32_t>(8 * half * half +
                               (Heights_.Grid().Levels - 1) * (6 * half * half + 4 * half));
}
}
