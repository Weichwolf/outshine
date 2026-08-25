#include <Outshine.h>
#include "Fetching.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <vector>

#include "Assembly.h"
#include "ScenarioLayer.h"
#include "Live.h"
#include "Sink.h"
#include "DeclaredSources.h"
#include "GroundStack.h"
#include "DriveAssembly.h"
#include "GroundPatchwork.h"
#include "Renderer.h"
#include "ScenarioRead.h"

namespace outshine {

inline constexpr size_t kParkedBound = 8;

namespace {

class Quietly : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool held, const char *why) override {
    if (!held && Why.empty()) { Why = why; }
  }
  void Near(double, double, double, const char *, const char *why) override {
    if (Why.empty()) { Why = why; }
  }
  void Say(const std::string &) override {}
  void Refuse(const std::string &why) override {
    if (Why.empty()) { Why = why; }
  }
  [[nodiscard]] const std::string &WhyNot() const { return Why; }

private:
  std::string Why;
};

constexpr double kTickS = 1.0 / 60.0;

[[nodiscard]] std::string Said(double value) {
  char held[32] = {};
  std::snprintf(held, sizeof held, "%.5f", value);
  return held;
}

[[nodiscard]] std::string Beneath(const std::string &under, const std::string &named) {
  if (under.empty() || named.empty() || named.front() == '/' ||
      named.find("://") != std::string::npos) {
    return named;
  }
  return under.back() == '/' ? under + named : under + "/" + named;
}

}

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
  Roots Under;
  std::unique_ptr<Fetching> Wire;
  size_t GroundTiles = 0;
  Ground::GroundStack Stack;
  Sim::DriveProduct Drive;
  bool Drove = false;
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

}

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
  if (!outshine::Assemble(declared, S_->Scene, S_->Vehicles, S_->Drives, S_->Kinds, S_->Stood,
                          S_->Error)) {
    return false;
  }

  S_->Drove = false;
  if (!declared.Driven.Declared) { return true; }
  if (!S_->Wire) {
    if (S_->Under.Offline) {
      S_->Error = "the scenario declares a drive from " + Said(declared.Driven.FromLatDeg) +
                  ", " + Said(declared.Driven.FromLonDeg) +
                  " and the engine was declared offline -- a route is FETCHED, so this is a "
                  "declaration it cannot execute";
      return false;
    }
    S_->Wire = std::make_unique<Fetching>(Fetching::Config{});
  }
  Quietly say;
  const Sim::Provision kept{S_->Under.Cache, S_->Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  if (!Sim::AssembleDrive(S_->Scene, S_->Stood, S_->Vehicles, S_->Drives, declared.Ground,
                          S_->Stack, *S_->Wire, kept, say, S_->Drive)) {
    S_->Error = say.WhyNot();
    return false;
  }
  S_->Drove = true;
  return true;
}

Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

void Engine::RenderTo(Extent frame) { S_->Frame = frame; }

bool Engine::ShowOn(SDL_Window *window) {
  if (window == nullptr) {
    S_->Error = "a surface is a window the client made, and this one is null";
    return false;
  }
  const auto shown = S_->Device.ShowOn(window);
  if (!shown) {
    S_->Error = std::string(shown.error());
    return false;
  }
  int width = 0, height = 0;
  if (SDL_GetWindowSizeInPixels(window, &width, &height)) {
    S_->Frame = Extent{width, height};
  }
  return true;
}

void Engine::Under(Roots roots) { S_->Under = std::move(roots); }

bool Engine::Drove(void) const { return S_->Drove; }

double Engine::ReachedM(void) const {
  return S_->Drove ? S_->Drive.State.Tally.ReachedM : 0.0;
}

double Engine::RouteM(void) const { return S_->Drove ? S_->Drive.Way.Line.LengthM() : 0.0; }

size_t Engine::GroundTiles(void) const { return S_->GroundTiles; }

