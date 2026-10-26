#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSNAPSHOT_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSNAPSHOT_H
#include "TransportNetworkSnapshot.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Path {
class Network;
}

namespace outshine::Generators::Osm {
class OsmField;
class StreetField;

enum class MvtTransportOmissionReason : uint8_t {
  UnsupportedArea,
  UnsupportedTunnel,
  InvalidAttributes,
  UnclassifiedWay,
  UnqualifiedLevelConnection
};

struct MvtTransportOmission {
  uint32_t Source = 0;
  std::array<uint8_t, 32> Feature{};
  MvtTransportOmissionReason Reason = MvtTransportOmissionReason::UnclassifiedWay;
};

struct MvtTransportSnapshot {
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  std::vector<MvtTransportOmission> Omissions;
  size_t InputConnections = 0;
};

[[nodiscard]] std::expected<MvtTransportSnapshot, std::string>
BuildMvtTransportSnapshot(const OsmField &field,
                          const StreetField &streets,
                          const Path::Network &network,
                          std::span<const GeographicBounds> coverage,
                          const std::stop_token &stop = {});
}
#endif
