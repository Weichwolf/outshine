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
  Region(ContentStore &store, Transport &wire) : Sources(store), Wire(wire) {}

  ~Region() {
    if (Query) { SourceSet::Abandon(*Query, Wire); }
  }

  SourceSet Sources;
  Transport &Wire;
  std::optional<SourceSet::Query> Query;
};

std::expected<std::unique_ptr<Region>, std::string> MakeRegion(const SourceProvider &provider,
                                                               ContentStore &store,
                                                               Transport &wire,
                                                               const ProviderRegistry *registry,
                                                               std::string_view shippedRoot) {
  auto source = MakeDeclaredSource(provider, shippedRoot, registry);
  if (!source) { return std::unexpected(std::move(source.error())); }
  if ((*source)->Declaration().Kind != DataKind::OriginalOsm ||
      (*source)->Declaration().Wire != WireFormat::OsmXml) {
    return std::unexpected("an original OSM provider must supply original OSM XML");
  }
  if ((*source)->Declaration().How != Scheme::WholeWorld) {
    return std::unexpected("an original OSM catalogue requires geographic cell demand");
  }
  auto region = std::make_unique<Region>(store, wire);
  if (region->Sources.Add(std::move(*source)) != SourceSet::Registration::Accepted) {
    return std::unexpected("original OSM source registration failed");
  }
  region->Sources.Seal();
  return region;
}

std::expected<bool, std::string>
CollectRegion(Region &region, OsmSourceChunk &chunk, Transport &wire, double beganMs) {
  if (!region.Query) { return false; }
  auto delivery = region.Sources.Collect(*region.Query, wire);
  if (auto answer = delivery.Take()) {
    chunk.Xml.assign(answer->Bytes.begin(), answer->Bytes.end());
    chunk.ReadMs = wire.NowMs() - beganMs;
    chunk.FromStore = region.Sources.Counters().FromStore != 0;
    region.Query.reset();
    return true;
  }
  if (delivery.Where() == Delivery::State::Pending) { return false; }
  const auto reason =
      delivery.Failure() ? delivery.Failure()->Reason : FetchFailureReason::ProviderRefused;
  return std::unexpected("original OSM source '" + chunk.Origin +
                         "' failed: " + std::string(Name(reason)));
}
}

std::expected<OsmSourceRead, std::string>
ReadOsmApiRegions(std::span<const SourceProvider> providers,
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
  for (const auto &provider : providers) {
    auto region = MakeRegion(provider, store, wire, registry, shippedRoot);
    if (!region) { return std::unexpected(std::move(region.error())); }
    chunks.push_back({.Provider = provider,
                      .Xml = {},
                      .Origin = (*region)->Sources.At(0).Declaration().Endpoint});
    regions.push_back(std::move(*region));
  }
  std::vector<double> beganMs(providers.size());
  size_t next = 0;
  size_t completed = 0;
  while (completed < regions.size()) {
    const double nowMs = wire.NowMs();
    if (stop.stop_requested() || !std::isfinite(nowMs) || !std::isfinite(deadlineMs) ||
        nowMs >= deadlineMs) {
      return std::unexpected(stop.stop_requested()
                                 ? "original OSM source acquisition canceled"
                                 : "original OSM source acquisition deadline exceeded");
    }
    while (next < regions.size() && next - completed < kConcurrentRegions) {
      regions[next]->Query.emplace(
          regions[next]->Sources.Ask(Fetch(DataKind::OriginalOsm, Address::Whole(0))));
      beganMs[next] = nowMs;
      ++next;
    }
    for (size_t at = 0; at < next; ++at) {
      auto ready = CollectRegion(*regions[at], chunks[at], wire, beganMs[at]);
      if (!ready) { return std::unexpected(std::move(ready.error())); }
      completed += static_cast<size_t>(*ready);
    }
    if (completed == regions.size()) { break; }
    if (next < regions.size() && next - completed < kConcurrentRegions) { continue; }
    (void)wire.Await(std::min(kIoAwaitMs, std::max(0.0, deadlineMs - wire.NowMs())));
  }
  return OsmSourceRead{.Chunks = std::move(chunks), .ElapsedMs = wire.NowMs() - began};
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