bool Engine::Compose(void) {
  S_->GroundTiles = 0;
  if (!S_->Standing) {
    S_->Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario &declared = S_->Declared;
  if (!declared.Ground.Declared) {
    S_->Error = "the scenario declares no sphere, so there is no ground to compose";
    return false;
  }
  if (!S_->Wire) {
    if (S_->Under.Offline) {
      S_->Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    S_->Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Quietly say;
  if (!S_->Stack.Opened() &&
      !S_->Stack.Open(S_->Under.Cache, S_->Under.Shipped,
                      {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                      declared.Ground.Lat, declared.Ground.Lon, *S_->Wire, say)) {
    S_->Error = say.WhyNot();
    return false;
  }

  Around over;
  over.LatDeg = declared.Ground.Lat;
  over.LonDeg = declared.Ground.Lon;
  over.Zoom = S_->Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Ring = 1;
  const auto laid = LayPatchwork(S_->Stack.Pool(), over);
  if (!laid) {
    S_->Error = laid.error();
    return false;
  }

  Gltf::Piece ground;
  ground.NodeName = "ground";
  ground.Material = 0;
  ground.PositionsM = Span<const float>(laid->PositionM.data(), laid->PositionM.size());
  ground.Normals = Span<const float>(laid->NormalM.data(), laid->NormalM.size());
  ground.Indices = Span<const uint32_t>(laid->Index.data(), laid->Index.size());

  Gltf::Subject world;
  if (!world.Assemble(Gltf::Assembly{Span<const Gltf::Piece>(&ground, 1)})) {
    S_->Error = world.Error();
    return false;
  }
  if (!world.Append(S_->Standing->Shown())) {
    S_->Error = world.Error();
    return false;
  }
  if (!S_->Standing->Restand(world, S_->Error)) { return false; }
  S_->GroundTiles = laid->Tiles;
  return true;
}

const Roots &Engine::Under(void) const { return S_->Under; }

bool Engine::Capture(std::string_view path) {
  if (!S_->Standing) {
    S_->Error = "nothing stands to be captured -- a scenario is declared before a frame is kept";
    return false;
  }
  return S_->Standing->Screenshot(std::string(path), S_->Error);
}

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
  declared.Stands = Beneath(S_->Under.Assets, subject->Uri);
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

}

bool Engine::ReadInto(std::string_view path, Scenario &out) {
  const std::string held(path);
  std::string text;
  if (!SlurpFile(held, text, S_->Error)) { return false; }

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

const Scenario &Engine::Declared() const { return S_->Declared; }

inline constexpr size_t kMostSaveBytes = 1 << 20;

bool Engine::Save(std::string_view path) const {
  if (S_->Declared.State.empty()) {
    S_->Error = "the scenario declares nothing to persist, so a save would be an empty "
                "promise -- declare <state><persist what=.../></state> first";
    return false;
  }
  std::vector<std::string> lines;
  for (const Persisted &row : S_->Declared.State) {
    const size_t dot = row.What.find('.');
    if (dot == std::string::npos) {
      S_->Error = "the persist row '" + row.What +
                  "' names no instance.trait pair, and a save writes only what a load can "
                  "put back";
      return false;
    }
    const Entity holder = S_->Stood.InstanceNamed(std::string_view(row.What).substr(0, dot));
    const uint32_t key = S_->Stood.TraitKey(std::string_view(row.What).substr(dot + 1));
    const Traits *held = holder == kNoEntity ? nullptr : S_->Kinds.Get(holder);
    const double *value = held == nullptr || key == 0 ? nullptr : held->Named(key);
    if (value == nullptr) {
      S_->Error = "the persist row '" + row.What +
                  "' names nothing the assembled scene holds -- a save of a missing value "
                  "would load as a lie";
      return false;
    }
    char digits[64];
    const auto written = std::to_chars(digits, digits + sizeof digits, *value);
    lines.push_back(row.What + " " + std::string(digits, written.ptr) + "\n");
  }
  std::sort(lines.begin(), lines.end());
  std::string text = "outshine-save 1 " + S_->Declared.Named.Name + " " +
                     S_->Declared.Named.Version + "\n";
  for (const std::string &line : lines) { text += line; }
  if (text.size() > kMostSaveBytes) {
    S_->Error = "the save of " + std::to_string(text.size()) + " bytes overflows the bound of " +
                std::to_string(kMostSaveBytes);
    return false;
  }
  const std::string held(path);
  std::FILE *const file = std::fopen(held.c_str(), "wb");
  if (file == nullptr) {
    S_->Error = held + ": the save file would not open";
    return false;
  }
  const size_t wrote = std::fwrite(text.data(), 1, text.size(), file);
  const bool closed = std::fclose(file) == 0;
  if (wrote != text.size() || !closed) {
    S_->Error = held + ": the save did not reach the disk whole -- a full disk is a refusal, "
                "never a successful save";
    return false;
  }
  S_->Error.clear();
  return true;
}

bool Engine::Restore(std::string_view path) {
  if (!S_->Stood.Instances.size() && S_->Declared.Instances.empty()) {
    S_->Error = "nothing is assembled, and loading a save is standing the scenario up FIRST "
                "and then applying the state -- one arrival route";
    return false;
  }
  std::string text;
  if (!SlurpFile(std::string(path), text, S_->Error)) { return false; }
  size_t at = text.find('\n');
  const std::string head = text.substr(0, at == std::string::npos ? text.size() : at);
  const std::string wanted =
      "outshine-save 1 " + S_->Declared.Named.Name + " " + S_->Declared.Named.Version;
  if (head != wanted) {
    S_->Error = "the save says '" + head + "' and this engine stands '" + wanted +
                "' -- a save from another scenario or version refuses quoting both";
    return false;
  }
  struct Landing {
    Entity Holder = kNoEntity;
    uint32_t Key = 0;
    double Value = 0.0;
  };
  std::vector<Landing> staged;
  while (at != std::string::npos && at + 1 < text.size()) {
    const size_t end = text.find('\n', at + 1);
    const std::string line =
        text.substr(at + 1, (end == std::string::npos ? text.size() : end) - at - 1);
    at = end;
    if (line.empty()) { continue; }
    const size_t gap = line.rfind(' ');
    const size_t dot = line.find('.');
    if (gap == std::string::npos || dot == std::string::npos || dot > gap) {
      S_->Error = "the save line '" + line + "' does not read as instance.trait value";
      return false;
    }
    Landing landing;
    landing.Holder = S_->Stood.InstanceNamed(std::string_view(line).substr(0, dot));
    landing.Key = S_->Stood.TraitKey(std::string_view(line).substr(dot + 1, gap - dot - 1));
    const auto scanned =
        std::from_chars(line.data() + gap + 1, line.data() + line.size(), landing.Value);
    if (scanned.ec != std::errc() || scanned.ptr != line.data() + line.size() ||
        !std::isfinite(landing.Value)) {
      S_->Error = "the save line '" + line + "' does not read as instance.trait value";
      return false;
    }
    if (landing.Holder == kNoEntity || landing.Key == 0) {
      S_->Error = "the save names '" + line.substr(0, gap) +
                  "', which the assembled scene does not hold -- the declaration moved on and "
                  "the save did not";
      return false;
    }
    staged.push_back(landing);
  }
  std::vector<std::pair<Entity, Traits>> rows;
  for (const Landing &landing : staged) {
    Traits *row = nullptr;
    for (auto &held : rows) {
      if (held.first == landing.Holder) { row = &held.second; }
    }
    if (row == nullptr) {
      const Traits *standing = S_->Kinds.Get(landing.Holder);
      rows.emplace_back(landing.Holder, standing == nullptr ? Traits{} : *standing);
      row = &rows.back().second;
    }
    if (row->Named(landing.Key) == nullptr) {
      S_->Error = "the save carries a value for a trait this holder never declared -- the "
                  "declaration moved on and the save did not, and NOTHING was applied";
      return false;
    }
    if (!row->Put(landing.Key, landing.Value)) {
      S_->Error = "the saved value found no seat -- the holder already carries its full " +
                  std::to_string(Traits::kMost) + " traits, and NOTHING was applied";
      return false;
    }
  }
  for (const auto &held : rows) {
    if (!S_->Kinds.Put(held.first, held.second)) {
      S_->Error = "a validated holder died between the dry run and the commit";
      return false;
    }
  }
  S_->Error.clear();
  return true;
}
const std::vector<std::string> &Engine::Carried() const { return S_->Carried; }

bool Engine::Advance() {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to advance";
    return false;
  }
  if (S_->Drove) {
    const Sim::Ridden &rode =
        Sim::DriveTick(S_->Drive.Way, S_->Drive.Stood, S_->Drive.State, kTickS, nullptr);
    if (!rode.Found || rode.Lost) {
      S_->Error = "the drive left its corridor at " + Said(rode.ReachedM) + " m";
      return false;
    }
    if (rode.Arrived) { return false; }
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

bool Engine::Park() {
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

std::vector<std::string> Engine::Parked() const {
  std::vector<std::string> names;
  for (const Scenario &asleep : S_->Asleep) { names.push_back(asleep.Named.Name); }
  return names;
}

int Engine::At() const { return S_->Standing ? S_->Standing->At() : 0; }
int Engine::Frames() const { return S_->Standing ? S_->Standing->Frames() : 0; }
bool Engine::Standing() const { return S_->Standing != nullptr; }
const std::string &Engine::Error() const { return S_->Error; }

}
