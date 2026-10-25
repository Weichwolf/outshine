#ifndef OUTSHINE_GENERATORS_TRANSPORT_TERRAIN_BRIDGEGROUNDSAMPLES_H
#define OUTSHINE_GENERATORS_TRANSPORT_TERRAIN_BRIDGEGROUNDSAMPLES_H

#include "BridgeInputRequirements.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Ground {
class HeightField;
}

namespace outshine::Generators {
enum class BridgeGroundGap : uint8_t {
  MissingTerrain,
  OutsideCoverage,
  FallbackTerrain,
  PartialBoundary,
  MissingProvenance,
  InvalidHeight
};

struct BridgeGroundValue {
  double AslM = 0;
  size_t TerrainBlock = 0;
};

struct BridgeGroundSample {
  uint64_t SourceNode = 0;
  std::variant<BridgeGroundGap, BridgeGroundValue> Value = BridgeGroundGap::MissingTerrain;
};

struct BridgeGroundWork {
  size_t NodeLookups = 0;
  size_t TileClassifications = 0;
  size_t HeightLookups = 0;
  size_t SourceReferenceChecks = 0;
  size_t Sourced = 0;
  size_t Unresolved = 0;
  size_t ContainerCapacityBytes = 0;
};

struct BridgeGroundSamples {
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::shared_ptr<const Ground::HeightField> Terrain;
  std::vector<BridgeGroundSample> Samples;
  BridgeGroundWork Work{};
};

struct BridgeGroundLimits {
  size_t NodesMost = 0;
  size_t TerrainBlocksMost = 0;
  size_t SourcesPerBlockMost = 0;
  size_t ContainerCapacityMost = 0;
};

enum class BridgeGroundErrorCode : uint8_t {
  MissingSource,
  InvalidSource,
  InvalidSelection,
  CapacityExceeded,
  Canceled
};

struct BridgeGroundError {
  BridgeGroundErrorCode Code;
  std::optional<uint64_t> Node;
};

[[nodiscard]] std::expected<BridgeGroundSamples, BridgeGroundError>
SampleBridgeGround(const BridgeInputRequirements &inputs,
                   std::shared_ptr<const Ground::HeightField> terrain,
                   BridgeGroundLimits limits,
                   const std::stop_token &stop = {});
}
#endif
