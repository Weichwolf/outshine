#include "BuildingImpostorSource.h"
#include "ImpostorBaker.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
BuildingImpostorSource::BuildingImpostorSource(
    Recipe recipe, std::shared_ptr<Generators::PreparedBuildingAssets> assets)
    : Recipe_(std::move(recipe)), Provenance_(Recipe_.Key()), Assets_(std::move(assets)) {}

std::expected<const Generators::BuildingSourceView *, std::string>
BuildingImpostorSource::Source() {
  const std::scoped_lock lock(Lock_);
  if (Source_) { return &*Source_; }
  if (Provenance_.empty() || !Assets_) {
    return std::unexpected("building source inputs unavailable");
  }
  const auto base = Assets_->Load(Recipe_.BaseKey);
  if (!base || !*base) { return std::unexpected("prepared building source unavailable"); }
  auto source = Generators::BuildingSourceView::Open(*base, Recipe_.Cells, Recipe_.Frame);
  if (!source) { return std::unexpected(source.error().Message); }
  if (!source->Unsupported().empty()) {
    return std::unexpected("unsupported building source ordinal " +
                           std::to_string(source->Unsupported().front()) + " of " +
                           std::to_string(source->Unsupported().size()) + "; retain its parent");
  }
  AnchorEcef_ = (*base)->AnchorEcef;
  return &Source_.emplace(std::move(*source));
}

Content::ImpostorViewAssets::Description
BuildingImpostorSource::Description(const Generators::BuildingSourceView &source) const {
  Render::ImpostorCapture capture;
  capture.LeastM = source.Bounds().Min;
  capture.MostM = source.Bounds().Max;
  capture.ViewDirections = Recipe_.Directions;
  std::string error;
  const auto projection = Render::ImpostorBaker::ProjectionFor(
      capture,
      {.Pixels = static_cast<int>(Recipe_.Pixels),
       .Views = static_cast<unsigned>(Recipe_.Directions.size())},
      error);
  if (!projection) { return {}; }
  Mat4 worldFromField;
  for (size_t axis = 0; axis < 3; ++axis) {
    Vec3 unit;
    unit[axis] = 1;
    const Vec3 direction = Recipe_.Frame.SourceDirection(unit);
    for (size_t row = 0; row < 3; ++row) { worldFromField.At(row, axis) = direction[row]; }
  }
  worldFromField.SetTranslation(AnchorEcef_ + Recipe_.Frame.Origin);
  return {.Centre = projection->Centre,
          .HalfExtentM = projection->HalfExtentM,
          .Pixels = Recipe_.Pixels,
          .Directions = Recipe_.Directions,
          .WorldFromField = worldFromField};
}

std::expected<Content::ImpostorViewAssets, std::string>
BuildingImpostorSource::ResolveMetadata(AssetCache &cache, const std::stop_token &stop) {
  if (stop.stop_requested() || Provenance_.empty()) {
    return std::unexpected("invalid or cancelled building source metadata request");
  }
  auto opened = Content::ImpostorViewAssets::Open(cache, Provenance_);
  if (!opened) { return std::unexpected("building source metadata lookup failed"); }
  if (*opened && (**opened).Metadata().WorldFromField) { return std::move(**opened); }
  const auto source = Source();
  if (!source) { return std::unexpected(source.error()); }
  const auto description = Description(**source);
  Box bounds;
  for (unsigned corner = 0; corner < 8; ++corner) {
    bounds.Cover(AnchorEcef_ + Recipe_.Frame.Origin +
                 Recipe_.Frame.SourceDirection((**source).Bounds().Corner(corner)));
  }
  if (stop.stop_requested()) { return std::unexpected("building source metadata cancelled"); }
  auto published = Content::ImpostorViewAssets::Publish(cache, Provenance_, description, bounds);
  if (!published) { return std::unexpected("building source metadata publication failed"); }
  return std::move(*published);
}

std::expected<ImpostorAtlas, std::string>
BuildingImpostorSource::Generate(Content::ImpostorViewAssets::ViewIndex view,
                                 size_t bytesMost,
                                 Generators::BuildingSourceBudget budget,
                                 const std::stop_token &stop) {
  const auto bound =
      ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(Recipe_.Pixels), .Views = 1});
  if (stop.stop_requested() || Provenance_.empty() || view.Value >= Recipe_.Directions.size() ||
      !bound || *bound > bytesMost ||
      budget.Samples < static_cast<uint64_t>(Recipe_.Pixels) * Recipe_.Pixels ||
      budget.Sources == 0 || budget.Faces == 0 || budget.TriangleAttempts == 0) {
    return std::unexpected("building field request exceeds valid admission");
  }
  const auto source = Source();
  if (!source) { return std::unexpected(source.error()); }
  const auto description = Description(**source);
  auto originals = (**source).Originals(budget, Recipe_.WallRoofMetreMaterials, stop);
  if (!originals) { return std::unexpected(originals.error().Message); }
  if (stop.stop_requested()) { return std::unexpected("building field capture cancelled"); }
  Render::ImpostorCapture capture;
  capture.LeastM = (**source).Bounds().Min;
  capture.MostM = (**source).Bounds().Max;
  capture.HalfExtentM = description.HalfExtentM;
  capture.ViewDirections = {Recipe_.Directions[view.Value]};
  capture.Sources.push_back({.Mesh = std::move(*originals), .Instances = {Mat4{}}});
  std::string error;
  auto field = Render::ImpostorBaker::Bake(
      std::move(capture), {.Pixels = static_cast<int>(Recipe_.Pixels), .Views = 1}, error, stop);
  if (!field) { return std::unexpected(std::move(error)); }
  if (stop.stop_requested()) { return std::unexpected("building field capture cancelled"); }
  return std::move(*field);
}
}
