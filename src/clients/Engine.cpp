#include <Outshine.h>
#include "Fetching.h"
#include "Unwired.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <vector>

#include "Assembly.h"
#include "ScenarioLayer.h"
#include "Live.h"
#include "Script.h"
#include "Typeface.h"
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
  void Number(const char *what, double how, const char *unit) override {
    std::string held = std::string(what) + " = " + Rounded(how);
    if (unit != nullptr && unit[0] != '\0') { held += " " + std::string(unit); }
    Held.push_back(std::move(held));
  }
  void Claim(bool held, const char *why) override {
    Held.push_back(std::string(held ? "HELD " : "FAILED ") + why);
    if (!held && Why.empty()) { Why = why; }
  }
  void Near(double was, double wanted, double within, const char *unit, const char *why) override {
    Held.push_back(std::string("NEAR ") + Rounded(was) + " of " + Rounded(wanted) + " within " +
                   Rounded(within) + (unit == nullptr ? "" : std::string(" ") + unit) + ": " + why);
    if (Why.empty()) { Why = why; }
  }
  void Say(const std::string &said) override { Held.push_back(said); }
  void Refuse(const std::string &why) override {
    Held.push_back("REFUSED " + why);
    if (Why.empty()) { Why = why; }
  }
  [[nodiscard]] const std::string &WhyNot() const { return Why; }

  std::vector<std::string> Held;

private:
  [[nodiscard]] static std::string Rounded(double how) {
    char held[32];
    std::snprintf(held, sizeof held, "%.6g", how);
    return held;
  }

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
  std::unique_ptr<Data::Transport> Wire;
  size_t GroundTiles = 0;
  Ui::Typeface Face;
  std::vector<std::string> Measured;
  Host *Offered = nullptr;
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
      S_->Wire = std::make_unique<Unwired>();
    } else {
      S_->Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }
  Quietly say;
  const Sim::Provision kept{S_->Under.Cache, S_->Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Sim::AssembleDrive(S_->Scene, S_->Stood, S_->Vehicles, S_->Drives,
                                         declared.Ground, S_->Stack, *S_->Wire, kept, say,
                                         S_->Drive);
  S_->Measured = std::move(say.Held);
  if (!routed) {
    S_->Error = say.WhyNot();
    return false;
  }
  S_->Drove = true;
  return true;
}

Engine::~Engine() = default;
Engine::Engine(Engine &&) noexcept = default;
Engine &Engine::operator=(Engine &&) noexcept = default;

bool Engine::DrawsInto(SDL_Window *presents) {
  if (presents == nullptr) {
    S_->Error = "a window is what DrawsInto presents on, and this one is none -- an engine that "
                "draws nowhere is declared with an Extent instead";
    return false;
  }
  int widthPx = 0, heightPx = 0;
  SDL_GetWindowSizeInPixels(presents, &widthPx, &heightPx);
  const auto standing = S_->Device.DrawsInto(widthPx, heightPx, presents);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Frame = Extent{widthPx, heightPx};
  return true;
}

bool Engine::DrawsInto(Extent offscreen) {
  const auto standing = S_->Device.DrawsInto(offscreen.WidthPx, offscreen.HeightPx, nullptr);
  if (!standing) {
    S_->Error = std::string(standing.error());
    return false;
  }
  S_->Frame = offscreen;
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
  S_->Device.WantsPixels();
  if (!S_->Standing->Advance(S_->Error)) { return false; }
  return S_->Standing->Screenshot(std::string(path), S_->Error);
}

namespace {

class ToTheClient final : public Script::Host {
public:
  explicit ToTheClient(outshine::Host *client) : Client_(client) {}

  [[nodiscard]] Script::Value Global(std::string_view name) override {
    for (size_t at = 0; at < Named_.size(); ++at) {
      if (Named_[at] == name) { return Script::Value::OfRef((int)at + 1); }
    }
    Named_.emplace_back(name);
    return Script::Value::OfRef((int)Named_.size());
  }

  [[nodiscard]] bool Call(const Script::Value &callee, const Script::Value *args, size_t count,
                          Script::Value &out) override {
    out = Script::Value();
    if (Client_ == nullptr || callee.What != Script::Kind::Ref) { return false; }
    const size_t which = (size_t)callee.Ref - 1;
    if (callee.Ref <= 0 || which >= Named_.size()) { return false; }

    std::vector<Argument> handed(count);
    for (size_t at = 0; at < count; ++at) {
      if (args[at].What == Script::Kind::Text) {
        handed[at].Is = Argument::Kind::Text;
        handed[at].Text = args[at].Text;
      } else {
        handed[at].Is = Argument::Kind::Number;
        handed[at].Number = args[at].Number;
      }
    }
    Fired_ = true;
    return Client_->Calls(Named_[which], handed);
  }

