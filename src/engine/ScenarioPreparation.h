#ifndef OUTSHINE_ENGINE_SCENARIOPREPARATION_H
#define OUTSHINE_ENGINE_SCENARIOPREPARATION_H

#include "Engine.h"
#include "InputMap.h"
#include "Views.h"
#include "SourceSession.h"
#include "TriangleBvh.h"
#include "WorldCandidate.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace outshine {
struct EngineRenderState;
struct ScenarioSession;
struct ScenarioRuntime;
struct WorldRegionState;

struct HeadlessDeclaration {
  std::optional<Geometry> Geometry;
  std::optional<TriangleBvh> Occlusion;
};

struct PreparedRuntimeDeclaration {
  HeadlessDeclaration Headless;
  std::unique_ptr<Core::WorldCandidate> Targeted;

  [[nodiscard]] Result ValidateGroundView(const Scenario::Document &scenario, int heightPx) const;
  [[nodiscard]] Result
  Publish(EngineRenderState &picture, WorldRegionState &world, Core::Declaration declared);
};

struct ScenarioPreparation {
  Scenario::Document Declared;
  std::vector<std::string> Carried;
  Core::Declaration Rendering;
  std::optional<ViewBook> Views;
  InputMap Bindings;
  std::unique_ptr<ScenarioRuntime> Simulation;
  std::optional<Data::SourceSession::Configuration> Sources;
  std::optional<Render::Viewpoint> View;
  std::unique_ptr<WorldRegionState> World;
  PreparedRuntimeDeclaration Runtime;

  ScenarioPreparation();
  ~ScenarioPreparation();

  [[nodiscard]] std::optional<Result>
  TryReuse(EngineRenderState &picture, ScenarioSession &session, std::string &error);
  [[nodiscard]] Result Publish(EngineRenderState &picture,
                               ScenarioSession &session,
                               std::unique_ptr<WorldRegionState> &activeWorld);
};
}
#endif
