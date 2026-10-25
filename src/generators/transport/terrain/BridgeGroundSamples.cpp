#include "BridgeGroundSamples.h"
#include "HeightField.h"
#include "TransportNetworkSnapshot.h"
#include "TileGeodesy.h"
#include "DataKind.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>

namespace outshine::Generators {
namespace {
BridgeGroundError Error(BridgeGroundErrorCode code, std::optional<uint64_t> node = {}) {
  return {.Code = code, .Node = node};
}

bool ValidNode(const World::TransportNode &node) {
  return std::isfinite(node.LatitudeDeg) && std::isfinite(node.LongitudeDeg) &&
         std::abs(node.LatitudeDeg) <= kDegPerHalfTurn * 0.5 &&
         std::abs(node.LongitudeDeg) <= kDegPerHalfTurn;
}

std::expected<std::variant<BridgeGroundGap, BridgeGroundValue>, BridgeGroundError>
Sample(const World::TransportNode &node,
       const Ground::HeightField *terrain,
       BridgeGroundLimits limits,
       BridgeGroundWork &work) {
  if (terrain == nullptr) { return BridgeGroundGap::MissingTerrain; }
  ++work.TileClassifications;
  const auto tile = Ground::TileIndex::Of(
      {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg}, terrain->Zoom());
  if (tile.Where() != Ground::TileIndex::State::Inside) { return BridgeGroundGap::OutsideCoverage; }
  ++work.HeightLookups;
  const auto located =
      terrain->SampleAt({.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg});
  if (!located) { return BridgeGroundGap::MissingTerrain; }
  const auto &block = terrain->Blocks()[located->BlockIndex];
  if (terrain->Fallback()) { return BridgeGroundGap::FallbackTerrain; }
  if (block.MissingBoundary) { return BridgeGroundGap::PartialBoundary; }
  if (block.Sources.size() > limits.SourcesPerBlockMost) {
    return std::unexpected(Error(BridgeGroundErrorCode::CapacityExceeded, node.SourceNodeId));
  }
  if (block.Sources.empty()) { return BridgeGroundGap::MissingProvenance; }
  for (const auto &source : block.Sources) {
    ++work.SourceReferenceChecks;
    if (source.Kind != Data::DataKind::Elevation || source.SourceId.empty() ||
        source.Revision.empty()) {
      return BridgeGroundGap::MissingProvenance;
    }
  }
  const auto elevation = located->Height.AslM();
  if (!elevation) { return BridgeGroundGap::InvalidHeight; }
  return BridgeGroundValue{.AslM = *elevation, .TerrainBlock = located->BlockIndex};
}
}

std::expected<BridgeGroundSamples, BridgeGroundError>
SampleBridgeGround(const BridgeInputRequirements &inputs,
                   std::shared_ptr<const Ground::HeightField> terrain,
                   BridgeGroundLimits limits,
                   const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error(BridgeGroundErrorCode::Canceled)); }
  const auto &nodes = inputs.GroundNodes;
  const size_t capacity = limits.ContainerCapacityMost / sizeof(BridgeGroundSample);
  if (nodes.size() > limits.NodesMost || nodes.size() > capacity ||
      (!nodes.empty() && terrain && terrain->Blocks().size() > limits.TerrainBlocksMost)) {
    return std::unexpected(Error(BridgeGroundErrorCode::CapacityExceeded));
  }
  BridgeGroundSamples result{.Source = inputs.Source, .Terrain = std::move(terrain), .Samples = {}};
  if (nodes.empty()) { return result; }
  if (!result.Source) { return std::unexpected(Error(BridgeGroundErrorCode::MissingSource)); }
  if (result.Source->SourceIdentity().DatasetId.empty() ||
      result.Source->SourceIdentity().Revision.empty()) {
    return std::unexpected(Error(BridgeGroundErrorCode::InvalidSource));
  }
  for (size_t at = 1; at < nodes.size(); ++at) {
    if (nodes[at - 1] >= nodes[at]) {
      return std::unexpected(Error(BridgeGroundErrorCode::InvalidSelection, nodes[at]));
    }
  }
  result.Samples.reserve(nodes.size());
  if (result.Samples.capacity() > capacity) {
    return std::unexpected(Error(BridgeGroundErrorCode::CapacityExceeded));
  }
  result.Work.ContainerCapacityBytes = result.Samples.capacity() * sizeof(BridgeGroundSample);
  for (const uint64_t id : nodes) {
    if (stop.stop_requested()) { return std::unexpected(Error(BridgeGroundErrorCode::Canceled)); }
    ++result.Work.NodeLookups;
    const auto *node = result.Source->Topology().FindNode(id);
    if (node == nullptr || !ValidNode(*node)) {
      return std::unexpected(Error(BridgeGroundErrorCode::InvalidSource, id));
    }
    const auto sampled = Sample(*node, result.Terrain.get(), limits, result.Work);
    if (!sampled) { return std::unexpected(sampled.error()); }
    const bool sourced = std::holds_alternative<BridgeGroundValue>(*sampled);
    result.Work.Sourced += sourced ? 1u : 0u;
    result.Work.Unresolved += sourced ? 0u : 1u;
    result.Samples.push_back({.SourceNode = id, .Value = *sampled});
  }
  return result;
}
}
