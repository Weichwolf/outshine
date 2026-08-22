#include <outshine/Outshine.h>

#include <cstdio>
#include <vector>

#include "Assembly.h"
#include "Live.h"
#include "Renderer.h"
#include "ScenarioRead.h"

namespace outshine {

struct Engine::State {
  Render::Renderer Device;
  std::unique_ptr<Clients::Live> Standing;
  Extent Frame{1280, 720};
  Scenario Declared;
  std::vector<std::string> Carried;
  std::vector<Scenario> Asleep;
  Store Scene;
  Column<Vehicle> Vehicles;
  Column<Drive> Drives;
  Assembled Stood;
  std::string Error;
};

namespace {

std::vector<std::string> Unacted(const Scenario &scenario) {
  std::vector<std::string> carried;
  const auto note = [&carried](size_t many, const char *what) {
    if (many > 0) { carried.push_back(std::to_string(many) + " " + what); }
  };
  note(scenario.Layers.size(), "layers");
  note(scenario.Providers.size(), "providers");
  note(scenario.Generators.size(), "generators");
  note(scenario.Compositors.size(), "compositors");
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
  note(scenario.Views.size(), "views");
  note(scenario.Vehicles.size(), "vehicles");
  if (!scenario.Played.Is.empty()) { carried.push_back("a player"); }
  note(scenario.Input.size(), "input bindings");
  note(scenario.State.size(), "persisted values");
  if (scenario.Ground.Declared) { carried.push_back("a world origin"); }
  if (!scenario.Motion.Dial.empty()) { carried.push_back("a physics dial"); }
  if (!scenario.Time.Start.empty()) { carried.push_back("a clock"); }
  if (scenario.Assets.size() > 1) {
    carried.push_back(std::to_string(scenario.Assets.size() - 1) + " assets beside the subject");
  }
  return carried;
}

} // namespace

Engine::Engine() : S_(std::make_unique<State>()) {}

bool Engine::Assemble() {
  const Scenario &declared = S_->Declared;
  const size_t named = declared.Vehicles.size() + (declared.Played.Is.empty() ? 0u : 1u) +
                       (declared.Driven.Declared ? 2u : 0u);
  if (named == 0) {
    S_->Error = "the declaration names nothing to assemble";
    return false;
  }
  if (!S_->Scene.Open(named) || !S_->Vehicles.Open(S_->Scene) ||
      !S_->Drives.Open(S_->Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return false;
  }
  return outshine::Assemble(declared, S_->Scene, S_->Vehicles, S_->Drives, S_->Stood, S_->Error);
}

Store &Engine::Scene(void) { return S_->Scene; }
const Store &Engine::Scene(void) const { return S_->Scene; }
Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

void Engine::RenderTo(Extent frame) { S_->Frame = frame; }

bool Engine::Declare(const Scenario &scenario) {
  const Asset *const subject = scenario.Subject();
  if (subject == nullptr) {
    S_->Error = "a scenario stands up an asset of kind 'gltf' and this one declares none";
    return false;
  }

  Clients::Declaration declared;
  declared.SurfaceWidthPx =
      scenario.Render.Frame.WidthPx > 0 ? scenario.Render.Frame.WidthPx : S_->Frame.WidthPx;
  declared.SurfaceHeightPx =
      scenario.Render.Frame.HeightPx > 0 ? scenario.Render.Frame.HeightPx : S_->Frame.HeightPx;
  declared.Stands = subject->Uri;
  declared.Variant = subject->Variant;
  declared.Fps = scenario.Render.Fps;
  declared.Fill = scenario.Render.Fill;
  declared.OrbitDegPerFrame = scenario.Render.OrbitDegPerFrame;
  declared.KeyLux = scenario.Lit.Key.Lux;
  declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;
  declared.KeyBearingDeg = scenario.Lit.Key.BearingDeg;
  for (int at = 0; at < 3; ++at) { declared.Environment[at] = scenario.Lit.Environment[at]; }

  S_->Standing.reset();
  if (!Clients::Live::Open(S_->Device, std::move(declared), nullptr, S_->Standing, S_->Error)) {
    S_->Standing.reset();
    return false;
  }
  S_->Declared = scenario;
  S_->Carried = Unacted(scenario);
  S_->Error.clear();
  return true;
}

bool Engine::Read(const std::string &path) {
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    S_->Error = path + ": no scenario at that path";
    return false;
  }
  std::string text;
  char block[4096];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { text.append(block, read); }
  std::fclose(file);

  Scenario scenario;
  scenario.Render.Frame = S_->Frame;
  if (!ReadScenario(text.c_str(), text.size(), scenario, S_->Error)) {
    S_->Error = path + ": " + S_->Error;
    return false;
  }
  S_->Declared = scenario;
  S_->Carried = Unacted(scenario);
  S_->Error.clear();
  return true;
}

bool Engine::Load(const std::string &path) {
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) {
    S_->Error = path + ": no scenario at that path";
    return false;
  }
  std::string text;
  char block[4096];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { text.append(block, read); }
  std::fclose(file);

  Scenario scenario;
  scenario.Render.Frame = S_->Frame;
  if (!ReadScenario(text.c_str(), text.size(), scenario, S_->Error)) {
    S_->Error = path + ": " + S_->Error;
    return false;
  }
  return Declare(scenario);
}

const Scenario &Engine::Declared(void) const { return S_->Declared; }
const std::vector<std::string> &Engine::Carried(void) const { return S_->Carried; }

bool Engine::Advance() {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to advance";
    return false;
  }
  return S_->Standing->Advance(S_->Error);
}

bool Engine::Run() {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to run";
    return false;
  }
  for (int frame = 0; frame < S_->Standing->Frames(); ++frame) {
    if (!S_->Standing->Advance(S_->Error)) { return false; }
  }
  return true;
}

bool Engine::Park(void) {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to park";
    return false;
  }
  if (S_->Declared.Named.Name.empty()) {
    S_->Error = "a scenario is parked under its name and this one declares none";
    return false;
  }
  for (const Scenario &asleep : S_->Asleep) {
    if (asleep.Named.Name == S_->Declared.Named.Name) {
      S_->Error = S_->Declared.Named.Name + " is parked already, so parking it twice would leave two";
      return false;
    }
  }
  S_->Asleep.push_back(S_->Declared);
  S_->Standing.reset();
  S_->Error.clear();
  return true;
}

bool Engine::Resume(const std::string &name) {
  for (size_t at = 0; at < S_->Asleep.size(); ++at) {
    if (S_->Asleep[at].Named.Name != name) { continue; }
    const Scenario taken = S_->Asleep[at];
    S_->Asleep.erase(S_->Asleep.begin() + (long)at);
    return Declare(taken);
  }
  S_->Error = name + " is not parked, and resuming what was never parked is not a load";
  return false;
}

std::vector<std::string> Engine::Parked(void) const {
  std::vector<std::string> names;
  for (const Scenario &asleep : S_->Asleep) { names.push_back(asleep.Named.Name); }
  return names;
}

int Engine::At(void) const { return S_->Standing ? S_->Standing->At() : 0; }
int Engine::Frames(void) const { return S_->Standing ? S_->Standing->Frames() : 0; }
bool Engine::Standing(void) const { return S_->Standing != nullptr; }
const std::string &Engine::Error(void) const { return S_->Error; }

} // namespace outshine
