#ifndef OUTSHINE_ENGINE_SCENARIO_SCENARIORUNTIME_H
#define OUTSHINE_ENGINE_SCENARIO_SCENARIORUNTIME_H

#include <optional>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <cstdint>
#include <vector>
#include "EntityState.h"
#include <Scenario.h>
#include "Assembled.h"
#include "Column.h"
#include "EntityBodyState.h"
#include "Vec3.h"
#include "Tables.h"
#include "Traits.h"
#include "Triggers.h"

namespace outshine {

struct ScenarioRuntime {
  ScenarioRuntime() = default;
  ScenarioRuntime(const ScenarioRuntime &) = delete;
  ScenarioRuntime &operator=(const ScenarioRuntime &) = delete;
  ScenarioRuntime(ScenarioRuntime &&) = delete;
  ScenarioRuntime &operator=(ScenarioRuntime &&) = delete;
  ~ScenarioRuntime() = default;

  [[nodiscard]] std::expected<std::vector<std::optional<Entity>>, std::string>
  BindAudio(std::span<const Audio::SoundSource> sounds) const;
  [[nodiscard]] std::expected<std::vector<std::optional<Entity>>, std::string>
  BindBodies(std::span<const std::string_view> names) const;
  [[nodiscard]] std::expected<void, std::string> PrepareBodies();

  uint64_t DeclarationRevision = 0;
  World::EntityState Entities;
  Column<Scenario::Body> Definitions;
  Column<Traits> Attributes;
  Assembled Stood;
  std::optional<TableBook> Tables;
  std::optional<TriggerField> Triggers;
  std::vector<std::optional<Entity>> ViewBodies;
};

}
#endif
