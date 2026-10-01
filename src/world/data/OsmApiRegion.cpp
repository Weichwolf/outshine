#include "OsmApiRegion.h"
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "DeclaredSources.h"

namespace outshine::Data {
OsmApiRegion::OsmApiRegion(ContentStore &store, Transport &wire, Address at)
    : Sources_(store), Wire_(wire), Address_(at) {}

OsmApiRegion::~OsmApiRegion() {
  if (Query_) { SourceSet::Abandon(*Query_, Wire_); }
}

std::expected<std::unique_ptr<OsmApiRegion>, std::string>
OsmApiRegion::Create(const SourceProvider &provider,
                     ContentStore &store,
                     Transport &wire,
                     const ProviderRegistry *registry,
                     std::string_view shippedRoot,
                     std::optional<GeoCellId> cell) {
  auto source = MakeDeclaredSource(provider, shippedRoot, registry);
  if (!source) { return std::unexpected(std::move(source.error())); }
  if ((*source)->Declaration().Kind != DataKind::OriginalOsm ||
      (*source)->Declaration().Wire != WireFormat::OsmXml) {
    return std::unexpected("an original OSM provider must supply original OSM XML");
  }
  if (!cell && (*source)->Declaration().How != Scheme::WholeWorld) {
    return std::unexpected("an original OSM catalogue requires geographic cell demand");
  }
  const auto at = cell ? Address::AtGeoCell(*cell) : Address::Whole(0);
  if (cell && ((*source)->Declaration().How != Scheme::GeodeticGrid ||
               (*source)->Covers(Fetch(DataKind::OriginalOsm, at)) != Coverage::Inside ||
               (*source)->Serves(Fetch(DataKind::OriginalOsm, at)) != at)) {
    return std::unexpected("an original OSM catalogue refused geographic cell demand");
  }
  auto region = std::unique_ptr<OsmApiRegion>(new OsmApiRegion(store, wire, at));
  if (region->Sources_.Add(std::move(*source)) != SourceSet::Registration::Accepted) {
    return std::unexpected("original OSM source registration failed");
  }
  region->Sources_.Seal();
  return region;
}

OsmSourceChunk OsmApiRegion::Chunk(const SourceProvider &provider,
                                   std::optional<GeoCellId> cell) const {
  const auto &declaration = Sources_.At(0).Declaration();
  OsmSourceChunk chunk{
      .Provider = provider, .Xml = {}, .Origin = declaration.Endpoint, .Cell = cell};
  if (cell) {
    chunk.Provider.Dataset = declaration.Id;
    chunk.Provider.Revision = declaration.Revision;
    chunk.Provider.PayloadSha256 = declaration.PayloadSha256;
    chunk.Provider.Coverage = cell->Bounds();
    chunk.Origin += "/" + Address_.Text();
  }
  return chunk;
}

std::expected<OsmApiRegion::Collected, std::string> OsmApiRegion::Collect(OsmSourceChunk &chunk,
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
  if (delivery.Where() == Delivery::State::Pending) { return Collected::Pending; }
  const auto reason =
      delivery.Failure() ? delivery.Failure()->Reason : FetchFailureReason::ProviderRefused;
  if (chunk.Cell && reason == FetchFailureReason::CapacityRefused) {
    Query_.reset();
    return Collected::Refine;
  }
  return std::unexpected("original OSM source '" + chunk.Origin +
                         "' failed: " + std::string(Name(reason)));
}

bool OsmApiRegion::Begin(const ContentStore &store, std::vector<GeoCellId> &refine) {
  const auto cell = Address_.GeoCell();
  if (cell && store.HasCompleteChildCoverage(Sources_.At(0).Declaration(), *cell)) {
    refine.push_back(*cell);
    return true;
  }
  Query_.emplace(Sources_.Ask(Fetch(DataKind::OriginalOsm, Address_)));
  return false;
}

}
