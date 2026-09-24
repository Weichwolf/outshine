#include "OsmTransportLoader.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <ranges>
#include <ratio>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmChunkSetLoader.h"
#include "SourceProviderValidation.h"

namespace outshine::World {

namespace {

constexpr size_t kMaxChunks = 4;
constexpr size_t kMaxPending = 2;

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point began) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
      .count();
}

}

OsmTransportLoader::~OsmTransportLoader() {
  for (const Pending &pending : Pending_) { Tasks_->Wait(pending.Handle); }
}

std::expected<void, std::string>
OsmTransportLoader::Request(std::span<const Data::SourceProvider> providers,
                            std::string_view shippedRoot) {
  if (auto valid = Data::ValidateSourceProviders(providers); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  if (providers.size() > kMaxChunks) {
    return std::unexpected("semantic OSM source exceeds the four-chunk request budget");
  }
  for (const Data::SourceProvider &provider : providers) {
    if (provider.Kind != "osm") {
      return std::unexpected("semantic OSM source accepts only osm provider chunks");
    }
  }
  std::vector<Data::SourceProvider> requested(providers.begin(), providers.end());
  std::ranges::sort(requested, {}, &Data::SourceProvider::Priority);
  const std::string root(shippedRoot);
  if (requested == Requested_ && (requested.empty() || root == Root_) && Phase_ != Phase::Failed) {
    return {};
  }
  if (Revision_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected("semantic OSM source request revision is exhausted");
  }
  Poll();
  if (!requested.empty() && Pending_.size() >= kMaxPending) {
    return std::unexpected("semantic OSM source has two builds pending");
  }

  ++Revision_;
  Requested_ = std::move(requested);
  Root_ = root;
  Error_.clear();
  if (Requested_.empty()) {
    Current_.reset();
    Phase_ = Phase::Inactive;
    return {};
  }

  auto result = std::make_shared<Result>();
  std::vector<Data::SourceProvider> input = Requested_;
  const Tasks::Handle handle =
      Tasks_->Post([input = std::move(input), root, result] { result->Value = Load(input, root); });
  Pending_.push_back({.Handle = handle, .Revision = Revision_, .Output = std::move(result)});
  Phase_ = Phase::Loading;
  return {};
}

void OsmTransportLoader::Poll() {
  for (size_t at = 0; at < Pending_.size();) {
    if (!Tasks_->Done(Pending_[at].Handle)) {
      ++at;
      continue;
    }
    const Pending finished = std::move(Pending_[at]);
    Pending_.erase(Pending_.begin() + static_cast<std::ptrdiff_t>(at));
    if (finished.Revision != Revision_) { continue; }
    if (!finished.Output->Value) {
      Error_ = "semantic OSM source worker returned no result";
      Phase_ = Phase::Failed;
      continue;
    }
    LoadResult &loaded = *finished.Output->Value;
    if (!loaded) {
      Error_ = std::move(loaded.error());
      Phase_ = Phase::Failed;
      continue;
    }
    Current_ = std::move(*loaded);
    Error_.clear();
    Phase_ = Phase::Ready;
  }
}

OsmTransportLoader::LoadResult
OsmTransportLoader::Load(std::span<const Data::SourceProvider> providers,
                         std::string_view shippedRoot) {
  auto loaded = Data::OsmChunkSetLoader::Load(providers, shippedRoot);
  if (!loaded) { return std::unexpected(std::move(loaded.error())); }
  const auto graphAt = std::chrono::steady_clock::now();
  auto graph = TransportTopology::Build(loaded->Elements);
  const double graphMs = MillisecondsSince(graphAt);
  if (!graph) {
    return std::unexpected("semantic OSM graph failed at source object " +
                           std::to_string(graph.error().SourceId) + " with code " +
                           std::to_string(static_cast<int>(graph.error().Code)));
  }
  return std::make_shared<TransportTopologySnapshot>(
      std::move(loaded->Elements),
      std::move(*graph),
      std::move(loaded->Coverage),
      TransportLoadMetrics{.SourceBytes = loaded->SourceBytes,
                           .ReadMs = loaded->ReadMs,
                           .ParseMs = loaded->ParseMs,
                           .GraphMs = graphMs});
}

}
