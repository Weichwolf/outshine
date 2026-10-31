#include "BuildingFieldRequest.h"
#include "BuildingImpostorSource.h"
#include "BuildingMaterials.h"
#include "ImpostorMetadataRequest.h"
#include "ImpostorFieldLoad.h"
#include "ImpostorViewJob.h"
#include "TileGeodesy.h"
#include "Geodesy.h"
#include "MaterialFieldInput.h"
#include "MaterialFieldRasterInput.h"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
Generators::BuildingCaptureFrame FrameOf(const Vec3 &anchor) {
  const auto geo = Ground::EcefToGeoWgs84({.X = anchor[0], .Y = anchor[1], .Z = anchor[2]});
  return {.Axes = EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}),
          .Origin = {}};
}

BuildingImpostorSource::Recipe RecipeOf(const BuildingSourceDemand::Source &source,
                                        const BuildingFieldRequest::Demand &demand) {
  BuildingImpostorSource::Recipe recipe{.BaseKey = source.BaseKey,
                                        .Cells = {},
                                        .Frame = FrameOf(source.AnchorEcef),
                                        .Directions = demand.Directions,
                                        .WallRoofMetreMaterials =
                                            Generators::BuildingSurfaceMaterials(),
                                        .Pixels = demand.Pixels};
  uint64_t cells = source.Cells;
  while (cells != 0) {
    recipe.Cells.push_back(static_cast<uint32_t>(std::countr_zero(cells)) + 1u);
    cells &= cells - 1u;
  }
  return recipe;
}

bool Matches(const Content::ImpostorViewAssets::Description &metadata,
             const BuildingSourceDemand::Source &source,
             const BuildingFieldRequest::Demand &demand) {
  if (metadata.Pixels != demand.Pixels || metadata.Directions != demand.Directions ||
      !metadata.WorldFromField) {
    return false;
  }
  const auto frame = FrameOf(source.AnchorEcef);
  Mat4 expected;
  for (size_t axis = 0; axis < 3; ++axis) {
    Vec3 unit;
    unit[axis] = 1;
    const auto direction = frame.SourceDirection(unit);
    for (size_t row = 0; row < 3; ++row) { expected.At(row, axis) = direction[row]; }
  }
  expected.SetTranslation(source.AnchorEcef);
  return *metadata.WorldFromField == expected;
}
}

struct BuildingFieldRequest::State {
  struct PreparedRaster {
    std::shared_ptr<const ImpostorAtlas> Field;
    std::shared_ptr<const Render::MaterialFieldInput> Input;
  };

  AssetPipeline *Pipeline;
  Tasks *Workers;
  std::shared_ptr<AssetCache> Cache;
  AssetPipeline::Consumer Consumer;
  std::shared_ptr<BuildingImpostorSource> Producer;
  std::optional<ImpostorMetadataRequest> Metadata;
  std::optional<AssetPipeline::RequestTicket> Ticket;
  std::optional<ImpostorFieldLoad> Load;
  std::shared_ptr<PreparedRaster> Raster = std::make_shared<PreparedRaster>();
  Product Value;
  Demand Requested;
  Budget Limits;
  bool Cancelled = false;
  std::string Error;

  State(AssetPipeline &pipeline,
        Tasks &workers,
        std::shared_ptr<AssetCache> cache,
        AssetPipeline::Consumer consumer,
        std::shared_ptr<BuildingImpostorSource> producer,
        BuildingSourceDemand::Source source,
        Demand demand,
        Budget budget)
      : Pipeline(&pipeline),
        Workers(&workers),
        Cache(std::move(cache)),
        Consumer(std::move(consumer)),
        Producer(std::move(producer)),
        Value{.Source = std::move(source), .Metadata = {}, .Field = {}, .RenderInput = {}},
        Requested(std::move(demand)),
        Limits(budget) {}

