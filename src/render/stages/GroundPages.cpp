#include "UploadQueue.h"
#include "GroundLattice.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void> GroundLattice::BuildPages(uint32_t layers,
                                               std::unique_ptr<Native::Texture> &into) {
  auto pages =
      Device_->CreateTexture({.Width = static_cast<uint32_t>(kPageSide) * kPageColumns,
                              .Height = static_cast<uint32_t>(kPageSide) * kPageColumns,
                              .Layers = layers,
                              .Type = Native::TextureType::Image2DArray,
                              .Texels = Native::Format::R32Float,
                              .Use = Native::TextureUse::Sampled | Native::TextureUse::CopySource |
                                     Native::TextureUse::CopyDestination});
  if (!pages) { return std::unexpected(pages.error()); }
  if (!Nearest_) {
    auto sampler = Device_->CreateSampler({.Min = Native::Filter::Nearest,
                                           .Mag = Native::Filter::Nearest,
                                           .Mip = Native::Filter::Nearest});
    if (!sampler) { return std::unexpected(sampler.error()); }
    Nearest_ = std::move(*sampler);
  }
  into = std::move(*pages);
  return {};
}

Native::Result<void> GroundLattice::ReservePages(uint32_t count) {
  if (count <= PageLayers_ * kPagesPerLayer) { return {}; }
  if (Device_ == nullptr || count > kMaximumPages) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground height page capacity exceeded"});
  }
  const uint32_t needed = (count + kPagesPerLayer - 1u) / kPagesPerLayer;
  const uint32_t layers = std::min(kMaximumPageLayers, std::max({needed, PageLayers_ * 2u, 1u}));
  std::unique_ptr<Native::Texture> candidate;
  const auto created = BuildPages(layers, candidate);
  if (!created) { return created; }
  if (Pages_) {
    auto commands = BeginUpload(*Device_, Uploads_);
    if (!commands) { return std::unexpected(commands.error()); }
    for (uint32_t layer = 0; layer < PageLayers_; ++layer) {
      const Native::TextureRegion region{.Layer = layer,
                                         .Width = static_cast<uint32_t>(kPageSide) * kPageColumns,
                                         .Height = static_cast<uint32_t>(kPageSide) * kPageColumns};
      const auto copied = (*commands)->Copy(*Pages_, region, *candidate, region);
      if (!copied) { return copied; }
    }
    const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
    if (!submitted) { return std::unexpected(submitted.error()); }
  }
  Pages_ = std::move(candidate);
  PageLayers_ = layers;
  return {};
}

Native::Result<PageId> GroundLattice::PlacePage(std::span<const float> nodes) {
  if (Device_ == nullptr || nodes.size() != kPageNodes) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "ground height page requires a device and a complete page"});
  }
  if (Spare_.empty()) {
    const auto reserved = ReservePages(PagesMade_ + 1u);
    if (!reserved) { return std::unexpected(reserved.error()); }
  }
  const PageId page = Spare_.empty() ? PagesMade_ : Spare_.back();
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  auto staging = Device_->CreateBuffer({.Bytes = nodes.size_bytes(),
                                        .Use = Native::BufferUse::CopySource,
                                        .Placement = Native::Memory::Upload});
  if (!staging) { return std::unexpected(staging.error()); }
  auto mapped = Device_->MapWrite(**staging, {.Bytes = nodes.size_bytes()});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memcpy(mapped->data(), nodes.data(), nodes.size_bytes());
  const auto unmapped = Device_->Unmap(**staging);
  if (!unmapped) { return std::unexpected(unmapped.error()); }
  const auto copied = (*commands)->Copy(
      **staging,
      *Pages_,
      {.RowBytes = static_cast<uint64_t>(kPageSide) * sizeof(float),
       .Region = {.Layer = page / kPagesPerLayer,
                  .X = (page % kPageColumns) * static_cast<uint32_t>(kPageSide),
                  .Y = ((page % kPagesPerLayer) / kPageColumns) * static_cast<uint32_t>(kPageSide),
                  .Width = static_cast<uint32_t>(kPageSide),
                  .Height = static_cast<uint32_t>(kPageSide)}});
  if (!copied) { return std::unexpected(copied.error()); }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  if (Spare_.empty()) {
    ++PagesMade_;
    PagesInUse_.push_back(1);
  } else {
    Spare_.pop_back();
    PagesInUse_[page] = 1;
  }
  ++PagesLive_;
  Selection_.Page(page, nodes);
  ++Generation_;
  return page;
}
}
