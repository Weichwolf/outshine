#include "BuildingImpostorSource.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <variant>

namespace outshine {
struct BuildingImpostorSource::MetadataPreparation::State {
  std::string Provenance;
  std::optional<Generators::PreparedBuildingAssets::BaseWrite> Migration;
  std::optional<Content::ImpostorViewAssets::MetadataWrite> Publication;
  uint64_t Preparations = 0;
};

BuildingImpostorSource::MetadataPreparation::MetadataPreparation() noexcept = default;
BuildingImpostorSource::MetadataPreparation::MetadataPreparation(MetadataPreparation &&) noexcept =
    default;
BuildingImpostorSource::MetadataPreparation &
BuildingImpostorSource::MetadataPreparation::operator=(MetadataPreparation &&) noexcept = default;
BuildingImpostorSource::MetadataPreparation::~MetadataPreparation() = default;

size_t BuildingImpostorSource::MetadataPreparation::PublicationBytes() const noexcept {
  if (!State_) { return 0; }
  const auto native = State_->Migration ? State_->Migration->PublicationBytes() : 0;
  return native + (State_->Publication ? State_->Publication->PublicationBytes() : 0);
}

uint64_t BuildingImpostorSource::MetadataPreparation::Preparations() const noexcept {
  return State_ ? State_->Preparations : 0;
}

std::expected<const Generators::BuildingSourceView *, std::string>
BuildingImpostorSource::ReadMetadataSource(MetadataPreparation &preparation,
                                           const std::stop_token &stop) {
  auto &migration = preparation.State_->Migration;
  const std::scoped_lock lock(Lock_);
  if (Source_ && !migration) { return &*Source_; }
  if (!Assets_) { return std::unexpected("building source inputs unavailable"); }
  std::atomic_bool stopping{stop.stop_requested()};
  const std::stop_callback observeStop(
      stop, [&stopping] { stopping.store(true, std::memory_order_relaxed); });
  auto base = Assets_->ReadBase(Recipe_.BaseKey, migration, stopping);
  if (!base) {
    const auto *kind = std::get_if<Generators::StructureBakeErrorKind>(&base.error());
    if (kind != nullptr && *kind == Generators::StructureBakeErrorKind::ArtifactBusy) {
      return nullptr;
    }
    return std::unexpected(stop.stop_requested() ? "building source metadata cancelled"
                                                 : "prepared building source unavailable");
  }
  if (!*base) {
    if (migration) { return nullptr; }
    return std::unexpected("prepared building source unavailable");
  }
  if (stop.stop_requested()) { return std::unexpected("building source metadata cancelled"); }
  return SourceFromBase(*base);
}

std::expected<void, std::string> BuildingImpostorSource::PrepareMetadata(
    AssetCache &cache, MetadataPreparation &preparation, const std::stop_token &stop) {
  auto &state = *preparation.State_;
  const auto source = ReadMetadataSource(preparation, stop);
  if (!source) {
    state.Migration.reset();
    return std::unexpected(source.error());
  }
  if (*source == nullptr) { return {}; }
  Box bounds;
  for (unsigned corner = 0; corner < 8; ++corner) {
    bounds.Cover(AnchorEcef_ + Recipe_.Frame.Origin +
                 Recipe_.Frame.SourceDirection((**source).Bounds().Corner(corner)));
  }
  if (stop.stop_requested()) {
    preparation = MetadataPreparation{};
    return std::unexpected("building source metadata cancelled");
  }
  auto prepared = Content::ImpostorViewAssets::PrepareMetadata(
      cache, Provenance_, Description(**source), bounds);
  if (!prepared) { return std::unexpected("building source metadata preparation failed"); }
  state.Publication = std::move(*prepared);
  ++state.Preparations;
  return {};
}

std::expected<std::optional<Content::ImpostorViewAssets>, std::string>
BuildingImpostorSource::ResolveMetadata(AssetCache &cache,
                                        MetadataPreparation &preparation,
                                        const std::stop_token &stop) {
  using Views = Content::ImpostorViewAssets;
  if (stop.stop_requested() || Provenance_.empty()) {
    preparation = MetadataPreparation{};
    return std::unexpected("invalid or cancelled building source metadata request");
  }
  if (preparation.State_ && preparation.State_->Provenance != Provenance_) {
    return std::unexpected("metadata preparation belongs to another building source");
  }
  auto opened = Views::Open(cache, Provenance_);
  if (!opened) {
    if (opened.error() == AssetCacheError::Busy) { return std::optional<Views>{}; }
    return std::unexpected("building source metadata lookup failed");
  }
  if (*opened && (**opened).Metadata().WorldFromField) {
    if (preparation.State_) {
      preparation.State_->Migration.reset();
      preparation.State_->Publication.reset();
    }
    return std::move(*opened);
  }
  if (!preparation.State_) {
    preparation.State_ = std::make_unique<MetadataPreparation::State>();
    preparation.State_->Provenance = Provenance_;
  }
  auto &state = *preparation.State_;
  if (!state.Publication) {
    const auto prepared = PrepareMetadata(cache, preparation, stop);
    if (!prepared) { return std::unexpected(prepared.error()); }
    return std::optional<Views>{};
  }
  if (stop.stop_requested()) {
    preparation = MetadataPreparation{};
    return std::unexpected("building source metadata cancelled");
  }
  auto published = Views::Publish(cache, *state.Publication);
  if (!published) {
    if (published.error() == AssetCacheError::Busy) { return std::optional<Views>{}; }
    state.Publication.reset();
    return std::unexpected("building source metadata publication failed");
  }
  state.Publication.reset();
  return std::move(*published);
}
}
