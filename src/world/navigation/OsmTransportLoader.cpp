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
constexpr size_t kMaxRoutes = 32;
constexpr size_t kMaxRouteEdges = 65536;

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
                            std::string_view shippedRoot,
                            std::span<const OsmCircuitRequest> routes) {
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
  if (routes.size() > kMaxRoutes || (!routes.empty() && providers.empty())) {
    return std::unexpected("semantic OSM routes require 1 to 32 requests and a source");
  }
  for (size_t at = 0; at < routes.size(); ++at) {
    if (routes[at].Id.empty() || routes[at].RelationId == 0) {
      return std::unexpected("semantic OSM route requires a name and relation ID");
    }
    for (size_t earlier = 0; earlier < at; ++earlier) {
      if (routes[earlier].Id == routes[at].Id) {
        return std::unexpected("duplicate semantic OSM route '" + routes[at].Id + "'");
      }
    }
  }
  std::vector<Data::SourceProvider> requested(providers.begin(), providers.end());
  std::vector<OsmCircuitRequest> requestedRoutes(routes.begin(), routes.end());
  std::ranges::sort(requested, {}, &Data::SourceProvider::Priority);
  const std::string root(shippedRoot);
  if (requested == Requested_ && requestedRoutes == RequestedRoutes_ &&
      (requested.empty() || root == Root_) && Phase_ != Phase::Failed) {
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
  RequestedRoutes_ = std::move(requestedRoutes);
  Root_ = root;
  Error_.clear();
  if (Requested_.empty()) {
    Current_.reset();
    Phase_ = Phase::Inactive;
    return {};
  }

  auto result = std::make_shared<Result>();
  std::vector<Data::SourceProvider> input = Requested_;
  std::vector<OsmCircuitRequest> routeInput = RequestedRoutes_;
  const Tasks::Handle handle =
      Tasks_->Post([input = std::move(input), routeInput = std::move(routeInput), root, result] {
        result->Value = Load(input, root, routeInput);
      });
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
                         std::string_view shippedRoot,
                         std::span<const OsmCircuitRequest> routes) {
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
  ResolvedTransport transport{.Graph = std::move(*graph), .Routes = {}};
  transport.Routes.reserve(routes.size());
  size_t routeEdges = 0;
  for (const OsmCircuitRequest &request : routes) {
    auto route = transport.Graph.ResolveCircuit(
        loaded->Elements, request.RelationId, {}, kMaxRouteEdges - routeEdges);
    if (!route) {
      return std::unexpected("semantic OSM route '" + request.Id + "' failed at source object " +
                             std::to_string(route.error().SourceId) + " with code " +
                             std::to_string(static_cast<int>(route.error().Code)));
    }
    routeEdges += route->EdgeIds.size();
    transport.Routes.push_back({.Id = request.Id, .Circuit = std::move(*route)});
  }
  return std::make_shared<TransportNetworkSnapshot>(
      std::move(loaded->Elements),
      std::move(transport),
      std::move(loaded->Coverage),
      TransportLoadMetrics{.SourceBytes = loaded->SourceBytes,
                           .ReadMs = loaded->ReadMs,
                           .ParseMs = loaded->ParseMs,
                           .GraphMs = graphMs});
}

}
