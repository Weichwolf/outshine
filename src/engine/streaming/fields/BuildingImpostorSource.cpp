#include "ImpostorViewAssets.h"
#include "BuildingImpostorSource.h"
#include "BuildingSourceRead.h"
#include "ImpostorBaker.h"
#include "ImpostorAtlasShape.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
BuildingImpostorSource::BuildingImpostorSource(
    Recipe recipe,
    std::shared_ptr<Generators::PreparedBuildingAssets> assets,
    std::shared_ptr<BuildingSourceRead> read)
    : Read_(std::move(read)),
      Recipe_(std::move(recipe)),
      Provenance_(Recipe_.Key()),
      Assets_(std::move(assets)) {}

std::expected<Generators::BuildingSourceView, std::string> BuildingImpostorSource::Source() const {
  if (Provenance_.empty() || (!Assets_ && !Read_) || (Read_ && Read_->Key() != Recipe_.BaseKey)) {
    return std::unexpected("building source inputs unavailable");
  }
  const auto base = Read_ ? Read_->Load() : Assets_->Load(Recipe_.BaseKey);
  if (!base || !*base) { return std::unexpected("prepared building source unavailable"); }
  auto source =
      Recipe_.Sources.empty()
          ? Generators::BuildingSourceView::Open(*base, Recipe_.Cells, Recipe_.Frame)
          : Generators::BuildingSourceView::OpenSources(*base, Recipe_.Sources, Recipe_.Frame);
  if (!source) { return std::unexpected(source.error().Message); }
  if (!source->Unsupported().empty()) {
    return std::unexpected("unsupported building source ordinal " +
                           std::to_string(source->Unsupported().front()) + " of " +
                           std::to_string(source->Unsupported().size()) + "; retain its parent");
  }
  return std::move(*source);
}

std::expected<GeometryAsset::Description, std::string>
BuildingImpostorSource::Layout(const Generators::BuildingSourceView &source) const {
  Mat4 worldFromField;
  for (size_t axis = 0; axis < 3; ++axis) {
    Vec3 unit;
    unit[axis] = 1;
    const Vec3 direction = Recipe_.Frame.SourceDirection(unit);
    for (size_t row = 0; row < 3; ++row) { worldFromField.At(row, axis) = direction[row]; }
  }
  worldFromField.SetTranslation(source.AnchorEcef() + Recipe_.Frame.Origin);
  auto coverage =
      SourceCoverage::FromSources(Recipe_.BaseKey, source.SourceCount(), source.Sources());
  if (!coverage) { return std::unexpected("invalid building geometry source coverage"); }
  return GeometryAsset::Description{.Bounds = source.Bounds(),
                                    .WorldFromLocal = worldFromField,
                                    .Coverage = std::move(*coverage)};
}

ImpostorViewAssets::Description
BuildingImpostorSource::Description(const GeometryAsset::Description &layout) const {
  Render::ImpostorCapture capture;
  capture.LeastM = layout.Bounds.Min;
  capture.MostM = layout.Bounds.Max;
  capture.ViewDirections = Recipe_.Directions;
  std::string error;
  const auto projection = Render::ImpostorBaker::ProjectionFor(
      capture,
      {.Pixels = static_cast<int>(Recipe_.Pixels),
       .Views = static_cast<unsigned>(Recipe_.Directions.size())},
      error);
  if (!projection) { return {}; }
  return {.Centre = projection->Centre,
          .HalfExtentM = projection->HalfExtentM,
          .Pixels = Recipe_.Pixels,
          .Directions = Recipe_.Directions,
          .WorldFromField = layout.WorldFromLocal};
}

