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
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "OsmChunkSetLoader.h"
#include "SourceProviderValidation.h"

namespace outshine::World {

namespace {

constexpr size_t kMaxChunks = 4;
constexpr size_t kMaxRoutes = 32;
constexpr size_t kMaxRouteEdges = 65536;

[[nodiscard]] double MillisecondsSince(std::chrono::steady_clock::time_point began) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
      .count();
}

}

OsmTransportLoader::~OsmTransportLoader() {
  if (Pending_) {
    (void)Pending_->Stop.request_stop();
    Tasks_->Wait(Pending_->Handle);
  }
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
  ++Revision_;
  Requested_ = std::move(requested);
  RequestedRoutes_ = std::move(requestedRoutes);
  Root_ = root;
  Error_.clear();
  if (Pending_) { (void)Pending_->Stop.request_stop(); }
  if (Requested_.empty()) {
    Current_.reset();
    Phase_ = Phase::Inactive;
    return {};
  }
  Phase_ = Phase::Loading;
  Poll();
  return {};
}

void OsmTransportLoader::Poll() {
  if (Pending_ && Tasks_->Done(Pending_->Handle)) {
    const Pending finished = std::move(*Pending_);
    Pending_.reset();
    if (finished.Revision != Revision_) {
      ++CanceledCount_;
    } else {
      ++CompletedCount_;
      if (!finished.Output->Value) {
        Error_ = "semantic OSM source worker returned no result";
        Phase_ = Phase::Failed;
      } else {
        LoadResult &loaded = *finished.Output->Value;
        if (!loaded) {
          Error_ = std::move(loaded.error());
          Phase_ = Phase::Failed;
        } else {
          Current_ = std::move(*loaded);
          Error_.clear();
          Phase_ = Phase::Ready;
        }
      }
    }
  }
  if (!Pending_ && Phase_ == Phase::Loading && !Requested_.empty()) { StartRequested(); }
}

void OsmTransportLoader::StartRequested() {
  auto result = std::make_shared<Result>();
  std::vector<Data::SourceProvider> input = Requested_;
  std::vector<OsmCircuitRequest> routes = RequestedRoutes_;
  const std::string root = Root_;
  std::stop_source stop;
  const std::stop_token token = stop.get_token();
  const Tasks::Handle handle =
      Tasks_->Post([input = std::move(input), routes = std::move(routes), root, result, token] {
        result->Value = Load(input, root, routes, token);
      });
  Pending_.emplace(Pending{.Handle = handle,
                           .Revision = Revision_,
                           .Output = std::move(result),
                           .Stop = std::move(stop)});
}

OsmTransportLoader::LoadResult
OsmTransportLoader::Load(std::span<const Data::SourceProvider> providers,
                         std::string_view shippedRoot,
                         std::span<const OsmCircuitRequest> routes,
                         const std::stop_token &stop) {
  auto loaded = Data::OsmChunkSetLoader::Load(providers, shippedRoot, stop);
  if (!loaded) { return std::unexpected(std::move(loaded.error())); }
  if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
  const auto graphAt = std::chrono::steady_clock::now();
  auto graph = TransportTopology::Build(loaded->Elements);
  const double graphMs = MillisecondsSince(graphAt);
  if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
  if (!graph) {
    return std::unexpected("semantic OSM graph failed at source object " +
                           std::to_string(graph.error().SourceId) + " with code " +
                           std::to_string(static_cast<int>(graph.error().Code)));
  }
  ResolvedTransport transport{.Graph = std::move(*graph), .Routes = {}};
  transport.Routes.reserve(routes.size());
  size_t routeEdges = 0;
  for (const OsmCircuitRequest &request : routes) {
    if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
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
  if (stop.stop_requested()) { return std::unexpected("semantic OSM source build canceled"); }
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
