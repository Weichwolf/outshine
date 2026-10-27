#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODIES_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODIES_H

#include "GroundPortField.h"
#include "HeightSolid.h"
#include "HeightSolidTerrainContact.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Generators {
struct GroundPortBody {
  World::TransportEdgeId Source;
  HeightSolid Geometry;
  std::vector<uint32_t> FaceParents;
  std::vector<EarthworkStamp> TerrainContacts;
};

struct GroundPortBodyRecipe {
  double ThicknessM = 0;
  double ApronM = 0;
  double ReliefM = 0;
  bool operator==(const GroundPortBodyRecipe &) const = default;
};

struct GroundPortBodyBudget {
  size_t Bodies = 2048;
  size_t SurfaceFaces = 16384;
  size_t ContainerBytes = 33554432;
  HeightSolidBudget Solid{};
};

struct GroundPortBodyWork {
  size_t FaceVisits = 0;
  size_t RemappedVertices = 0;
  size_t CopiedSurfaceBytes = 0;
  size_t ContainerCapacityBytes = 0;
  size_t ScratchCapacityBytes = 0;
  size_t SolidEdgeVisits = 0;
  size_t TerrainContactFaces = 0;
  size_t ContainerBoundBytes = 0;
  size_t ReframedVertices = 0;
  size_t ReframedTerrainContacts = 0;
};

struct GroundPortBodies {
  LongitudeLatitude Anchor;
  std::shared_ptr<const World::TransportNetworkSnapshot> Source;
  std::array<TransportInputReference, 2> Inputs;
  std::array<uint8_t, 32> ProcedureAndTerrainBinding;
  GroundPortBodyRecipe Recipe;
  std::optional<GroundPortFieldScopeInfo> Scope;
  std::vector<GroundPortBody> Bodies;
  GroundPortBodyWork Work;
};

using GroundPortBodyError = std::variant<RoadGroundError, HeightSolidError, SurfaceEarthworkError>;

[[nodiscard]] size_t GroundPortBodyCapacityBytes(const GroundPortBodies &bodies) noexcept;

[[nodiscard]] std::expected<GroundPortBodies, GroundPortBodyError>
ReframeGroundPortBodies(GroundPortBodies &&input,
                        LongitudeLatitude anchor,
                        GroundPortBodyBudget budget = {},
                        const std::stop_token &stop = {});

[[nodiscard]] std::expected<GroundPortBodies, GroundPortBodyError>
BuildGroundPortBodies(const GroundPortField &field,
                      GroundPortBodyRecipe recipe,
                      GroundPortBodyBudget budget = {},
                      const std::stop_token &stop = {});
}
#endif
