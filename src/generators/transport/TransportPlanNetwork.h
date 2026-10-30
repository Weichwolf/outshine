#ifndef OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTPLANNETWORK_H
#define OUTSHINE_GENERATORS_TRANSPORT_TRANSPORTPLANNETWORK_H
#include "TransportNetworkSnapshot.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
struct TransportPlanNetworkLimits {
  size_t DirectedEdges = 16384;
  size_t References = 32768;
  size_t Routes = 1024;
  size_t RouteEdges = 65536;
  size_t TextBytes = 1048576;
  size_t OutputCapacityBytes = 8388608;
};

struct TransportPlanNetworkWork {
  size_t EdgeLookups = 0;
  size_t NodeLookups = 0;
  size_t PathLookups = 0;
  size_t ReferenceVisits = 0;
  size_t RouteEdgeVisits = 0;
  size_t CopiedEdgeBytes = 0;
  size_t OutputCapacityBytes = 0;
  size_t ScratchCapacityBytes = 0;
};

struct TransportPlanRouteOmission {
  std::string Id;
  uint64_t SourceRouteId = 0;
};

struct TransportPlanNetwork {
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  std::vector<TransportPlanRouteOmission> OmittedRoutes;
  TransportPlanNetworkWork Work;
};

enum class TransportPlanNetworkError : uint8_t {
  EmptySelection,
  MissingEdge,
  InvalidSource,
  CapacityExceeded,
  Canceled
};

[[nodiscard]] std::expected<TransportPlanNetwork, TransportPlanNetworkError>
ProjectTransportPlanNetwork(const World::TransportNetworkSnapshot &source,
                            std::span<const World::TransportEdgeId> directedEdges,
                            TransportPlanNetworkLimits limits = {},
                            const std::stop_token &stop = {});
}
#endif
