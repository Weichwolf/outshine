#include "TerrainClipmapPreparation.h"
#include "TerrainClipmapRequest.h"
#include "ClipmapSurface.h"
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <format>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace outshine {
namespace {
size_t Missing(const Render::ClipmapHeightCache &cache) {
  size_t count = 0;
  for (uint32_t level = 0; level < cache.Grid().Levels; ++level) {
    count += static_cast<size_t>(
        std::ranges::count_if(cache.Values(level), [](float value) { return std::isnan(value); }));
  }
  return count;
}

std::expected<void, std::string> Collect(TerrainClipmapRequest &request,
                                         std::optional<Render::ClipmapHeightUpdate> &prepared) {
  if (prepared) { return {}; }
  auto ready = request.Take();
  if (!ready) { return {}; }
  if (!*ready) { return std::unexpected(ready->error()); }
  prepared.emplace(std::move(**ready));
  return {};
}
}

TerrainClipmapPreparation::TerrainClipmapPreparation(
    Tasks &worker,
    std::shared_ptr<const TerrainSurfacePublication> source,
    std::shared_ptr<Render::ClipmapSurface> surface)
    : Worker_(&worker), Source_(std::move(source)), Surface_(std::move(surface)) {}

TerrainClipmapPreparation::~TerrainClipmapPreparation() = default;

std::expected<void, std::string>
TerrainClipmapPreparation::Advance(Vec2 camera, TerrainClipmapAdmission admission) {
  if (Heights_ || Normals_) {
    const auto heightReady = Collect(*Heights_, PreparedHeights_);
    const auto normalReady = Collect(*Normals_, PreparedNormals_);
    if (!heightReady) { return std::unexpected(heightReady.error()); }
    if (!normalReady) { return std::unexpected(normalReady.error()); }
    if (!PreparedHeights_ || !PreparedNormals_) { return {}; }
    const auto published =
        Surface_->Publish(*PreparedHeights_, *PreparedNormals_, admission.StagingBytesMost);
    if (!published) {
      if (published.error().Kind == Render::Native::ErrorKind::Busy) { return {}; }
      return std::unexpected(published.error().Message);
    }
    Heights_.reset();
    Normals_.reset();
    PreparedHeights_.reset();
    PreparedNormals_.reset();
  }
  auto heights = Surface_->Heights().Plan(camera, admission.WorkBytesMost);
  if (!heights) {
    return std::unexpected("clipmap height work exceeds admission or coordinate range");
  }
  auto normals =
      Surface_->NormalHeights().Plan(camera, admission.WorkBytesMost - heights->HeapBytes());
  if (!normals) {
    return std::unexpected("clipmap normal work exceeds admission or coordinate range");
  }
  if (heights->Samples().empty() && normals->Samples().empty()) {
    if (!Surface_->Complete()) {
      return std::unexpected(
          std::format("clipmap source coverage incomplete: geometry={}, normals={}",
                      Missing(Surface_->Heights()),
                      Missing(Surface_->NormalHeights())));
    }
    return {};
  }
  Heights_ = std::make_unique<TerrainClipmapRequest>(
      *Worker_,
      Source_,
      std::move(*heights),
      TerrainClipmapSampling{.SpacingM = Surface_->Heights().Grid().CellM,
                             .SamplesPerSlice = admission.SamplesPerSlice});
  Normals_ = std::make_unique<TerrainClipmapRequest>(
      *Worker_,
      Source_,
      std::move(*normals),
      TerrainClipmapSampling{.SpacingM = Surface_->NormalHeights().Grid().CellM,
                             .SamplesPerSlice = admission.SamplesPerSlice});
  return {};
}

bool TerrainClipmapPreparation::Ready() const noexcept {
  return !Heights_ && !Normals_ && Surface_->Complete();
}

size_t TerrainClipmapPreparation::HeapBytes() const noexcept {
  return (Heights_ ? Heights_->HeapBytes() : 0) + (Normals_ ? Normals_->HeapBytes() : 0) +
         (PreparedHeights_ ? PreparedHeights_->HeapBytes() : 0) +
         (PreparedNormals_ ? PreparedNormals_->HeapBytes() : 0);
}
}
