#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTFIELD_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTFIELD_H

#include "GroundPortSources.h"
#include "GroundPortTerrain.h"
#include "RoadGroundFit.h"
#include "RoadGroundMesh.h"
#include "RoadPortFrame.h"
#include "TransportPlanCodec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Generators {
using GroundPortFieldError = std::variant<GroundPortSourceError,
                                          GroundPortTerrainError,
                                          RoadGroundError,
                                          RoadProfiles::HeightFitError,
                                          RoadPortQueryError,
                                          TransportPlanCodecError>;

class GroundPortFieldRequest {
public:
  GroundPortFieldRequest(GroundPortFieldRequest &&) = default;
  GroundPortFieldRequest &operator=(GroundPortFieldRequest &&) = default;
  GroundPortFieldRequest(const GroundPortFieldRequest &) = delete;
  GroundPortFieldRequest &operator=(const GroundPortFieldRequest &) = delete;

  [[nodiscard]] static std::expected<GroundPortFieldRequest, GroundPortFieldError>
  Prepare(const GroundPortSources &sources,
          World::TransportEdgeId seed,
          GroundPortChainLimits chainLimits = {},
          RoadGroundBudget groundBudget = {},
          const std::stop_token &stop = {});

  [[nodiscard]] std::span<const LongitudeLatitude> TerrainQueries() const noexcept {
    return Strip_.TerrainQueries;
  }

  [[nodiscard]] const GroundPortSourceChain &Source() const noexcept { return Source_; }

private:
  friend class GroundPortField;
  GroundPortFieldRequest(GroundPortSourceChain source, RoadGroundStrip strip);
  GroundPortSourceChain Source_;
  RoadGroundStrip Strip_;
};

struct GroundPortFieldBudget {
  RoadGroundBudget Ground{};
  RoadProfiles::HeightFitBudget Fit{};
  RoadGroundMeshBudget Mesh{};
  size_t BindingBytes = 65536;
};

struct GroundPortFieldRequestPoint {
  World::TransportEdgeId Edge;
  LongitudeLatitude Anchor;
};

struct GroundPortFieldQueryLimits {
  size_t Ports = 1024;
  size_t ContainerBytes = 1048576;
  RoadPortQueryBudget Field{};
};

struct GroundPortFieldQueryWork {
  size_t ValidatedFaces = 0;
  size_t MembershipLookups = 0;
  size_t TerrainSamples = 0;
  size_t ContainerCapacityBytes = 0;
  RoadPortQueryWork Field{};
};

struct GroundPortFieldSamples {
  std::vector<RoadPortFrameSample> Ports;
  GroundPortFieldQueryWork Work;
};

class GroundPortField {
public:
  GroundPortField(GroundPortField &&) = default;
  GroundPortField &operator=(GroundPortField &&) = default;
  GroundPortField(const GroundPortField &) = delete;
  GroundPortField &operator=(const GroundPortField &) = delete;

  [[nodiscard]] static std::expected<GroundPortField, GroundPortFieldError>
  Make(GroundPortFieldRequest request,
       std::shared_ptr<const GroundPortTerrain> terrain,
       double maximumPriorChangeM,
       GroundPortFieldBudget budget = {},
       const std::stop_token &stop = {});

  [[nodiscard]] const GroundPortSourceChain &Source() const noexcept { return Data_.Source; }

  [[nodiscard]] const RoadGroundField &Domain() const noexcept { return Data_.Domain; }

  [[nodiscard]] const RoadGroundFit &Fit() const noexcept { return Data_.Fit; }

  [[nodiscard]] const RoadGroundMesh &Geometry() const noexcept { return Data_.Geometry; }

  [[nodiscard]] const GroundPortTerrain &Terrain() const noexcept { return *Data_.Terrain; }

  [[nodiscard]] const std::array<uint8_t, 32> &ProcedureAndTerrainBinding() const noexcept {
    return Data_.ProcedureAndTerrainBinding;
  }

  [[nodiscard]] std::expected<GroundPortFieldSamples, GroundPortFieldError>
  Sample(std::span<const GroundPortFieldRequestPoint> points,
         GroundPortFieldQueryLimits limits = {},
         const std::stop_token &stop = {}) const;

private:
  struct Data {
    GroundPortSourceChain Source;
    std::shared_ptr<const GroundPortTerrain> Terrain;
    RoadGroundField Domain;
    RoadGroundFit Fit;
    RoadGroundMesh Geometry;
    std::array<uint8_t, 32> ProcedureAndTerrainBinding;
    std::vector<uint32_t> Members;
  };

  explicit GroundPortField(Data data);
  Data Data_;
};
}
#endif