  std::expected<bool, std::string> PollMetadata() {
    if (!Metadata) { return std::unexpected("building field metadata request unavailable"); }
    const auto ready = Metadata->Poll();
    if (!ready) { return std::unexpected(ready.error()); }
    if (!*ready) { return false; }
    Value.Metadata = *ready;
    const auto &worldFromField = Value.Metadata->Metadata().WorldFromField;
    if (!worldFromField || !Matches(Value.Metadata->Metadata(), Value.Source, Requested)) {
      return std::unexpected(
          "building field metadata does not match its requested capture binding");
    }
    if (Requested.GeometricView) {
      const auto &metadata = Value.Metadata->Metadata();
      const auto hull = Render::ImpostorPoseBounds::From(
          metadata.Centre, metadata.Directions[Requested.View.Value], metadata.HalfExtentM);
      if (!hull) { return std::unexpected(hull.error()); }
      const auto &pose = *Requested.GeometricView;
      const auto error = pose.Space == CoordinateSpace::World
                             ? hull->Project(pose.Camera, pose.Optics, *worldFromField)
                             : hull->Project(pose.Camera, pose.Optics);
      if (!error) { return std::unexpected(error.error()); }
      if (!error->DepthInside || error->L1UpperPx > pose.GeometryErrorMostPx) {
        return std::unexpected("building field pose exceeds its geometric domain or error budget");
      }
      Value.GeometricError = *error;
    }
    return true;
  }

  std::expected<bool, std::string> RequestField() {
    const auto key = Value.Metadata->FieldKey(Requested.View);
    const auto parsed = AssetProductKey::Parse(key);
    if (!parsed) { return std::unexpected("building field has no valid product address"); }
    ImpostorViewJob job{.Cache = Cache,
                        .Provenance = Producer->Provenance(),
                        .ProductKey = key,
                        .BytesMost = Limits.FieldBytesMost,
                        .Product = ImpostorViewJob::MaterialView{
                            .Selected = Requested.View,
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
         .ConsumerBytes = Limits.ResidentBytes + Limits.RasterBytesMost,
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
          *Workers,
          Cache,
          Producer->Provenance(),
          Requested.View,
          Limits.FieldBytesMost,
          std::move(*Ticket),
          [raster = Raster, bytes = Limits.RasterBytesMost](
              const ImpostorAtlas &field) -> std::expected<void, std::string> {
            auto input = Render::MaterialFieldInput::Prepare(field, 0, bytes);
            if (!input) { return std::unexpected(input.error()); }
            raster->Input = std::move(*input);
            return {};
          });
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
      budget.RasterBytesMost > std::numeric_limits<size_t>::max() - budget.ResidentBytes ||
      budget.RasterBytesMost / (2 * sizeof(Render::MaterialFieldRasterInput::Word)) <
          static_cast<uint64_t>(demand.Pixels) * demand.Pixels ||
      demand.View.Value >= demand.Directions.size() ||
      budget.Capture.Samples < static_cast<uint64_t>(demand.Pixels) * demand.Pixels ||
      budget.Capture.Sources == 0 || budget.Capture.Faces == 0 ||
      budget.Capture.TriangleAttempts == 0 || !std::isfinite(Length(source.AnchorEcef)) ||
      Length(source.AnchorEcef) == 0) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  if (demand.GeometricView && (!std::isfinite(demand.GeometricView->GeometryErrorMostPx) ||
                               demand.GeometricView->GeometryErrorMostPx < 0 ||
                               (demand.GeometricView->Space != CoordinateSpace::CaptureLocal &&
                                demand.GeometricView->Space != CoordinateSpace::World))) {
    return std::unexpected(AssetPublicationError::InvalidRequest);
  }
  auto recipe = RecipeOf(source, demand);
  if (recipe.Key().empty()) { return std::unexpected(AssetPublicationError::InvalidRequest); }
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) { return std::unexpected(consumer.error()); }
  const auto producer =
      std::make_shared<BuildingImpostorSource>(std::move(recipe), std::move(sources));
  auto state = std::make_shared<State>(
      pipeline, workers, cache, std::move(*consumer), producer, std::move(source), demand, budget);
  auto metadata = ImpostorMetadataRequest::Start(
      pipeline,
      state->Consumer,
      workers,
      cache,
      producer->Provenance(),
      [cache, producer](const std::stop_token &stop) {
        return producer->ResolveMetadata(*cache, stop);
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
  state.Raster->Field = std::move(*field);
  state.Value.Field = state.Raster->Field;
  state.Value.RenderInput =
      std::shared_ptr<const Render::MaterialFieldInput>(state.Raster, state.Raster->Input.get());
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
