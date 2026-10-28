#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSOURCEFACTS_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSOURCEFACTS_H

#include "MvtTransportNodeGroups.h"
#include "TransportNetworkSnapshot.h"
#include "Earth.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::Generators::Osm {

enum class MvtTransportSourceFactState : uint8_t {
  NotRequested,
  Captured,
  BudgetExceeded,
  MissingNodeGroups
};

struct MvtTransportSourceMember {
  World::TransportEdgeId Edge;
  uint64_t DomainNode = 0;
  uint32_t NodeGroup = 0;
  uint32_t StreetWay = 0;
  uint32_t SourceStep = 0;
  uint8_t End = 0;
  bool SourceTerminal = false;
};

struct MvtTransportSourceGroup {
  uint32_t FirstMember = 0;
  uint32_t MemberCount = 0;
  uint64_t OriginalEndpointCount = 0;
  uint64_t FilteredEndpointCount = 0;
  // Completeness refers to the supplied native Network, not unloaded source data.
  bool Complete = false;
  bool Frontier = false;
};

struct MvtTransportWayFacts {
  uint32_t StreetWay = 0;
  uint32_t Feature = 0;
  uint32_t Ring = 0;
  uint32_t PointCount = 0;
  uint32_t HalfWidthWord = 0;
  uint32_t MaximumGradientWord = 0;
  std::array<LongitudeLatitude, 4> EndStencil{};
  std::array<bool, 2> OwnsEnds{};
};

struct MvtTransportSourceFacts {
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
  std::shared_ptr<const MvtTransportNodeGroups> NodeGroups;
  uint64_t SourceGeneration = 0;
  uint64_t SourceWayCount = 0;
  std::vector<MvtTransportSourceGroup> Groups;
  std::vector<MvtTransportSourceMember> Members;
  std::vector<MvtTransportWayFacts> Ways;

  struct MissingWay {
    uint32_t Omission = 0;
    MvtTransportWayFacts Way;
    int32_t Layer = 0;
    bool Bridge = false;
    bool Tunnel = false;
    std::vector<LongitudeLatitude> Points;
  };

  // Original geometry is evidence, not a certified network-influence bound.
  std::vector<MissingWay> MissingWays;
  std::array<uint8_t, 32> OmissionsDigest{};
  std::array<uint8_t, 32> Digest{};
};

}
#endif
