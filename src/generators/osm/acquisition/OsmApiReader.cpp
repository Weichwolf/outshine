#include "OsmApiReader.h"
#include "OsmApiRegion.h"

#include "SourceConfiguration.h"
#include "SourceSet.h"

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr double kIoAwaitMs = 5.0;
constexpr size_t kConcurrentRegions = 2;

std::expected<size_t, std::string> PollStarted(std::span<const std::unique_ptr<ApiRegion>> regions,
                                               std::span<SourceChunk> chunks,
                                               std::span<const double> beganMs,
                                               std::vector<Data::GeoCellId> &refine) {
  size_t completed = 0;
  for (size_t at = 0; at < regions.size(); ++at) {
    auto ready = regions[at]->Collect(chunks[at], beganMs[at]);
    if (!ready) { return std::unexpected(std::move(ready.error())); }
    const auto cell = chunks[at].Cell;
    if (*ready == ApiRegion::Collected::Refine && cell) { refine.push_back(*cell); }
    completed += static_cast<size_t>(*ready != ApiRegion::Collected::Pending);
  }
  return completed;
}

std::expected<SourceRead, std::string>
ReadRequests(std::span<const Data::SourceProvider> providers,
             std::span<const Data::GeoCellId> cells,
             Data::ContentStore &store,
             Data::Transport &wire,
             double deadlineMs,
             const std::stop_token &stop,
             const Data::ProviderRegistry *registry,
             std::string_view shippedRoot,
             const std::function<double()> &currentDeadline) {
  const double began = wire.NowMs();
  std::vector<std::unique_ptr<ApiRegion>> regions;
  std::vector<SourceChunk> chunks;
  regions.reserve(providers.size());
  chunks.reserve(providers.size());
  for (size_t at = 0; at < providers.size(); ++at) {
    const auto &provider = providers[at];
    const auto cell = cells.empty() ? std::nullopt : std::optional(cells[at]);
    auto region = ApiRegion::Create(provider, store, wire, registry, shippedRoot, cell);
    if (!region) { return std::unexpected(std::move(region.error())); }
    chunks.push_back((*region)->Chunk(provider, cell));
    regions.push_back(std::move(*region));
  }
  std::vector<double> beganMs(providers.size());
  size_t next = 0;
  size_t completed = 0;
  std::vector<Data::GeoCellId> refine;
  while (completed < regions.size()) {
    const double nowMs = wire.NowMs();
    const double untilMs = currentDeadline ? currentDeadline() : deadlineMs;
    if (stop.stop_requested() || !std::isfinite(nowMs) || !std::isfinite(untilMs) ||
        nowMs >= untilMs) {
      return std::unexpected(stop.stop_requested()
                                 ? "original OSM source acquisition canceled"
                                 : "original OSM source acquisition deadline exceeded");
    }
    while (next < regions.size() && next - completed < kConcurrentRegions) {
      completed += static_cast<size_t>(regions[next]->Begin(store, refine));
      beganMs[next] = nowMs;
      ++next;
    }
    auto ready = PollStarted(std::span(regions).first(next), chunks, beganMs, refine);
    if (!ready) { return std::unexpected(std::move(ready.error())); }
    completed += *ready;
    if (completed == regions.size()) { break; }
    if (next < regions.size() && next - completed < kConcurrentRegions) { continue; }
    (void)wire.Await(std::min(kIoAwaitMs, std::max(0.0, untilMs - wire.NowMs())));
  }
  std::erase_if(chunks, [](const auto &chunk) { return chunk.Xml.empty(); });
  return SourceRead{
      .Chunks = std::move(chunks), .Refine = std::move(refine), .ElapsedMs = wire.NowMs() - began};
}
}

std::expected<SourceRead, std::string> ReadCells(const Data::SourceProvider &catalogue,
                                                 std::span<const Data::GeoCellId> cells,
                                                 Data::ContentStore &store,
                                                 Data::Transport &wire,
                                                 double deadlineMs,
                                                 const std::stop_token &stop,
                                                 const Data::ProviderRegistry *registry,
                                                 std::string_view shippedRoot,
                                                 const std::function<double()> &currentDeadline) {
  if (cells.empty() || cells.size() > kConcurrentRegions) {
    return std::unexpected("original OSM cell jobs require one or two geographic cells");
  }
  if (cells.size() == kConcurrentRegions && cells.front() == cells.back()) {
    return std::unexpected("original OSM cell jobs require distinct addresses");
  }
  const std::vector<Data::SourceProvider> providers(cells.size(), catalogue);
  return ReadRequests(
      providers, cells, store, wire, deadlineMs, stop, registry, shippedRoot, currentDeadline);
}

std::expected<SourceRead, std::string> ReadRegions(std::span<const Data::SourceProvider> providers,
                                                   Data::ContentStore &store,
                                                   Data::Transport &wire,
                                                   double deadlineMs,
                                                   const std::stop_token &stop,
                                                   const Data::ProviderRegistry *registry,
                                                   std::string_view shippedRoot,
                                                   const std::function<double()> &currentDeadline) {
  return ReadRequests(
      providers, {}, store, wire, deadlineMs, stop, registry, shippedRoot, currentDeadline);
}

std::expected<SourceChunk, std::string> ReadRegion(const Data::SourceProvider &provider,
                                                   Data::ContentStore &store,
                                                   Data::Transport &wire,
                                                   double deadlineMs,
                                                   const std::stop_token &stop,
                                                   const Data::ProviderRegistry *registry,
                                                   std::string_view shippedRoot) {
  auto chunks =
      ReadRegions(std::span(&provider, 1), store, wire, deadlineMs, stop, registry, shippedRoot);
  if (!chunks) { return std::unexpected(std::move(chunks.error())); }
  return std::move(chunks->Chunks.front());
}

}