std::expected<ImpostorViewAssets, std::string> BuildingImpostorSource::ResolveMetadata(
    AssetCache &cache, size_t geometryBytesMost, const std::stop_token &stop) {
  if (stop.stop_requested() || Provenance_.empty()) {
    return std::unexpected("invalid or cancelled building source metadata request");
  }
  auto opened = ImpostorViewAssets::Open(cache, Provenance_);
  if (!opened) { return std::unexpected("building source metadata lookup failed"); }
  if (*opened && (**opened).Metadata().WorldFromField) { return std::move(**opened); }
  auto layout =
      GeometryAsset::Describe(cache, Recipe_.GeometryKey(), Recipe_.BaseKey, geometryBytesMost);
  if (!layout) { return std::unexpected("building geometry metadata lookup failed"); }
  if (!*layout) {
    const auto source = Source();
    if (!source) { return std::unexpected(source.error()); }
    auto prepared = Layout(*source);
    if (!prepared) { return std::unexpected(prepared.error()); }
    *layout = std::move(*prepared);
  }
  const auto description = Description(**layout);
  const Box bounds = (**layout).Bounds.Through((**layout).WorldFromLocal);
  if (stop.stop_requested()) { return std::unexpected("building source metadata cancelled"); }
  auto published = ImpostorViewAssets::Publish(cache, Provenance_, description, bounds);
  if (!published) { return std::unexpected("building source metadata publication failed"); }
  return std::move(*published);
}

std::expected<GeometryAsset::Product, std::string> BuildingImpostorSource::GeometryFor(
    AssetCache &cache, Budget budget, const std::stop_token &stop) const {
  const auto key = Recipe_.GeometryKey();
  auto cached = GeometryAsset::Load(cache, key, Recipe_.BaseKey, budget.GeometryBytesMost);
  if (!cached) { return std::unexpected("building geometry asset lookup failed"); }
  if (*cached) { return std::move(**cached); }
  const auto source = Source();
  if (!source) { return std::unexpected(source.error()); }
  auto originals = source->Originals(budget.Capture, Recipe_.WallRoofMetreMaterials, stop);
  if (!originals) { return std::unexpected(originals.error().Message); }
  if (stop.stop_requested()) { return std::unexpected("building geometry generation cancelled"); }
  auto layout = Layout(*source);
  if (!layout) { return std::unexpected(layout.error()); }
  GeometryAsset::Product product{.Layout = std::move(*layout), .Mesh = std::move(*originals)};
  const auto stored = GeometryAsset::Publish(
      cache, key, Recipe_.BaseKey, product.Layout, product.Mesh, budget.GeometryBytesMost);
  if (!stored) { return std::unexpected("building geometry asset publication failed"); }
  return product;
}

std::expected<ImpostorAtlas, std::string>
BuildingImpostorSource::Generate(AssetCache &cache,
                                 ImpostorViewAssets::ViewIndex view,
                                 Budget budget,
                                 const std::stop_token &stop) {
  thread_local Render::ImpostorBaker captures;
  return Generate(captures, cache, view, budget, stop);
}

std::expected<ImpostorAtlas, std::string>
BuildingImpostorSource::Generate(Render::ImpostorBaker &captures,
                                 AssetCache &cache,
                                 ImpostorViewAssets::ViewIndex view,
                                 Budget budget,
                                 const std::stop_token &stop) {
  const auto bound =
      ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(Recipe_.Pixels), .Views = 1});
  if (stop.stop_requested() || Provenance_.empty() || view.Value >= Recipe_.Directions.size() ||
      !bound || *bound > budget.FieldBytesMost || budget.GeometryBytesMost == 0 ||
      budget.Capture.Samples < static_cast<uint64_t>(Recipe_.Pixels) * Recipe_.Pixels ||
      budget.Capture.Sources == 0 || budget.Capture.Faces == 0 ||
      budget.Capture.TriangleAttempts == 0) {
    return std::unexpected("building field request exceeds valid admission");
  }
  auto geometry = GeometryFor(cache, budget, stop);
  if (!geometry) { return std::unexpected(geometry.error()); }
  const auto description = Description(geometry->Layout);
  Render::ImpostorCapture capture;
  capture.LeastM = geometry->Layout.Bounds.Min;
  capture.MostM = geometry->Layout.Bounds.Max;
  capture.HalfExtentM = description.HalfExtentM;
  capture.Sources.push_back({.Mesh = std::move(geometry->Mesh), .Instances = {Mat4{}}});
  if (stop.stop_requested()) { return std::unexpected("building field capture cancelled"); }
  capture.ViewDirections = {Recipe_.Directions[view.Value]};
  std::string error;
  const ImpostorAtlasShape shape{.Pixels = static_cast<int>(Recipe_.Pixels), .Views = 1};
  auto field = captures.Capture(std::move(capture), shape, error, stop);
  if (!field) { return std::unexpected(std::move(error)); }
  if (stop.stop_requested()) { return std::unexpected("building field capture cancelled"); }
  return std::move(*field);
}
}
