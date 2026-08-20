#include <outshine/Outshine.h>

#include <cstdio>
#include <vector>

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
  note(scenario.Actors.size(), "actors");
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

int Engine::At(void) const { return S_->Standing ? S_->Standing->At() : 0; }
int Engine::Frames(void) const { return S_->Standing ? S_->Standing->Frames() : 0; }
bool Engine::Standing(void) const { return S_->Standing != nullptr; }
const std::string &Engine::Error(void) const { return S_->Error; }

} // namespace outshine
