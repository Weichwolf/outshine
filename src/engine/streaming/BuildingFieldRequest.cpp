#include "BuildingFieldRequest.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "ImpostorMetadataRequest.h"
#include "ImpostorFieldLoad.h"
#include "ImpostorViewJob.h"
#include "TileGeodesy.h"
#include "Geodesy.h"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
BuildingImpostorSource::Recipe RecipeOf(const BuildingSourceDemand::Source &source,
                                        const BuildingFieldRequest::Demand &demand) {
  const auto &anchor = source.AnchorEcef;
  const auto geo = Ground::EcefToGeoWgs84({.X = anchor[0], .Y = anchor[1], .Z = anchor[2]});
  BuildingImpostorSource::Recipe recipe{
      .BaseKey = source.BaseKey,
      .Cells = {},
      .Frame = {.Axes =
                    EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}),
                .Origin = {}},
      .Directions = demand.Directions,
      .WallRoofMetreMaterials = Generators::BuildingSurfaceMaterials(),
      .Pixels = demand.Pixels};
  uint64_t cells = source.Cells;
  while (cells != 0) {
    recipe.Cells.push_back(static_cast<uint32_t>(std::countr_zero(cells)) + 1u);
    cells &= cells - 1u;
  }
  return recipe;
}
}

struct BuildingFieldRequest::State {
  AssetPipeline *Pipeline;
  Tasks *Workers;
  std::shared_ptr<AssetCache> Cache;
  AssetPipeline::Consumer Consumer;
  std::shared_ptr<BuildingImpostorSource> Producer;
  std::optional<ImpostorMetadataRequest> Metadata;
  std::optional<AssetPipeline::RequestTicket> Ticket;
  std::optional<ImpostorFieldLoad> Load;
  Product Value;
  Content::ImpostorViewAssets::ViewIndex View;
  Budget Limits;
  bool Cancelled = false;
  std::string Error;

  State(AssetPipeline &pipeline,
        Tasks &workers,
        std::shared_ptr<AssetCache> cache,
        AssetPipeline::Consumer consumer,
        std::shared_ptr<BuildingImpostorSource> producer,
        BuildingSourceDemand::Source source,
        Content::ImpostorViewAssets::ViewIndex view,
        Budget budget)
      : Pipeline(&pipeline),
        Workers(&workers),
        Cache(std::move(cache)),
        Consumer(std::move(consumer)),
        Producer(std::move(producer)),
        Value{.Source = std::move(source), .Metadata = {}, .Field = {}},
        View(view),
        Limits(budget) {}

  std::expected<bool, std::string> PollMetadata() {
    if (!Metadata) { return std::unexpected("building field metadata request unavailable"); }
    const auto ready = Metadata->Poll();
    if (!ready) { return std::unexpected(ready.error()); }
    if (!*ready) { return false; }
    Value.Metadata = *ready;
    const auto &frame = Value.Metadata->Metadata().WorldFromField;
    if (!frame || frame->Translation() != Value.Source.AnchorEcef) {
      return std::unexpected("building field metadata has another World frame");
    }
    return true;
  }

  std::expected<bool, std::string> RequestField() {
    const auto key = Value.Metadata->FieldKey(View);
    const auto parsed = AssetProductKey::Parse(key);
    if (!parsed) { return std::unexpected("building field has no valid product address"); }
    ImpostorViewJob job{.Cache = Cache,
                        .Provenance = Producer->Provenance(),
                        .ProductKey = key,
                        .BytesMost = Limits.FieldBytesMost,
                        .Product = ImpostorViewJob::MaterialView{
                            .Selected = View,
                            .Generate = [source = Producer, budget = Limits.Capture](
                                            Content::ImpostorViewAssets::ViewIndex view,
                                            size_t bytes,
                                            const std::stop_token &stop) {
                              return source->Generate(view, bytes, budget, stop);
                            }}};
    auto requested = Pipeline->RequestResolved(
        Consumer,
        {.Key = *parsed,
         .Generation = Consumer.Generation(),
         .InputBytes = Limits.ProducerBytes,
         .ConsumerBytes = Limits.ResidentBytes,
         .ReceiptBytesMost = Content::ImpostorViewAssets::MetadataBytesMost},
        std::move(job));
    if (!requested) {
      if (requested.error() == AssetPublicationError::ResourceDeferred) { return false; }
      return std::unexpected("building field publication admission failed");
    }
    Ticket.emplace(std::move(*requested));
    return true;
  }