  [[nodiscard]] bool Fired() const { return Fired_; }

private:
  outshine::Host *Client_ = nullptr;
  std::vector<std::string> Named_;
  bool Fired_ = false;
};

}

void Engine::Offers(Host *host) { S_->Offered = host; }

bool Engine::Handles(const SDL_Event &event) {
  if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN || !S_->Standing) { return false; }

  size_t surface = 0;
  const Ui::Touched found =
      S_->Standing->Under((double)event.button.x, (double)event.button.y, surface);
  if (!found.Held() || found.Action.empty()) { return false; }
  const std::string &action = found.Action;
  if (S_->Offered == nullptr) {
    S_->Error = "a surface declares the call '" + action +
                "' and no host was offered to answer it -- the client calls Offers before it "
                "hands an event in";
    return false;
  }

  Script::Program programme;
  const std::string text = S_->Standing->ProgrammeOf(surface) + "\n" + action + ";\n";
  if (!programme.Read(text, S_->Error)) { return false; }
  ToTheClient answering(S_->Offered);
  if (!programme.Run(answering, S_->Error)) { return false; }
  return answering.Fired();
}

bool Engine::Declare(const Scenario &scenario) {
  if (S_->Frame.WidthPx <= 0 || S_->Frame.HeightPx <= 0) {
    S_->Error =
        "no canvas stands, so a scenario has nowhere to draw -- the client hands one in through "
        "DrawsInto before it declares";
    return false;
  }
  const Asset *const subject = scenario.Subject();
  if (subject == nullptr && scenario.Surfaces.empty()) {
    S_->Error = "a scenario shows an asset of kind 'gltf' or a surface, and this one declares "
                "neither -- a declaration with nothing to draw is a picture nobody asked for";
    return false;
  }

  Clients::Declaration declared;
  declared.SurfaceWidthPx = S_->Frame.WidthPx;
  declared.SurfaceHeightPx = S_->Frame.HeightPx;
  if (subject != nullptr) {
    declared.Stands = Beneath(S_->Under.Assets, subject->Uri);
    declared.Variant = subject->Variant;
    declared.Animation = subject->Animation;
  }
  declared.Fps = scenario.Render.Fps;
  declared.Fill = scenario.Render.Fill;
  declared.OrbitDegPerFrame = scenario.Render.OrbitDegPerFrame;
  declared.PictureLeftFrac = scenario.Render.Picture.LeftFrac;
  declared.PictureTopFrac = scenario.Render.Picture.TopFrac;
  declared.PictureWidthFrac = scenario.Render.Picture.WidthFrac;
  declared.PictureHeightFrac = scenario.Render.Picture.HeightFrac;
  declared.KeyLux = scenario.Lit.Key.Lux;
  declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;
  declared.KeyBearingDeg = scenario.Lit.Key.BearingDeg;
  for (int at = 0; at < 3; ++at) { declared.Environment[at] = scenario.Lit.Environment[at]; }

  std::vector<const Surface *> ordered;
  ordered.reserve(scenario.Surfaces.size());
  for (const Surface &surface : scenario.Surfaces) { ordered.push_back(&surface); }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const Surface *a, const Surface *b) { return a->Z < b->Z; });
  for (const Surface *surface : ordered) {
    Clients::Shows shows;
    shows.Markup = surface->Document;
    shows.Style = surface->Style;
    shows.Programme = surface->Programme;
    shows.LeftFrac = surface->Where.LeftFrac;
    shows.TopFrac = surface->Where.TopFrac;
    shows.WidthFrac = surface->Where.WidthFrac;
    shows.HeightFrac = surface->Where.HeightFrac;
    declared.Surfaces.push_back(std::move(shows));
  }
  if (!declared.Surfaces.empty() && !S_->Face.Opens(S_->Under.Shipped + "/fonts", S_->Error)) {
    return false;
  }

  S_->Standing.reset();
  if (!Clients::Live::Open(S_->Device, std::move(declared), &S_->Face, S_->Standing, S_->Error)) {
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

const std::vector<std::string> &Engine::Measured() const { return S_->Measured; }

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
  if (!S_->Standing->Advance(S_->Error)) { return false; }
  return true;
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
