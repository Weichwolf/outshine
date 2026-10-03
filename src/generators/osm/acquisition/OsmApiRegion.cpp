#include "OsmApiRegion.h"
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "SourceConfiguration.h"
#include "OsmProvider.h"

namespace outshine::Generators::Osm {
namespace {
std::expected<std::unique_ptr<Data::Source>, std::string>
ConfiguredSource(const Data::SourceProvider &provider,
                 std::string_view root,
                 const Data::ProviderRegistry *registry) {
  if (registry != nullptr) { return Data::ConfigureSource(provider, root, *registry); }
  Data::ProviderRegistry defaults;
  static const Provider original;
  if (!defaults.registerProvider(original)) {
    return std::unexpected("original OSM provider registration failed");
  }
  return Data::ConfigureSource(provider, root, defaults);
}
}

ApiRegion::ApiRegion(Data::ContentStore &store, Data::Transport &wire, Data::Address at)
    : Sources_(store), Wire_(wire), Address_(at) {}

ApiRegion::~ApiRegion() {
  if (Query_) { Data::SourceSet::Abandon(*Query_, Wire_); }
}

std::expected<std::unique_ptr<ApiRegion>, std::string>
ApiRegion::Create(const Data::SourceProvider &provider,
                  Data::ContentStore &store,
                  Data::Transport &wire,
                  const Data::ProviderRegistry *registry,
                  std::string_view shippedRoot,
                  std::optional<Data::GeoCellId> cell) {
  auto source = ConfiguredSource(provider, shippedRoot, registry);
  if (!source) { return std::unexpected(std::move(source.error())); }
  if ((*source)->Declaration().Kind != Data::DataKind::OriginalOsm ||
      (*source)->Declaration().Wire != Data::WireFormat::OsmXml) {
    return std::unexpected("an original OSM provider must supply original OSM XML");
  }
  if (!cell && (*source)->Declaration().How != Data::Scheme::WholeWorld) {
    return std::unexpected("an original OSM catalogue requires geographic cell demand");
  }
  const auto at = cell ? Data::Address::AtGeoCell(*cell) : Data::Address::Whole(0);
  if (cell &&
      ((*source)->Declaration().How != Data::Scheme::GeodeticGrid ||
       (*source)->Covers(Data::Fetch(Data::DataKind::OriginalOsm, at)) != Data::Coverage::Inside ||
       (*source)->Serves(Data::Fetch(Data::DataKind::OriginalOsm, at)) != at)) {
    return std::unexpected("an original OSM catalogue refused geographic cell demand");
  }
  auto region = std::unique_ptr<ApiRegion>(new ApiRegion(store, wire, at));
  if (region->Sources_.Add(std::move(*source)) != Data::SourceSet::Registration::Accepted) {
    return std::unexpected("original OSM source registration failed");
  }
  region->Sources_.Seal();
  return region;
}

SourceChunk ApiRegion::Chunk(const Data::SourceProvider &provider,
                             std::optional<Data::GeoCellId> cell) const {
  const auto &declaration = Sources_.At(0).Declaration();
  SourceChunk chunk{.Provider = provider, .Xml = {}, .Origin = declaration.Endpoint, .Cell = cell};
  if (cell) {
    chunk.Provider.Dataset = declaration.Id;
    chunk.Provider.Revision = declaration.Revision;
    chunk.Provider.PayloadSha256 = declaration.PayloadSha256;
    chunk.Provider.Coverage = cell->Bounds();
    chunk.Origin += "/" + Address_.Text();
  }
  return chunk;
}

std::expected<ApiRegion::Collected, std::string> ApiRegion::Collect(SourceChunk &chunk,
                                                                    double beganMs) {
  if (!Query_) { return Collected::Pending; }
  auto delivery = Sources_.Collect(*Query_, Wire_);
  if (auto answer = delivery.Take()) {
    chunk.Xml.assign(answer->Bytes.begin(), answer->Bytes.end());
    chunk.ReadMs = Wire_.NowMs() - beganMs;
    chunk.FromStore = Sources_.Counters().FromStore != 0;
    Query_.reset();
    return Collected::Ready;
  }
  if (delivery.Where() == Data::Delivery::State::Pending) { return Collected::Pending; }
  const auto reason =
      delivery.Failure() ? delivery.Failure()->Reason : Data::FetchFailureReason::ProviderRefused;
  if (chunk.Cell && reason == Data::FetchFailureReason::CapacityRefused) {
    Query_.reset();
    return Collected::Refine;
  }
  std::string context;
  if (const auto &failure = delivery.Failure()) {
    if (failure->HttpStatus) { context = " HTTP " + std::to_string(*failure->HttpStatus); }
    context += " after " + std::to_string(failure->Retries) + " retries";
  }
  return std::unexpected("original OSM source '" + chunk.Origin +
                         "' failed: " + std::string(Data::Name(reason)) + context);
}

bool ApiRegion::HasCachedCoverage(Data::ContentStore &store) const {
  const auto cell = Address_.GeoCell();
  if (!cell) { return false; }
  const auto &declaration = Sources_.At(0).Declaration();
  return store.LookupCell(declaration, *cell).Where == Data::ContentStore::Presence::Bytes ||
         store.HasCompleteChildCoverage(declaration, *cell);
}

bool ApiRegion::Begin(const Data::ContentStore &store, std::vector<Data::GeoCellId> &refine) {
  const auto cell = Address_.GeoCell();
  if (cell && store.CanResumeFromChildren(Sources_.At(0).Declaration(), *cell)) {
    refine.push_back(*cell);
    return true;
  }
  Query_.emplace(Sources_.Ask(Data::Fetch(Data::DataKind::OriginalOsm, Address_)));
  return false;
}

}
