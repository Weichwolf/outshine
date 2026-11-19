#include "TerrainLattice.h"
#include "UploadQueue.h"
#include "TerrainResidency.h"
#include "GroundPageUpload.h"
#include "VectorCapacity.h"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
Native::Result<std::unique_ptr<Native::Texture>>
TerrainResidency::ReplacementPages(uint32_t pages) {
  const uint32_t needed =
      (pages + TerrainLattice::kPagesPerLayer - 1u) / TerrainLattice::kPagesPerLayer;
  uint32_t layers = PageLayers_;
  if (needed > layers) {
    layers = std::min(TerrainLattice::kMaximumPageLayers, std::max({needed, layers * 2u, 1u}));
  } else if (Pages_.use_count() <= 1) {
    return std::unique_ptr<Native::Texture>{};
  }
  return Device_->CreateTexture(
      {.Width = static_cast<uint32_t>(TerrainLattice::kPageSide) * TerrainLattice::kPageColumns,
       .Height = static_cast<uint32_t>(TerrainLattice::kPageSide) * TerrainLattice::kPageColumns,
       .Layers = layers,
       .Type = Native::TextureType::Image2DArray,
       .Texels = Native::Format::R32Float,
       .Use = Native::TextureUse::Sampled | Native::TextureUse::CopySource |
              Native::TextureUse::CopyDestination});
}

Native::Result<std::vector<PageId>>
TerrainResidency::PlacePages(std::span<const HeightPageView> nodes) {
  if (nodes.size() > kHeightPageBatchMost) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground height page batch exceeds its budget"});
  }
  if (nodes.empty()) { return std::vector<PageId>{}; }
  if (Device_ == nullptr || std::ranges::any_of(nodes, [](HeightPageView page) {
        return page.size() != TerrainLattice::kPageNodes;
      })) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "ground height page requires a device and a complete page"});
  }
  const size_t recycled = std::min(Spare_.size(), nodes.size());
  const size_t fresh = nodes.size() - recycled;
  if (fresh > TerrainLattice::kMaximumPages - PagesMade_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground height page capacity exceeded"});
  }
  const uint32_t made = PagesMade_ + static_cast<uint32_t>(fresh);
  std::vector<PageId> pages(nodes.size());
  for (size_t at = 0; at < pages.size(); ++at) {
    pages[at] = at < recycled ? Spare_[Spare_.size() - 1u - at]
                              : PagesMade_ + static_cast<uint32_t>(at - recycled);
  }
  GrowCapacity(PagesInUse_, made);
  auto pagesForWrite = ReplacementPages(made);
  if (!pagesForWrite) { return std::unexpected(pagesForWrite.error()); }
  auto candidate = std::move(*pagesForWrite);
  std::unique_ptr<Native::Sampler> sampler;
  if (!Nearest_) {
    auto created = Device_->CreateSampler({.Min = Native::Filter::Nearest,
                                           .Mag = Native::Filter::Nearest,
                                           .Mip = Native::Filter::Nearest});
    if (!created) { return std::unexpected(created.error()); }
    sampler = std::move(*created);
  }
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  Native::Texture &target = candidate ? *candidate : *Pages_;
  if (candidate && Pages_) {
    const auto copied = CopyHeightPageLayers(**commands, *Pages_, target);
    if (!copied) { return std::unexpected(copied.error()); }
  }
  const auto copied = UploadHeightPages(*Device_, **commands, target, pages, nodes);
  if (!copied) { return std::unexpected(copied.error()); }
  const auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  if (candidate) {
    PageLayers_ = candidate->Description().Layers;
    Pages_ = std::move(candidate);
  }
  if (sampler) { Nearest_ = std::move(sampler); }
  Spare_.resize(Spare_.size() - recycled);
  PagesInUse_.resize(made, 0);
  PagesMade_ = made;
  PagesLive_ += static_cast<uint32_t>(pages.size());
  for (size_t at = 0; at < pages.size(); ++at) {
    PagesInUse_[pages[at]] = 1;
    Patches_.Page(pages[at], nodes[at]);
    ++Generation_;
  }
  return pages;
}

Native::Result<PageId> TerrainResidency::PlacePage(std::span<const float> nodes) {
  const std::array batch{nodes};
  auto placed = PlacePages(batch);
  if (!placed) { return std::unexpected(placed.error()); }
  return placed->front();
}
}
