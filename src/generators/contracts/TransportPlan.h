#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLAN_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLAN_H

#include "Earth.h"
#include "EarthworkStamp.h"
#include "HeightSolid.h"
#include "TransportBoundary.h"
#include "TransportGeometry.h"
#include "TransportPlanAddress.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace outshine::World {
class TransportNetworkSnapshot;
}

namespace outshine::Generators {
enum class TransportUse : uint8_t { Road, Path, Rail };
enum class TransportCoverage : uint8_t { Unknown, Partial, Complete };

struct TransportInputReference {
  std::string DatasetId;
  std::string Version;
  std::string Revision;
  std::string ItemId;
  std::array<uint8_t, 32> Content{};
};

struct TransportPort {
  TransportBoundary Boundary;
  std::string SourceId;
  std::array<uint8_t, 32> Inputs{};
  LongitudeLatitude Anchor{};
  RoadPort Contact{};
  RoadProfile Profile = RoadProfile::Rounded;
  double CrossfallRad = 0;
  int32_t Layer = 0;
  TransportUse Use = TransportUse::Road;
};

struct TransportBoundaryPortReference {
  uint32_t Port = 0;
};

struct TransportJunctionGateReference {
  uint32_t Junction = 0;
  uint32_t Gate = 0;
};

using TransportPathEnd =
    std::variant<std::monostate, TransportBoundaryPortReference, TransportJunctionGateReference>;

struct TransportPath {
  std::string PartId;
  std::string SourceId;
  std::string Material;
  uint32_t FirstStation = 0;
  uint32_t StationCount = 0;
  double HalfWidthM = 0;
  double CrossfallRad = 0;
  double MaximumGradient = 0;
  double SpeedMps = 0;
  RoadProfile Profile = RoadProfile::Rounded;
  TransportUse Use = TransportUse::Road;
  int32_t Layer = 0;
  int32_t Lanes = 0;
  int32_t Priority = 0;
  bool Oneway = false;
  bool Bridge = false;
  std::array<TransportPathEnd, 2> Ends;
  std::optional<uint32_t> Body;
};

struct TransportJunction {
  uint64_t Node = 0;
  std::vector<std::string> Sources;
  std::string Material;
  std::vector<RoadGate> Gates;
  RoadPlane Plane{};
};

enum class TransportOmissionReason : uint8_t {
  InvalidDefinition,
  InvalidGeometry,
  MissingBoundary,
  ConflictingConstraints,
  UnsupportedRepresentation,
  SourceUnavailable,
  ResourceDeferred
};

struct TransportOmission {
  std::string SourceId;
  TransportOmissionReason Reason = TransportOmissionReason::MissingBoundary;
};

struct TransportPlan {
  explicit TransportPlan(const TransportPlanDefinition &definition) noexcept
      : Definition(definition) {}

  TransportPlanDefinition Definition;
  LongitudeLatitude Anchor{};
  std::array<uint8_t, 32> InputBinding{};
  TransportCoverage Coverage = TransportCoverage::Unknown;
  std::vector<TransportInputReference> Inputs;
  std::vector<TransportPath> Paths;
  std::vector<RoadStation> Stations;
  std::vector<TransportJunction> Junctions;
  std::vector<TransportPort> Ports;
  std::vector<HeightSolid> Bodies;
  std::vector<EarthworkStamp> TerrainContacts;
  std::vector<TransportOmission> Omissions;
  std::shared_ptr<const World::TransportNetworkSnapshot> Network;
};

using TransportPlanLease = std::shared_ptr<const TransportPlan>;
}
#endif
