#ifndef OUTSHINE_ENGINE_SIMULATIONSTATE_H
#define OUTSHINE_ENGINE_SIMULATIONSTATE_H

#include <optional>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <world/EntityRegistry.h>
#include <scenario/Scenario.h>
#include "Assembled.h"
#include "Column.h"
#include "Rigid.h"
#include "math/Vec3.h"
#include "Tables.h"
#include "Traits.h"
#include "Triggers.h"

namespace outshine {

struct SimulatedBody {
  Entity Owner = kNoEntity;
  Physics::Rigid Motion;
};

struct SimulationState {
  SimulationState() = default;
  SimulationState(const SimulationState &) = delete;
  SimulationState &operator=(const SimulationState &) = delete;
  SimulationState(SimulationState &&) = delete;
  SimulationState &operator=(SimulationState &&) = delete;
  ~SimulationState() = default;

  [[nodiscard]] std::expected<std::vector<std::optional<size_t>>, std::string>
  BindAudio(std::span<const Scenario::Sound> sounds) const;
  [[nodiscard]] std::expected<std::vector<std::optional<size_t>>, std::string>
  BindBodies(std::span<const std::string_view> names) const;
  void PrepareBodies();
  void Integrate(double stepSeconds, const Vec3 &gravityMs2);

  uint64_t DeclarationRevision = 0;
  EntityRegistry Entities;
  Column<Scenario::Body> Bodies;
  Column<Traits> Kinds;
  Assembled Stood;
  std::optional<TableBook> Tables;
  std::optional<TriggerField> Triggers;
  std::vector<SimulatedBody> DynamicBodies;
  std::vector<std::optional<size_t>> ViewBodies;
};

}
#endif
