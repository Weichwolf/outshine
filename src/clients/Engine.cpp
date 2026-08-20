#include <outshine/Outshine.h>

#include <cstdio>
#include <vector>

#include "Json.h"
#include "Live.h"
#include "Renderer.h"

namespace outshine {

struct Engine::State {
  Render::Renderer Device;
  std::unique_ptr<Clients::Live> Standing;
  Extent Frame{1280, 720};
  std::string Error;
};

Engine::Engine() : S_(std::make_unique<State>()) {}
Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

void Engine::RenderTo(Extent frame) { S_->Frame = frame; }

bool Engine::Declare(const Scenario &scenario) {
  Clients::Declaration declared;
  declared.SurfaceWidthPx = scenario.Frame.WidthPx > 0 ? scenario.Frame.WidthPx : S_->Frame.WidthPx;
  declared.SurfaceHeightPx =
      scenario.Frame.HeightPx > 0 ? scenario.Frame.HeightPx : S_->Frame.HeightPx;
  declared.Stands = scenario.Stands;
  declared.Variant = scenario.Variant;
  declared.Fps = scenario.Fps;
  declared.Fill = scenario.Fill;
  declared.OrbitDegPerFrame = scenario.OrbitDegPerFrame;
  declared.KeyLux = scenario.Key.Lux;
  declared.KeyElevationDeg = scenario.Key.ElevationDeg;
  declared.KeyBearingDeg = scenario.Key.BearingDeg;
  for (int at = 0; at < 3; ++at) { declared.Environment[at] = scenario.Environment[at]; }

  S_->Standing.reset();
  if (!Clients::Live::Open(S_->Device, std::move(declared), nullptr, S_->Standing, S_->Error)) {
    S_->Standing.reset();
    return false;
  }
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

  Json declaration;
  if (!declaration.Parse(text.c_str(), text.size())) {
    S_->Error = path + ": the scenario does not parse as JSON";
    return false;
  }
  const Json::Ref root = declaration.Root();
  if (root["schema"].Str("") != "outshine/scenario") {
    S_->Error = path + ": a scenario declares schema 'outshine/scenario'";
    return false;
  }

  Scenario scenario;
  scenario.Frame.WidthPx = (int)root["frame"]["widthPx"].Int(S_->Frame.WidthPx);
  scenario.Frame.HeightPx = (int)root["frame"]["heightPx"].Int(S_->Frame.HeightPx);
  scenario.Stands = root["stands"].Str("");
  scenario.Variant = root["variant"].Str("");
  scenario.Fps = root["fps"].Num(60.0);
  scenario.Fill = root["fill"].Num(0.9);
  scenario.OrbitDegPerFrame = root["orbitDegPerFrame"].Num(0.0);
  scenario.Key.Lux = root["key"]["lux"].Num(0.0);
  scenario.Key.ElevationDeg = root["key"]["elevationDeg"].Num(0.0);
  scenario.Key.BearingDeg = root["key"]["bearingDeg"].Num(0.0);
  const Json::Ref environment = root["environment"];
  for (size_t at = 0; at < 3 && at < environment.Size(); ++at) {
    scenario.Environment[at] = environment[at].Num(0.0);
  }
  if (scenario.Stands.empty()) {
    S_->Error = path + ": a scenario declares what it stands up";
    return false;
  }
  return Declare(scenario);
}

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
