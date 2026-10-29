#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLAN_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLAN_H
#include "GroundPortBodies.h"
#include "GroundPortPlanNetwork.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <variant>

namespace outshine::Generators {
struct GroundPortPlanPathRecipe {
  uint64_t PathId = 0;
  std::string Material;
  TransportUse Use = TransportUse::Road;
  double SpeedMps = 0;
  int32_t Priority = 0;
};

struct GroundPortPlanBudget {
  GroundPortBodyBudget Bodies{};
  GroundPortFieldQueryLimits Samples{};
  size_t Paths = 2048;
  size_t Ports = 8192;
  size_t RecipePaths = 16384;
  size_t BindingBytes = 1048576;
  size_t OutputCapacityBytes = 67108864;
};

struct GroundPortPlanWork {
  TransportCoreWork Core{};
  GroundPortBodyWork Bodies{};
  GroundPortFieldQueryWork Samples{};
  size_t Paths = 0;
  size_t Ports = 0;
  size_t MovedVertices = 0;
  size_t MovedContacts = 0;
  size_t OutputCapacityBytes = 0;
};

struct GroundPortPlanProduct {
  TransportPlan Plan;
  Box BoundsEcef;
  GroundPortPlanWork Work;
};

enum class GroundPortPlanInputError : uint8_t {
  InvalidRecipe,
  InvalidOmissions,
  EmptyCore,
  CapacityExceeded,
  Canceled
};

using GroundPortPlanError = std::variant<GroundPortPlanInputError,
                                         RoadGroundError,
                                         HeightSolidError,
                                         SurfaceEarthworkError,
                                         GroundPortSourceError,
                                         GroundPortTerrainError,
                                         RoadProfiles::HeightFitError,
                                         RoadPortQueryError,
                                         TransportPlanCodecError,
                                         TransportCoreError,
                                         TransportPlanNetworkError>;

[[nodiscard]] std::expected<GroundPortPlanProduct, GroundPortPlanError>
BuildGroundPortPartialPlan(const GroundPortField &field,
                           const GroundPortPlanNetwork &network,
                           const GroundPortBodies &wholeBodies,
                           GeoCellId core,
                           std::span<const GroundPortPlanPathRecipe> recipes,
                           std::span<const TransportOmission> omissions,
                           GroundPortPlanBudget budget = {},
                           const std::stop_token &stop = {});
}
#endif
