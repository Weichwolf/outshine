#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTS_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTS_H

#include "GroundPortSourceProfile.h"
#include "TransportPlan.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators::Osm {
class OsmField;
class StreetField;
struct MvtTransportSnapshot;

struct MvtGroundPortInputLimits {
  size_t Nodes = 16384;
  size_t DirectedEdges = 32768;
  size_t Paths = 16384;
  size_t SourceWays = 16384;
  size_t SourceTiles = 1024;
  size_t Omissions = 16384;
  size_t AttributeWords = 1048576;
  size_t ContainerBytes = 4194304;
};

struct MvtGroundPortInputWork {
  size_t SourceTiles = 0;
  size_t Paths = 0;
  size_t DirectedEdges = 0;
  size_t NodeLookups = 0;
  size_t AttributeWords = 0;
  size_t MissingProfiles = 0;
  size_t OmissionLookups = 0;
  size_t ContainerCapacityBytes = 0;
};

enum class MvtGroundPortInputError : uint8_t {
  MissingSource,
  SourceChanged,
  InvalidBinding,
  MissingIncidence,
  CapacityExceeded,
  Canceled
};

struct MvtGroundPortInputs {
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  std::vector<GroundPortSourceProfile> Profiles;
  std::vector<uint64_t> UnknownNodes;
  TransportInputReference ProfileInput;
  MvtGroundPortInputWork Work;
};

[[nodiscard]] std::expected<MvtGroundPortInputs, MvtGroundPortInputError>
BindMvtGroundPortInputs(const MvtTransportSnapshot &snapshot,
                        const OsmField &field,
                        const StreetField &streets,
                        MvtGroundPortInputLimits limits = {},
                        const std::stop_token &stop = {});
}
#endif
