#ifndef OUTSHINE_ENGINE_SIMULATIONSTATE_H
#define OUTSHINE_ENGINE_SIMULATIONSTATE_H

#include <optional>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include <EntityRegistry.h>
#include <Scenario.h>
#include "Assembled.h"
#include "Column.h"
#include "EntityBodyState.h"
#include "Vec3.h"
#include "Tables.h"
#include "Traits.h"
#include "Triggers.h"

namespace outshine {

struct SimulationState {
  SimulationState() = default;
  SimulationState(const SimulationState &) = delete;
  SimulationState &operator=(const SimulationState &) = delete;
  SimulationState(SimulationState &&) = delete;
  SimulationState &operator=(SimulationState &&) = delete;
  ~SimulationState() = default;

  [[nodiscard]] std::expected<std::vector<std::optional<size_t>>, std::string>
  BindAudio(std::span<const Audio::SoundSource> sounds) const;
  [[nodiscard]] std::expected<std::vector<std::optional<size_t>>, std::string>
  BindBodies(std::span<const std::string_view> names) const;
  void PrepareBodies();

  uint64_t DeclarationRevision = 0;
  EntityRegistry Entities;
  Column<Scenario::Body> Bodies;
  Column<Traits> Kinds;
  Assembled Stood;
  std::optional<TableBook> Tables;
  std::optional<TriggerField> Triggers;
  std::vector<World::EntityBodyState> DynamicBodies;
  std::vector<std::optional<size_t>> ViewBodies;
};

}
#endif