  std::expected<std::shared_ptr<const ImpostorAtlas>, std::string> PollField() {
    if (!Load) {
      if (!Ticket) { return std::unexpected("building field publication unavailable"); }
      const auto publication = Ticket->Poll();
      if (!publication.Finished) { return nullptr; }
      if (publication.Status != AssetPublicationStatus::Ready) {
        return std::unexpected(publication.Message);
      }
      auto loaded = ImpostorFieldLoad::Start(
          *Workers, Cache, Producer->Provenance(), View, Limits.FieldBytesMost, std::move(*Ticket));
      if (!loaded) { return std::unexpected(loaded.error()); }
      Ticket.reset();
      Load.emplace(std::move(*loaded));
    }
    return Load->Poll();
  }
};

BuildingFieldRequest::BuildingFieldRequest(std::shared_ptr<State> state) noexcept
    : State_(std::move(state)) {}

BuildingFieldRequest::BuildingFieldRequest(BuildingFieldRequest &&) noexcept = default;

BuildingFieldRequest &BuildingFieldRequest::operator=(BuildingFieldRequest &&other) noexcept {
  if (this != &other) {
    Cancel();
    State_ = std::move(other.State_);
  }
  return *this;
}

BuildingFieldRequest::~BuildingFieldRequest() {
  Cancel();
}

std::expected<BuildingFieldRequest, AssetPublicationError>
BuildingFieldRequest::Start(AssetPipeline &pipeline,
                            Tasks &workers,
                            const std::shared_ptr<AssetCache> &cache,
                            std::shared_ptr<Generators::PreparedBuildingAssets> sources,
                            BuildingSourceDemand::Source source,
                            const Demand &demand,
                            Budget budget) {
  const auto bytes =
      ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(demand.Pixels), .Views = 1});
  if (!cache || source.Bounds.Empty() || source.Cells == 0 || !bytes ||
      *bytes > budget.FieldBytesMost || budget.ProducerBytes == 0 || budget.ResidentBytes == 0 ||
      demand.View.Value >= demand.Directions.size() ||
      budget.Capture.Samples < static_cast<uint64_t>(demand.Pixels) * demand.Pixels ||
      budget.Capture.SourceTests == 0 || !std::isfinite(Length(source.AnchorEcef)) ||
      Length(source.AnchorEcef) == 0) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  auto recipe = RecipeOf(source, demand);
  if (recipe.Key().empty()) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) { return std::unexpected(consumer.error()); }
  const auto producer =
      std::make_shared<BuildingImpostorSource>(std::move(recipe), std::move(sources));
  auto state = std::make_shared<State>(pipeline,
                                       workers,
                                       cache,
                                       std::move(*consumer),
                                       producer,
                                       std::move(source),
                                       demand.View,
                                       budget);
  const auto preparation = std::make_shared<BuildingImpostorSource::MetadataPreparation>();
  auto metadata = ImpostorMetadataRequest::Start(
      pipeline,
      state->Consumer,
      workers,
      cache,
      producer->Provenance(),
      [cache, producer, preparation](const std::stop_token &stop) {
        return producer->ResolveMetadata(*cache, *preparation, stop);
      },
      budget.ProducerBytes);
  if (!metadata) { return std::unexpected(metadata.error()); }
  state->Metadata.emplace(std::move(*metadata));
  return BuildingFieldRequest(std::move(state));
}

std::expected<std::shared_ptr<const BuildingFieldRequest::Product>, std::string>
BuildingFieldRequest::Poll() {
  if (!State_) { return std::unexpected("building field request moved or cancelled"); }
  auto &state = *State_;
  if (state.Cancelled) { return std::unexpected("building field request cancelled"); }
  if (!state.Error.empty()) { return std::unexpected(state.Error); }
  if (state.Value.Field) { return std::shared_ptr<const Product>(State_, &state.Value); }
  if (!state.Value.Metadata) {
    const auto ready = state.PollMetadata();
    if (!ready) { return std::unexpected(state.Error = ready.error()); }
    if (!*ready) { return nullptr; }
  }
  if (!state.Ticket && !state.Load) {
    const auto admitted = state.RequestField();
    if (!admitted) { return std::unexpected(state.Error = admitted.error()); }
    if (!*admitted) { return nullptr; }
  }
  auto field = state.PollField();
  if (!field) { return std::unexpected(state.Error = field.error()); }
  if (!*field) { return nullptr; }
  state.Value.Field = std::move(*field);
  state.Producer.reset();
  return std::shared_ptr<const Product>(State_, &state.Value);
}

void BuildingFieldRequest::Cancel() noexcept {
  if (!State_) { return; }
  State_->Cancelled = true;
  if (State_->Metadata) { State_->Metadata->Cancel(); }
  if (State_->Ticket) { State_->Ticket->Cancel(); }
  if (State_->Load) { State_->Load->Cancel(); }
  State_->Producer.reset();
}
}
