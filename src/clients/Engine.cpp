#include <outshine/Outshine.h>

#include <cstdio>
#include <vector>

#include "Assembly.h"
#include "ScenarioLayer.h"
#include "Live.h"
#include "Renderer.h"
#include "ScenarioRead.h"

namespace outshine {

inline constexpr size_t kParkedBound = 8; // [SET] eight doorways deep covers a building of
                                          // interiors; the state kept per park is the
                                          // declaration alone, whose measure the four-lines
                                          // proof publishes


struct Engine::State {
  Render::Renderer Device;
  std::unique_ptr<Clients::Live> Standing;
  Extent Frame{1280, 720};
  Scenario Declared;
  std::vector<std::string> Carried;
  std::vector<Scenario> Asleep;
  std::vector<std::string> LayerTrace;
  Store Scene;
  Column<Vehicle> Vehicles;
  Column<Drive> Drives;
  Column<Traits> Kinds;
  Assembled Stood;
  std::string Error;
};

namespace {

std::vector<std::string> Unacted(const Scenario &scenario) {
  std::vector<std::string> quiet;
  for (const Asset &asset : scenario.Assets) {
    if (asset.Animation == AssetAnimation::Ignore) {
      quiet.push_back("asset '" + asset.Uri +
                      "': its own animation is IGNORED by declaration -- a still is what was "
                      "asked for, not what the engine fell back to");
    } else if (asset.Animation == AssetAnimation::Driven) {
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
  if (scenario.Played.Declared) { carried.push_back("a player"); }
  note(scenario.Input.size(), "input bindings");
  note(scenario.State.size(), "persisted values");
  if (scenario.Ground.Declared) { carried.push_back("a world origin"); }
  if (scenario.Motion.Declared) { carried.push_back("a physics dial"); }
  if (scenario.Time.Declared) { carried.push_back("a clock"); }
  if (scenario.Assets.size() > 1) {
    carried.push_back(std::to_string(scenario.Assets.size() - 1) + " assets beside the subject");
  }
  carried.insert(carried.end(), quiet.begin(), quiet.end());
  return carried;
}

} // namespace

Engine::Engine() : S_(std::make_unique<State>()) {}

bool Engine::Assemble() {
  const Scenario &declared = S_->Declared;
  const size_t named = AssembledCapacity(declared);
  if (named == 0) {
    S_->Error = "the declaration names nothing to assemble";
    return false;
  }
  if (!S_->Scene.Open(named) || !S_->Vehicles.Open(S_->Scene) ||
      !S_->Drives.Open(S_->Scene) || !S_->Kinds.Open(S_->Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return false;
  }
  return outshine::Assemble(declared, S_->Scene, S_->Vehicles, S_->Drives, S_->Kinds,
                            S_->Stood, S_->Error);
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
  declared.Animation = subject->Animation;
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

namespace {

[[nodiscard]] bool SlurpFile(const std::string &held, std::string &text, std::string &error) {
  std::FILE *const file = std::fopen(held.c_str(), "rb");
  if (file == nullptr) {
    error = held + ": no scenario at that path";
    return false;
  }
  text.clear();
  char block[4096];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { text.append(block, read); }
  std::fclose(file);
  return true;
}

} // namespace

bool Engine::ReadInto(std::string_view path, Scenario &out) {
  const std::string held(path);
  std::string text;
  if (!SlurpFile(held, text, S_->Error)) { return false; }

  out.Render.Frame = S_->Frame;
  if (!ReadScenario(text.c_str(), text.size(), out, S_->Error)) {
    S_->Error = held + ": " + S_->Error;
    return false;
  }

  S_->LayerTrace.clear();
  if (out.Layers.empty()) { return true; }
  const size_t cut = held.find_last_of('/');
  const std::string dir = cut == std::string::npos ? std::string() : held.substr(0, cut + 1);
  for (const Layer &layer : out.Layers) {
    const std::string named = layer.Id.empty() ? layer.Path : layer.Id;
    if (!LayerActive(layer, out.Named.Active)) {
      S_->LayerTrace.push_back("layer '" + named + "' is inactive -- its set '" + layer.Set +
                               "' is not selected by active=\"" + out.Named.Active + "\"");
      continue;
    }
    const std::string at =
        (!layer.Path.empty() && layer.Path.front() == '/') ? layer.Path : dir + layer.Path;
    std::string fragmentText;
    if (!SlurpFile(at, fragmentText, S_->Error)) { return false; }
    if (!ApplyLayer(out, fragmentText.c_str(), fragmentText.size(), named, S_->LayerTrace,
                    S_->Error)) {
      S_->Error = at + ": " + S_->Error;
      return false;
    }
  }
  return true;
}

bool Engine::Read(std::string_view path) {
  Scenario scenario;
  if (!ReadInto(path, scenario)) { return false; }
  S_->Declared = scenario;
  S_->Carried = Unacted(scenario);
  S_->Carried.insert(S_->Carried.end(), S_->LayerTrace.begin(), S_->LayerTrace.end());
  S_->Error.clear();
  return true;
}

bool Engine::Load(std::string_view path) {
  Scenario scenario;
  if (!ReadInto(path, scenario)) { return false; }
  if (!Declare(scenario)) { return false; }
  S_->Carried.insert(S_->Carried.end(), S_->LayerTrace.begin(), S_->LayerTrace.end());
  return true;
}

const Scenario &Engine::Declared(void) const { return S_->Declared; }
const Assembled &Engine::Stood(void) const { return S_->Stood; }
const Column<Traits> &Engine::Resolved(void) const { return S_->Kinds; }
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
  // the parked set states its bound, and reaching it REFUSES: a park is the ONLY copy of
  // that scenario's state (no savefile stands beneath it -- board:1492 is not built), so
  // evicting would destroy what nobody chose to discard; "degrade on detail, refuse on
  // existence" decides this, and the refusal names the least recently live door to clear
  if (S_->Asleep.size() >= kParkedBound) {
    S_->Error = "the parked set is full at its declared bound of " +
                std::to_string(kParkedBound) + " -- resume or discard '" +
                S_->Asleep.front().Named.Name + "' (the least recently live) before parking " +
                S_->Declared.Named.Name;
    return false;
  }
  S_->Asleep.push_back(S_->Declared);
  S_->Standing.reset();
  S_->Error.clear();
  return true;
}

bool Engine::Resume(std::string_view name) {
  if (S_->Standing) {
    S_->Error = "a scenario is standing, and Resume stands nothing down -- park it or Discard "
                "it explicitly first, because state that vanishes on somebody else's call is "
                "state nobody can reason about";
    return false;
  }
  for (size_t at = 0; at < S_->Asleep.size(); ++at) {
    if (S_->Asleep[at].Named.Name != name) { continue; }
    // the parked copy is the ONLY copy: it leaves the set only after the stand-up succeeds
    if (!Declare(S_->Asleep[at])) { return false; }
    S_->Asleep.erase(S_->Asleep.begin() + (long)at);
    return true;
  }
  S_->Error = std::string(name) + " is not parked, and resuming what was never parked is not a load";
  return false;
}

bool Engine::Discard(std::string_view name) {
  for (size_t at = 0; at < S_->Asleep.size(); ++at) {
    if (S_->Asleep[at].Named.Name != name) { continue; }
    S_->Asleep.erase(S_->Asleep.begin() + (long)at);
    S_->Error.clear();
    return true;
  }
  S_->Error = std::string(name) + " is not parked, so there is nothing to discard";
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
