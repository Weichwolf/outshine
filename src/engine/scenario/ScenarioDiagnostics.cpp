#include "ScenarioDiagnostics.h"
#include "Scenario.h"
#include <cstddef>
#include <string>
#include <vector>

namespace outshine {

std::vector<std::string> DescribeScenarioDeclarations(const Scenario::Document &scenario) {
  std::vector<std::string> quiet;
  for (const Scenario::Asset &asset : scenario.Assets) {
    if (asset.Animation == Scenario::AssetAnimation::Ignore) {
      quiet.push_back("asset '" + asset.Uri +
                      "': its own animation is IGNORED by declaration -- a still is what was "
                      "asked for, not what the engine fell back to");
    } else if (asset.Animation == Scenario::AssetAnimation::Driven) {
      quiet.push_back("asset '" + asset.Uri +
                      "': its own animation is DRIVEN by the engine -- the file's clips wait "
                      "for the pose the simulation supplies");
    }
  }
  std::vector<std::string> carried;
  const auto note = [&carried](size_t many, const char *what) {
    if (many > 0) { carried.push_back(std::to_string(many) + " " + what); }
  };
  note(scenario.Layers.size(), "layers");
  note(scenario.Generators.size(), "generators");
  note(scenario.Placements.size(), "placements");
  note(scenario.Surfaces.size(), "surfaces");
  note(scenario.Kinds.size(), "kinds");
  note(scenario.Instances.size(), "instances");
  note(scenario.Regions.size(), "regions");
  note(scenario.Doors.size(), "doors");
  note(scenario.Volumes.size(), "trigger volumes");
  note(scenario.Sounds.size(), "sounds");
  note(scenario.Buses.size(), "audio buses");
  note(scenario.Tables.size(), "tables");
  note(scenario.Events.size(), "declared events");
  note(scenario.Bodies.size(), "bodies");
  note(scenario.State.size(), "persisted values");
  if (scenario.Motion.Declared) { carried.emplace_back("a physics dial"); }
  if (scenario.Time.Declared) { carried.emplace_back("a clock"); }
  if (scenario.Assets.size() > 1) {
    carried.push_back(std::to_string(scenario.Assets.size() - 1) + " assets beside the geometry");
  }
  carried.insert(carried.end(), quiet.begin(), quiet.end());
  return carried;
}

}
