#include "OsmApiReader.h"

#include "DeclaredSources.h"
#include "SourceSet.h"

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Data {
namespace {
constexpr double kIoAwaitMs = 5.0;
constexpr size_t kConcurrentRegions = 2;

struct Region {
  Region(ContentStore &store, Transport &wire, Address at) : Sources(store), Wire(wire), At(at) {}

  ~Region() {
    if (Query) { SourceSet::Abandon(*Query, Wire); }
  }

  SourceSet Sources;
  Transport &Wire;
  Address At;
  std::optional<SourceSet::Query> Query;
};

std::expected<std::unique_ptr<Region>, std::string> MakeRegion(const SourceProvider &provider,
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
  auto region = std::make_unique<Region>(store, wire, at);
  if (region->Sources.Add(std::move(*source)) != SourceSet::Registration::Accepted) {
    return std::unexpected("original OSM source registration failed");
  }
  region->Sources.Seal();
  return region;
}

OsmSourceChunk
MakeChunk(const SourceProvider &provider, const Region &region, std::optional<GeoCellId> cell) {
  const auto &declaration = region.Sources.At(0).Declaration();
  OsmSourceChunk chunk{
      .Provider = provider, .Xml = {}, .Origin = declaration.Endpoint, .Cell = cell};
  if (cell) {
    chunk.Provider.Dataset = declaration.Id;
    chunk.Provider.Revision = declaration.Revision;
    chunk.Provider.PayloadSha256 = declaration.PayloadSha256;
    chunk.Provider.Coverage = cell->Bounds();
    chunk.Origin += "/" + region.At.Text();
  }
  return chunk;
}

enum class Collected { Pending, Ready, Refine };

std::expected<Collected, std::string>
CollectRegion(Region &region, OsmSourceChunk &chunk, Transport &wire, double beganMs) {
  if (!region.Query) { return Collected::Pending; }
  auto delivery = region.Sources.Collect(*region.Query, wire);
  if (auto answer = delivery.Take()) {
    chunk.Xml.assign(answer->Bytes.begin(), answer->Bytes.end());
    chunk.ReadMs = wire.NowMs() - beganMs;
    chunk.FromStore = region.Sources.Counters().FromStore != 0;
    region.Query.reset();
    return Collected::Ready;
  }
  if (delivery.Where() == Delivery::State::Pending) { return Collected::Pending; }
  const auto reason =
      delivery.Failure() ? delivery.Failure()->Reason : FetchFailureReason::ProviderRefused;
  if (chunk.Cell && reason == FetchFailureReason::CapacityRefused) {
    region.Query.reset();
    return Collected::Refine;
  }
  return std::unexpected("original OSM source '" + chunk.Origin +
                         "' failed: " + std::string(Name(reason)));
}

std::expected<size_t, std::string> PollStarted(std::span<const std::unique_ptr<Region>> regions,
                                               std::span<OsmSourceChunk> chunks,
                                               std::span<const double> beganMs,
                                               Transport &wire,
                                               std::vector<GeoCellId> &refine) {
  size_t completed = 0;
  for (size_t at = 0; at < regions.size(); ++at) {
    auto ready = CollectRegion(*regions[at], chunks[at], wire, beganMs[at]);
    if (!ready) { return std::unexpected(std::move(ready.error())); }
    const auto cell = chunks[at].Cell;
    if (*ready == Collected::Refine && cell) { refine.push_back(*cell); }
    completed += static_cast<size_t>(*ready != Collected::Pending);
  }
  return completed;
}

bool BeginRegion(Region &region, const ContentStore &store, std::vector<GeoCellId> &refine) {
  const auto cell = region.At.GeoCell();
  if (cell && store.HasCompleteChildCoverage(region.Sources.At(0).Declaration(), *cell)) {
    refine.push_back(*cell);
    return true;
  }
  region.Query.emplace(region.Sources.Ask(Fetch(DataKind::OriginalOsm, region.At)));
  return false;
}

std::expected<OsmSourceRead, std::string> ReadRequests(std::span<const SourceProvider> providers,
                                                       std::span<const GeoCellId> cells,
                                                       ContentStore &store,
                                                       Transport &wire,
                                                       double deadlineMs,
                                                       const std::stop_token &stop,
                                                       const ProviderRegistry *registry,
                                                       std::string_view shippedRoot) {
  const double began = wire.NowMs();
  std::vector<std::unique_ptr<Region>> regions;
  std::vector<OsmSourceChunk> chunks;
  regions.reserve(providers.size());
  chunks.reserve(providers.size());
  for (size_t at = 0; at < providers.size(); ++at) {
    const auto &provider = providers[at];
    const auto cell = cells.empty() ? std::nullopt : std::optional(cells[at]);
    auto region = MakeRegion(provider, store, wire, registry, shippedRoot, cell);
    if (!region) { return std::unexpected(std::move(region.error())); }
    chunks.push_back(MakeChunk(provider, **region, cell));
    regions.push_back(std::move(*region));
  }
  std::vector<double> beganMs(providers.size());
  size_t next = 0;
  size_t completed = 0;
  std::vector<GeoCellId> refine;
  while (completed < regions.size()) {
    const double nowMs = wire.NowMs();
    if (stop.stop_requested() || !std::isfinite(nowMs) || !std::isfinite(deadlineMs) ||
        nowMs >= deadlineMs) {
      return std::unexpected(stop.stop_requested()
                                 ? "original OSM source acquisition canceled"
                                 : "original OSM source acquisition deadline exceeded");
    }
    while (next < regions.size() && next - completed < kConcurrentRegions) {
      completed += static_cast<size_t>(BeginRegion(*regions[next], store, refine));
      beganMs[next] = nowMs;
      ++next;
    }
    auto ready = PollStarted(std::span(regions).first(next), chunks, beganMs, wire, refine);
    if (!ready) { return std::unexpected(std::move(ready.error())); }
    completed += *ready;
    if (completed == regions.size()) { break; }
    if (next < regions.size() && next - completed < kConcurrentRegions) { continue; }
    (void)wire.Await(std::min(kIoAwaitMs, std::max(0.0, deadlineMs - wire.NowMs())));
  }
  std::erase_if(chunks, [](const auto &chunk) { return chunk.Xml.empty(); });
  return OsmSourceRead{
      .Chunks = std::move(chunks), .Refine = std::move(refine), .ElapsedMs = wire.NowMs() - began};
}
}

std::expected<OsmSourceRead, std::string> ReadOsmApiCells(const SourceProvider &catalogue,
                                                          std::span<const GeoCellId> cells,
                                                          ContentStore &store,
                                                          Transport &wire,
                                                          double deadlineMs,
                                                          const std::stop_token &stop,
                                                          const ProviderRegistry *registry,
                                                          std::string_view shippedRoot) {
  if (cells.empty() || cells.size() > kConcurrentRegions) {
    return std::unexpected("original OSM cell jobs require one or two geographic cells");
  }
  if (cells.size() == kConcurrentRegions && cells.front() == cells.back()) {
    return std::unexpected("original OSM cell jobs require distinct addresses");
  }
  const std::vector<SourceProvider> providers(cells.size(), catalogue);
  return ReadRequests(providers, cells, store, wire, deadlineMs, stop, registry, shippedRoot);
}

std::expected<OsmSourceRead, std::string>
ReadOsmApiRegions(std::span<const SourceProvider> providers,
                  ContentStore &store,
                  Transport &wire,
                  double deadlineMs,
                  const std::stop_token &stop,
                  const ProviderRegistry *registry,
                  std::string_view shippedRoot) {
  return ReadRequests(providers, {}, store, wire, deadlineMs, stop, registry, shippedRoot);
}

std::expected<OsmSourceChunk, std::string> ReadOsmApiRegion(const SourceProvider &provider,
                                                            ContentStore &store,
                                                            Transport &wire,
                                                            double deadlineMs,
                                                            const std::stop_token &stop,
                                                            const ProviderRegistry *registry,
                                                            std::string_view shippedRoot) {
  auto chunks = ReadOsmApiRegions(
      std::span(&provider, 1), store, wire, deadlineMs, stop, registry, shippedRoot);
  if (!chunks) { return std::unexpected(std::move(chunks.error())); }
  return std::move(chunks->Chunks.front());
}

}
