#ifndef OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEBODYINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_SURFACE_SURFACEBODYINTERNAL_H

#include "SurfaceBody.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

namespace outshine::Generators::SurfaceBodies {
using Error = SurfaceBodyPlanError;
template <typename T> using Checked = std::expected<T, SurfaceBodyError>;
inline constexpr uint32_t kNone = std::numeric_limits<uint32_t>::max();

struct Flight {
  PlanarPoint Origin{}, Tangent{};
  double Start = 0, End = 0;
  int Direction = 1;
};

enum class Pass : uint8_t { Envelope, Bands };

struct Groups {
  std::vector<PlanarFaceRegion> Regions;
  std::vector<uint32_t> Leaders, Passed;
};

struct State {
  const SurfaceHeightRequest &Request;
  std::span<const double> Heights;
  SurfaceBodyBudget Budget;
  const std::stop_token &Stop;
  SurfaceBody Output;
  std::vector<Flight> Flights;
  std::vector<uint32_t> SourceFlights;

  [[nodiscard]] Checked<void> Visit(size_t count = 1) {
    if (Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (count > Budget.Visits - Output.Work.Visits) {
      return std::unexpected(Error::CapacityExceeded);
    }
    Output.Work.Visits += count;
    return {};
  }
};

[[nodiscard]] Checked<void> Validate(const State &state);
[[nodiscard]] Checked<void> Prepare(State &state);
[[nodiscard]] Checked<void> RefreshRecipes(State &state);
[[nodiscard]] Checked<std::vector<HeightLevelRange>> AssignLevels(State &state);
[[nodiscard]] Checked<Groups> GroupFaces(State &state, Pass pass);
[[nodiscard]] Checked<void> Remesh(State &state, Pass pass);
void Compact(State &state);
}
#endif
