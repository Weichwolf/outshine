#include <Outshine.h>
#include "Fetching.h"
#include "HeapProbe.h"
#include "Unwired.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <numbers>
#include <charconv>
#include <cmath>
#include <vector>

#include "Assembly.h"
#include "ScenarioLayer.h"
#include "Live.h"
#include "Script.h"
#include "Typeface.h"
#include "InputPump.h"
#include "Triggers.h"
#include "Views.h"
#include "Sink.h"
#include "DeclaredSources.h"
#include "GroundStack.h"
#include "GroundUnderfoot.h"
#include "DriveAssembly.h"
#include "GroundPatchwork.h"
#include "TileGeodesy.h"
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
    Took.push_back(Measure{what, how, unit == nullptr ? std::string() : std::string(unit)});
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
  std::vector<Measure> Took;

private:
  [[nodiscard]] static std::string Rounded(double how) {
    char held[32];
    std::snprintf(held, sizeof held, "%.6g", how);
    return held;
  }

  std::string Why;
};


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
  Column<Body> Bodies;
  Column<Journey> Drives;
  Column<Traits> Kinds;
  Assembled Stood;
  Roots Under;
  std::unique_ptr<Data::Transport> Wire;
  size_t GroundTiles = 0;
  size_t MostSteps = 0;
  size_t Steps = 0;
  Ui::Typeface Face;
  std::optional<ViewBook> Views;
  InputMap Bound;
  Clients::InputPump Pump;
  bool Pumping = false;
  std::optional<TriggerField> Volumes;
  std::vector<Measure> Numbers;
  size_t Standing_Placed = 0;
  Host *Offered = nullptr;
  Ground::GroundStack Stack;
  std::vector<const Generates *> Making;
  Sim::DriveProduct Drive;
  std::vector<Physics::Body> Freestanding;
  std::unique_ptr<Sim::GroundUnderfoot> Surface;
  bool Drove = false;
  size_t Fired = 0;
  Clients::Declaration Shown;
  double OwedS = 0.0;
  std::string Error;

  void Places(const char *what, double how, const char *unit);
  void Drew(void);
  [[nodiscard]] bool Rides(void);
  [[nodiscard]] bool Places(const Physics::Body &body, const double shiftM[3]);
  void Falls(void);
  [[nodiscard]] bool Composes(void);
  [[nodiscard]] bool Routes(void);
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
  note(scenario.Bodies.size(), "vehicles");
  note(scenario.State.size(), "persisted values");
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
    S_->Drove = false;
    return S_->Routes();
  }
  if (!S_->Scene.Open(named) || !S_->Bodies.Open(S_->Scene) ||
      !S_->Drives.Open(S_->Scene) || !S_->Kinds.Open(S_->Scene)) {
    S_->Error = "the scene did not open for the " + std::to_string(named) +
                " entities the declaration names";
    return false;
  }
  if (!outshine::Assemble(declared, S_->Scene, S_->Bodies, S_->Drives, S_->Kinds, S_->Stood,
                          S_->Error)) {
    return false;
  }

  return S_->Routes();
}

namespace {

using Assembler = bool (*)(const Store &, const Assembled &, const Column<Body> &,
                           const Column<Journey> &, const WorldSettings &, Ground::GroundStack &,
                           Data::Transport &, const Sim::Provision &, Sink &, Sim::DriveProduct &);

struct Travelling_ {
  Travels By;
  const char *Named;
  Assembler How;
};

constexpr size_t kTravels = 4;

const Travelling_ kAssemblers[kTravels] = {
    {Travels::Walk, "foot", nullptr},
    {Travels::Drive, "road", &Sim::AssembleDrive},
    {Travels::Fly, "air", nullptr},
    {Travels::Rail, "rail", nullptr},
};

[[nodiscard]] Assembler Assembles(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.How; }
  }
  return nullptr;
}

[[nodiscard]] const char *Travelling(Travels by) {
  for (const Travelling_ &one : kAssemblers) {
    if (one.By == by) { return one.Named; }
  }
  return "an unnamed way";
}

}

bool Engine::State::Routes(void) {
  const Scenario &declared = Declared;
  Drove = false;
  Freestanding.clear();
  for (const Body &stands : declared.Bodies) {
    if (!stands.Placed) { continue; }
    Physics::Body held;
    held.MassKg = stands.MassKg;
    for (int axis = 0; axis < 3; ++axis) {
      held.PositionM[axis] = stands.AtM[axis];
      held.InertiaKgM2[axis] = stands.InertiaKgM2[axis];
    }
    held.OrientationQ[0] = stands.FacingXyzw[3];
    held.OrientationQ[1] = stands.FacingXyzw[0];
    held.OrientationQ[2] = stands.FacingXyzw[1];
    held.OrientationQ[3] = stands.FacingXyzw[2];
    Freestanding.push_back(held);
  }
  if (!declared.Routed.Declared) { return true; }
  if (Assembles(declared.Routed.By) == nullptr) {
    Error = std::string("the scenario declares a journey travelling by ") +
            Travelling(declared.Routed.By) +
            ", and nothing assembles that -- a mode the engine cannot lay a corridor for is a "
            "refusal, never a journey that quietly does not happen";
    return false;
  }
  if (!Wire) {
    if (Under.Offline) {
      Wire = std::make_unique<Unwired>();
    } else {
      Wire = std::make_unique<Fetching>(Fetching::Config{});
    }
  }
  Quietly say;
  const Sim::Provision kept{Under.Cache, Under.Shipped,
                            {Data::ShippedProviders().begin(), Data::ShippedProviders().end()}};
  const bool routed = Assembles(declared.Routed.By)(Scene, Stood, Bodies, Drives, declared.Ground,
                                                    Stack, *Wire, kept, say, Drive);
  if (routed) {
    Stack.Restand(Drive.Way.FrameLat, Drive.Way.FrameLon);
    Surface = std::make_unique<Sim::GroundUnderfoot>(Stack, Drive.Surfaces);
    Surface->Restand();
  }
  Carried.insert(Carried.end(), std::make_move_iterator(say.Held.begin()),
                     std::make_move_iterator(say.Held.end()));
  Numbers = std::move(say.Took);
  Standing_Placed = Numbers.size();
  if (!routed) {
    Error = say.WhyNot();
    return false;
  }
  Places("how long the corridor is", Drive.Way.Line.LengthM(), "m");
  Places("how far along it the body has come", 0.0, "m");
  if (Standing && Drive.Stood.MetresPerAssetUnit > 0.0) {
    Standing->ScaledBy(Drive.Stood.MetresPerAssetUnit);
  }
  Drove = true;
  {
    const double slowestMs = Drive.Way.Profile.Quantile(0.01);
    if (!(slowestMs > 0.0)) {
      Error = "a hundredth of this speed plan stands still, so the drive has no pace to be "
              "bounded by and would never arrive -- p01 is " + Said(slowestMs) + " m/s";
      return false;
    }
    const double stepS = Declared.Motion.StepS > 0.0 ? Declared.Motion.StepS : 1.0;
    MostSteps = (size_t)(Drive.Way.Line.LengthM() / slowestMs / stepS) + 1u;
    Places("the steps the plan allows at its slowest station", (double)MostSteps, "steps");
  }
  if (!Composes()) {
    Carried.push_back("the ground did not compose: " + Error);
    Error.clear();
  } else if (!Rides()) {
    return false;
  }
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



bool Engine::State::Composes(void) {
  GroundTiles = 0;
  if (!Standing) {
    Error = "nothing stands to compose a world around";
    return false;
  }
  const Scenario &declared = Declared;
  const Sim::Corridor &way = Drive.Way;
  const bool overADrive = Drove && !way.Fine.empty();
  if (!declared.Ground.Declared && !overADrive) {
    Error = "the scenario declares neither a sphere nor a drive that laid a corridor, so there "
            "is nowhere for a ground to be composed";
    return false;
  }
  const double atLat = overADrive ? way.FrameLat : declared.Ground.Lat;
  const double atLon = overADrive ? way.FrameLon : declared.Ground.Lon;
  if (!Wire) {
    if (Under.Offline) {
      Error = "the ground is FETCHED and the engine was declared offline";
      return false;
    }
    Wire = std::make_unique<Fetching>(Fetching::Config{});
  }

  Quietly say;
  if (!Stack.Opened() &&
      !Stack.Open(Under.Cache, Under.Shipped,
                      {Data::ShippedProviders().begin(), Data::ShippedProviders().end()},
                      atLat, atLon, *Wire, say)) {
    Error = say.WhyNot();
    return false;
  }

  Around over;
  over.LatDeg = atLat;
  over.LonDeg = atLon;
  over.Zoom = Stack.FinestZoomOf(Data::DataKind::Elevation);
  over.Ring = 1;
  over.Awaited = true;
  auto laid = LayPatchwork(Stack.Pool(), over);
  if (!laid) {
    Error = laid.error();
    return false;
  }

  std::vector<float> inFrame;
  if (overADrive) {
    inFrame.resize(laid->PositionM.size());
    for (size_t at = 0; at + 2 < laid->PositionM.size(); at += 3) {
      const Ground::Ecef held{laid->OriginEcef[0] + (double)laid->PositionM[at],
                              laid->OriginEcef[1] + (double)laid->PositionM[at + 1],
                              laid->OriginEcef[2] + (double)laid->PositionM[at + 2]};
      const Ground::Geo where = Ground::EcefToGeoWgs84(held);
      inFrame[at] = (float)((where.LonDeg - way.FrameLon) * way.PerLonM);
      inFrame[at + 1] = (float)where.AltM;
      inFrame[at + 2] = (float)(-(where.LatDeg - way.FrameLat) * way.PerLatM);
    }
  }

  if (overADrive) {
    double nearest = 1.0e30, atUp = 0.0;
    for (size_t at = 0; at + 2 < inFrame.size(); at += 3) {
      const double east = (double)inFrame[at], south = (double)inFrame[at + 2];
      const double away = east * east + south * south;
      if (away >= nearest) { continue; }
      nearest = away;
      atUp = (double)inFrame[at + 1];
    }
    Places("the ring's nearest vertex to the frame origin", std::sqrt(nearest), "m");
    Places("and its up", atUp, "m");
  }
  if (overADrive) {
    for (size_t at = 0; at + 2 < laid->Index.size(); at += 3) {
      std::swap(laid->Index[at + 1], laid->Index[at + 2]);
    }
    const double lat = way.FrameLat * std::numbers::pi / 180.0;
    const double lon = way.FrameLon * std::numbers::pi / 180.0;
    const double sinLat = std::sin(lat), cosLat = std::cos(lat);
    const double sinLon = std::sin(lon), cosLon = std::cos(lon);
    const double east[3] = {-sinLon, cosLon, 0.0};
    const double north[3] = {-sinLat * cosLon, -sinLat * sinLon, cosLat};
    const double upward[3] = {cosLat * cosLon, cosLat * sinLon, sinLat};
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double held[3] = {(double)laid->NormalM[at], (double)laid->NormalM[at + 1],
                              (double)laid->NormalM[at + 2]};
      double alongEast = 0.0, alongUp = 0.0, alongNorth = 0.0;
      for (int axis = 0; axis < 3; ++axis) {
        alongEast += east[axis] * held[axis];
        alongUp += upward[axis] * held[axis];
        alongNorth += north[axis] * held[axis];
      }
      laid->NormalM[at] = (float)alongEast;
      laid->NormalM[at + 1] = (float)alongUp;
      laid->NormalM[at + 2] = (float)(-alongNorth);
    }
  }
  {
    double up = 0.0, down = 0.0, sideways = 0.0, unlengthed = 0.0;
    for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
      const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
      const double length = std::sqrt(x * x + y * y + z * z);
      if (!(length > 0.5)) { unlengthed += 1.0; continue; }
      const double upward = y / length;
      if (upward > 0.5) { up += 1.0; }
      else if (upward < -0.5) { down += 1.0; }
      else { sideways += 1.0; }
    }
    Places("the ring's normals that point up", up, "normals");
    Places("its normals that point DOWN", down, "normals");
    Places("its normals that lie sideways", sideways, "normals");
    {
      double steepest = 0.0, mean = 0.0, counted = 0.0;
      for (size_t at = 0; at + 2 < laid->NormalM.size(); at += 3) {
        const double x = laid->NormalM[at], y = laid->NormalM[at + 1], z = laid->NormalM[at + 2];
        const double length = std::sqrt(x * x + y * y + z * z);
        if (!(length > 1.0e-6)) { continue; }
        const double leanDeg = std::acos(std::fmin(1.0, y / length)) * 180.0 / std::numbers::pi;
        steepest = leanDeg > steepest ? leanDeg : steepest;
        mean += leanDeg;
        counted += 1.0;
      }
      Places("the steepest the ring's surface leans", steepest, "deg");
      Places("how far it leans on average", counted > 0.0 ? mean / counted : 0.0, "deg");
    }
    Places("its normals with no length at all", unlengthed, "normals");
    Places("its normals in all", (double)(laid->NormalM.size() / 3), "normals");
    {
      double least = 1.0e30, most = -1.0e30;
      const std::vector<float> &held = overADrive ? inFrame : laid->PositionM;
      for (size_t at = 1; at < held.size(); at += 3) {
        const double y = (double)held[at];
        if (y < least) { least = y; }
        if (y > most) { most = y; }
      }
      Places("the ring's lowest vertex", least, "m");
      Places("its highest", most, "m");
    }
  }
  Geometry ground;
  const int ringPart = ground.Part("ground", 0);
  (void)ground.Positions(ringPart, overADrive
                                       ? std::span<const float>(inFrame.data(), inFrame.size())
                                       : std::span<const float>(laid->PositionM.data(),
                                                                laid->PositionM.size()));
  (void)ground.Normals(ringPart,
                       std::span<const float>(laid->NormalM.data(), laid->NormalM.size()));
  (void)ground.Triangles(ringPart, std::span<const uint32_t>(laid->Index.data(),
                                                             laid->Index.size()));

  Gltf::Subject laidGround;
  if (!laidGround.Assemble(ground)) {
    Error = laidGround.Error();
    return false;
  }
  const Render::Medium air;
  Material wearing;
  for (int channel = 0; channel < 3; ++channel) {
    wearing.BaseColour[channel] = air.GroundAlbedo[channel];
  }
  const size_t drivenParts = Standing->Shown().Parts().size();
  if (!Standing->Restand(laidGround, drivenParts, wearing, Error)) { return false; }
  GroundTiles = laid->Tiles;
  Places("tiles the ring laid", (double)laid->Tiles, "tiles");
  Places("tiles it is still waiting for", (double)laid->Pending, "tiles");
  Places("tiles the stack does not hold", (double)laid->Absent, "tiles");
  Places("tiles it refused", (double)laid->Refused, "tiles");
  return true;
}


bool Engine::RenderTo(Extent frame) {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to draw";
    return false;
  }
  if (frame.WidthPx > 0 && frame.HeightPx > 0 &&
      (frame.WidthPx != S_->Frame.WidthPx || frame.HeightPx != S_->Frame.HeightPx)) {
    S_->Error = "this engine stands on a " + std::to_string(S_->Frame.WidthPx) + "x" +
                std::to_string(S_->Frame.HeightPx) + " canvas and was asked to draw " +
                std::to_string(frame.WidthPx) + "x" + std::to_string(frame.HeightPx) +
                " -- a canvas is declared before a scenario stands on it";
    return false;
  }
  if (!S_->Standing->Draw(S_->Error)) { return false; }
  S_->Drew();
  return true;
}

bool Engine::Pixels(std::vector<uint8_t> &rgba) {
  if (!S_->Standing) {
    S_->Error = "nothing stands to be read -- a scenario is declared before a frame carries pixels";
    return false;
  }
  S_->Device.WantsPixels();
  if (!S_->Standing->Draw(S_->Error)) { return false; }
  return S_->Standing->ReadPixels(rgba, S_->Error);
}

bool Engine::Capture(std::string_view path) {
  if (!S_->Standing) {
    S_->Error = "nothing stands to be captured -- a scenario is declared before a frame is kept";
    return false;
  }
  S_->Device.WantsPixels();
  if (!S_->Standing->Draw(S_->Error)) { return false; }
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
  if (!S_->Standing) { return false; }
  if (event.type == SDL_EVENT_MOUSE_WHEEL) {
    float xPx = 0.0f, yPx = 0.0f;
    SDL_GetMouseState(&xPx, &yPx);
    return S_->Standing->Wheeled((double)xPx, (double)yPx,
                                 -(double)event.wheel.y * S_->Declared.WheelStepPx, S_->Error);
  }
  if (S_->Pumping && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP)) {
    Clients::InputPump::Fired fired[2];
    const size_t many = S_->Pump.Translate(event, fired);
    if (S_->Offered == nullptr) { return false; }
    bool acted = false;
    for (size_t at = 0; at < many; ++at) {
      const std::string *const named = S_->Bound.ActionNamed(fired[at].Action);
      if (named == nullptr) { continue; }
      const Argument value{Argument::Kind::Number, (double)fired[at].Value, {}};
      acted = S_->Offered->Calls(*named, std::span<const Argument>(&value, 1)) || acted;
    }
    return acted;
  }
  if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN) { return false; }

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

bool Engine::Shows(const std::vector<Surface> &surfaces) {
  if (!S_->Standing) {
    S_->Error = "nothing stands, so there is no picture for a surface to be laid over -- a "
                "scenario is declared before its surfaces are exchanged";
    return false;
  }
  if (!surfaces.empty() && !S_->Face.Opens(S_->Under.Shipped + "/fonts", S_->Error)) {
    return false;
  }
  std::vector<const Surface *> ordered;
  ordered.reserve(surfaces.size());
  for (const Surface &surface : surfaces) { ordered.push_back(&surface); }
  std::stable_sort(ordered.begin(), ordered.end(),
                   [](const Surface *a, const Surface *b) { return a->Z < b->Z; });
  std::vector<Clients::Shows> laid;
  laid.reserve(ordered.size());
  for (const Surface *surface : ordered) {
    Clients::Shows shows;
    shows.Markup = surface->Document;
    shows.Style = surface->Style;
    shows.Programme = surface->Programme;
    shows.LeftFrac = surface->Where.LeftFrac;
    shows.TopFrac = surface->Where.TopFrac;
    shows.WidthFrac = surface->Where.WidthFrac;
    shows.HeightFrac = surface->Where.HeightFrac;
    laid.push_back(std::move(shows));
  }
  S_->Declared.Surfaces = surfaces;
  return S_->Standing->Redeclare(std::move(laid), S_->Error);
}

namespace {

[[nodiscard]] bool SameShows(const Clients::Shows &a, const Clients::Shows &b) {
  return a.Markup == b.Markup && a.Style == b.Style && a.Programme == b.Programme &&
         a.LeftFrac == b.LeftFrac && a.TopFrac == b.TopFrac && a.WidthFrac == b.WidthFrac &&
         a.HeightFrac == b.HeightFrac;
}

[[nodiscard]] bool SameSurfaces(const std::vector<Clients::Shows> &a,
                                const std::vector<Clients::Shows> &b) {
  if (a.size() != b.size()) { return false; }
  for (size_t at = 0; at < a.size(); ++at) {
    if (!SameShows(a[at], b[at])) { return false; }
  }
  return true;
}

[[nodiscard]] bool SamePicture(const Clients::Declaration &a, const Clients::Declaration &b) {
  return a.SurfaceWidthPx == b.SurfaceWidthPx &&
         a.SurfaceHeightPx == b.SurfaceHeightPx && a.Built == b.Built &&
         a.MetresPerUnit == b.MetresPerUnit && a.Fps == b.Fps &&
         a.Fill == b.Fill && a.OrbitDegPerFrame == b.OrbitDegPerFrame &&
         a.PictureLeftFrac == b.PictureLeftFrac && a.PictureTopFrac == b.PictureTopFrac &&
         a.PictureWidthFrac == b.PictureWidthFrac && a.PictureHeightFrac == b.PictureHeightFrac &&
         a.Environment[0] == b.Environment[0] && a.Environment[1] == b.Environment[1] &&
         a.Environment[2] == b.Environment[2] && a.KeyLux == b.KeyLux &&
         a.Exposure == b.Exposure && a.DrawsSky == b.DrawsSky &&
         a.ShadowRadiusM == b.ShadowRadiusM && a.KeyElevationDeg == b.KeyElevationDeg &&
         a.KeyBearingDeg == b.KeyBearingDeg;
}

[[nodiscard]] bool SameStand(const Clients::Declaration &a, const Clients::Declaration &b) {
  return SamePicture(a, b) && a.Stands == b.Stands && a.Variant == b.Variant &&
         a.Animation == b.Animation;
}

}

void Engine::Offers(const Generates &maker) {
  for (const Generates *const stood : S_->Making) {
    if (stood->Kind() == maker.Kind()) { return; }
  }
  S_->Making.push_back(&maker);
}

bool Engine::Declare(const Scenario &scenario) {
  const auto offers = [this](const std::string &kind) {
    for (const Generates *const stood : S_->Making) {
      if (stood->Kind() == kind) { return true; }
    }
    return false;
  };
  for (const Generator &named : scenario.Generators) {
    if (offers(named.Kind)) { continue; }
    S_->Error = "the scenario declares a generator of kind '" + named.Kind +
                "' and nothing offers that kind -- a declaration nobody can act on is a refusal, "
                "never a line that is counted and dropped";
    return false;
  }
  for (const Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated" || offers(shown.Uri)) { continue; }
    S_->Error = "the scenario stands the generated asset '" + shown.Uri +
                "' and nothing offers a generator of that kind -- an asset names a generator the "
                "way a scenario names anything, and a name nobody answers is a refusal";
    return false;
  }
  if (S_->Frame.WidthPx <= 0 || S_->Frame.HeightPx <= 0) {
    S_->Error =
        "no canvas stands, so a scenario has nowhere to draw -- the client hands one in through "
        "DrawsInto before it declares";
    return false;
  }
  const Asset *const subject = scenario.Subject();

  Clients::Declaration declared;
  declared.SurfaceWidthPx = S_->Frame.WidthPx;
  declared.SurfaceHeightPx = S_->Frame.HeightPx;
  if (subject != nullptr) {
    declared.Stands = Beneath(S_->Under.Assets, subject->Uri);
    declared.Variant = subject->Variant;
    declared.Animation = subject->Animation;
  }
  declared.DrawsSky = scenario.Ground.Declared && scenario.Ground.AirDensityKgM3 > 0.0;
  const Patch whole;
  const Patch &picture = scenario.Render.Declared ? scenario.Render.Picture : whole;
  if (scenario.Render.Declared) {
    declared.Fps = scenario.Render.Fps;
    declared.Fill = scenario.Render.Fill;
    declared.OrbitDegPerFrame = scenario.Render.OrbitDegPerFrame;
  }
  declared.PictureLeftFrac = picture.LeftFrac;
  declared.PictureTopFrac = picture.TopFrac;
  declared.PictureWidthFrac = picture.WidthFrac;
  declared.PictureHeightFrac = picture.HeightFrac;
  if (scenario.Lit.Declared) {
    declared.KeyLux = scenario.Lit.Key.Lux;
    declared.KeyElevationDeg = scenario.Lit.Key.ElevationDeg;
    declared.KeyBearingDeg = scenario.Lit.Key.BearingDeg;
    for (int at = 0; at < 3; ++at) { declared.Environment[at] = scenario.Lit.Environment[at]; }
    declared.ShadowRadiusM = scenario.Lit.ShadowRadiusM;
  }

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

  S_->Pumping = false;
  if (!scenario.Input.empty()) {
    if (!S_->Bound.Build(scenario.Input, S_->Error)) { return false; }
    if (!S_->Pump.Open(S_->Bound)) {
      S_->Error = "the declared bindings did not open a pump, so no event could reach an action";
      return false;
    }
    S_->Pumping = true;
  }

  S_->Volumes.reset();
  if (!scenario.Volumes.empty()) {
    auto stood = TriggerField::Stand(scenario.Volumes, scenario.Events);
    if (!stood) {
      S_->Error = stood.error();
      return false;
    }
    S_->Volumes.emplace(std::move(*stood));
  }

  S_->Views.reset();
  if (!scenario.Views.empty()) {
    const std::string_view starting = scenario.Played.View.empty()
                                          ? std::string_view(scenario.Views.front().Id)
                                          : std::string_view(scenario.Played.View);
    auto stood = ViewBook::Stand(scenario.Views, starting);
    if (!stood) {
      S_->Error = stood.error();
      return false;
    }
    S_->Views.emplace(std::move(*stood));
  }

  if (S_->Standing && SamePicture(S_->Shown, declared)) {
    if (!SameStand(S_->Shown, declared) &&
        !S_->Standing->Restands(declared.Stands, declared.Variant, declared.Animation,
                                S_->Error)) {
      return false;
    }
    if (!SameSurfaces(S_->Shown.Surfaces, declared.Surfaces) &&
        !S_->Standing->Redeclare(declared.Surfaces, S_->Error)) {
      return false;
    }
    S_->Shown = std::move(declared);
    S_->Declared = scenario;
    S_->Carried = Unacted(scenario);
    S_->Error.clear();
    return true;
  }

  std::vector<std::vector<Ui::Layout::Scrolled>> wasScrolled;
  if (S_->Standing) { wasScrolled = S_->Standing->Scrolled(); }
  S_->Standing.reset();
  S_->Shown = declared;
  if (!Clients::Live::Open(S_->Device, std::move(declared), &S_->Face, S_->Standing, S_->Error)) {
    S_->Standing.reset();
    return false;
  }
  if (!wasScrolled.empty() && !S_->Standing->Scrolled(std::move(wasScrolled), S_->Error)) {
    return false;
  }
  S_->Declared = scenario;
  S_->Carried = Unacted(scenario);
  S_->Error.clear();
  return Generated(scenario);
}

bool Engine::Generated(const Scenario &scenario) {
  Ask ask;
  ask.EastM = scenario.Ground.Lon;
  ask.NorthM = scenario.Ground.Lat;
  ask.ExtentM = scenario.Ground.RadiusM;

  Geometry made;
  const auto asked = [&](const std::string &kind) {
    for (const Generates *const stood : S_->Making) {
      if (stood->Kind() != kind) { continue; }
      if (stood->Make(ask, made)) { return true; }
      S_->Error = "the generator of kind '" + kind + "' refused to make anything";
      return false;
    }
    return true;
  };

  for (const Asset &shown : scenario.Assets) {
    if (shown.Kind != "generated") { continue; }
    if (!asked(shown.Uri)) { return false; }
  }
  for (const Generator &named : scenario.Generators) {
    if (!asked(named.Kind)) { return false; }
  }
  return made.Parts() == 0 || Stands(made);
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

bool Engine::Stands(const Geometry &geometry) {
  if (!S_->Standing) {
    S_->Error = "no scenario stands, so there is nothing for geometry to be handed to -- Declare "
                "before Stands";
    return false;
  }
  if (!geometry.Whole()) {
    S_->Error = "the geometry stands no whole part, and a subject of nothing is a refusal rather "
                "than an empty picture";
    return false;
  }
  Gltf::Subject handed;
  if (!handed.Assemble(geometry)) {
    S_->Error = handed.Error();
    return false;
  }
  return S_->Standing->Restand(handed, 0, S_->Error);
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


const std::vector<Measure> &Engine::Numbers() const { return S_->Numbers; }

bool Engine::Takes(std::string_view view) {
  if (!S_->Views) {
    S_->Error = "the scenario declares no views, so there is none to take";
    return false;
  }
  if (!S_->Views->Take(view)) {
    S_->Error = "the scenario declares no view by that name";
    return false;
  }
  return true;
}

void Engine::State::Places(const char *what, double how, const char *unit) {
  for (size_t at = Standing_Placed; at < Numbers.size(); ++at) {
    if (Numbers[at].What == what) {
      Numbers[at].How = how;
      return;
    }
  }
  Numbers.push_back(Measure{what, how, unit});
}

bool Engine::State::Rides(void) {
  return Places(Drive.State.Body, Drive.Stood.ModelShiftM);
}

bool Engine::State::Places(const Physics::Body &body, const double shiftM[3]) {
  double bodyFromWorld[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  {
    const double *const q = body.OrientationQ;
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    bodyFromWorld[0] = 1.0 - 2.0 * (y * y + z * z);
    bodyFromWorld[1] = 2.0 * (x * y + z * w);
    bodyFromWorld[2] = 2.0 * (x * z - y * w);
    bodyFromWorld[4] = 2.0 * (x * y - z * w);
    bodyFromWorld[5] = 1.0 - 2.0 * (x * x + z * z);
    bodyFromWorld[6] = 2.0 * (y * z + x * w);
    bodyFromWorld[8] = 2.0 * (x * z + y * w);
    bodyFromWorld[9] = 2.0 * (y * z - x * w);
    bodyFromWorld[10] = 1.0 - 2.0 * (x * x + y * y);
  }
  for (int axis = 0; axis < 3; ++axis) {
    bodyFromWorld[12 + axis] = body.PositionM[axis] + bodyFromWorld[0 + axis] * shiftM[0] +
                               bodyFromWorld[4 + axis] * shiftM[1] +
                               bodyFromWorld[8 + axis] * shiftM[2];
  }

  const double stillM[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  if (!Standing->Carry(bodyFromWorld, stillM, Error)) { return false; }
  Places("the body, east", body.PositionM[0], "m");
  Places("the body, up", body.PositionM[1], "m");
  Places("the body, south", body.PositionM[2], "m");
  Places("the mesh it carries, east", bodyFromWorld[12], "m");
  Places("the mesh it carries, up", bodyFromWorld[13], "m");
  Places("the mesh it carries, south", bodyFromWorld[14], "m");
  if (Volumes) {
    Volumes->Probe(0, body.PositionM, (double)Standing->At() * Declared.Motion.StepS);
    for (const TriggerField::Fired &fired : Volumes->Drain()) {
      ++Fired;
      Places("events a declared volume has fired", (double)Fired, "events");
      Carried.push_back("a volume fired event " + std::to_string(fired.Event) +
                             " for body " + std::to_string(fired.Body));
    }
  }
  if (!Views) { return true; }

  const View &seen = Views->Active();
  const double *const centreM = Drive.Stood.CentreM;
  const double seatM[3] = {seen.OffsetM[0] - centreM[0], seen.OffsetM[1] - centreM[1],
                           seen.OffsetM[2] - centreM[2]};
  double at[3];
  for (int axis = 0; axis < 3; ++axis) {
    at[axis] = body.PositionM[axis] + bodyFromWorld[0 + axis] * seatM[0] +
               bodyFromWorld[4 + axis] * seatM[1] + bodyFromWorld[8 + axis] * seatM[2];
  }
  const double ahead[3] = {at[0] - bodyFromWorld[8], at[1] - bodyFromWorld[9],
                           at[2] - bodyFromWorld[10]};
  double eye[3] = {at[0], at[1], at[2]};
  if (seen.DistanceM > 0.0) {
    const double back = seen.DistanceM;
    for (int axis = 0; axis < 3; ++axis) {
      eye[axis] = at[axis] + bodyFromWorld[8 + axis] * back +
                  bodyFromWorld[4 + axis] * back * seen.RisesBy;
    }
  }
  Places("the eye, east", eye[0], "m");
  Places("the eye, up", eye[1], "m");
  Places("the eye, south", eye[2], "m");
  Gltf::Placement from;
  if (!Gltf::Placement::LookAt(eye, seen.DistanceM > 0.0 ? at : ahead, 0.0, from)) {
    return true;
  }
  from.YfovRad = (seen.FovDeg > 0.0 ? seen.FovDeg : 55.0) * std::numbers::pi / 180.0;
  Standing->Eye(from);
  return true;
}

bool Engine::Advance() {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to advance";
    return false;
  }
  if (S_->Drove) {
    if (S_->Steps >= S_->MostSteps) {
      S_->Error = "the drive has taken " + Said((double)S_->Steps) +
                  " steps and its own plan allows " + Said((double)S_->MostSteps) +
                  " at the slowest station on it, so it is not arriving";
      return false;
    }
    ++S_->Steps;
    const Sim::Ridden &rode =
        Sim::DriveTick(S_->Drive.Way, S_->Drive.Stood, *S_->Surface, S_->Drive.State,
                       S_->Declared.Motion.StepS, nullptr);
    if (!rode.Found || rode.Lost) {
      S_->Error = "the drive left its corridor at " + Said(rode.ReachedM) + " m";
      return false;
    }
    if (rode.Arrived) {
      S_->Places("wheel-steps that asked the ground what it is", (double)rode.GroundAsked, "steps");
      S_->Places("steps it could answer", (double)rode.GroundAnswered, "steps");
      return false;
    }
    if (!S_->Rides()) { return false; }
  }
  if (S_->Drove) {
    S_->Places("how far along it the body has come", S_->Drive.State.Tally.ReachedM, "m");
    S_->Places("ticks the one lane task has kept", (double)S_->Drive.State.Kept, "ticks");
    S_->Places("bytes the world holds while it drives", (double)HeapProbe::LiveBytes(), "bytes");
  }
  S_->Falls();
  if (!S_->Drove && !S_->Freestanding.empty() && S_->Standing->Stands()) {
    const double unshifted[3] = {0.0, 0.0, 0.0};
    if (!S_->Places(S_->Freestanding.front(), unshifted)) { return false; }
  }
  if (!S_->Standing->Advance(S_->Error)) { return false; }
  return true;
}

void Engine::State::Falls(void) {
  if (Freestanding.empty()) { return; }
  const double stepS = Declared.Motion.StepS > 0.0 ? Declared.Motion.StepS : 1.0 / 60.0;
  const double gravityMs2 =
      Declared.Ground.GravityMs2 > 0.0 ? Declared.Ground.GravityMs2 : 9.80665;
  for (Physics::Body &held : Freestanding) {
    Physics::Wrench pulled;
    pulled.ForceN[1] = -held.MassKg * gravityMs2;
    Physics::Step(held, pulled, stepS);
  }
  Places("bodies standing on no route", (double)Freestanding.size(), "bodies");
  Places("the first of them, up", Freestanding.front().PositionM[1], "m");
  Places("and how fast it falls", Freestanding.front().VelocityMs[1], "m/s");
}

void Engine::State::Drew(void) {
  Places("batches the picture draws", (double)Device.SubjectBatchCount(), "batches");
  Places("batches the shadow casts", (double)Device.ShadowCastCount(), "batches");
  Places("placement rows the renderer has been sent", (double)Device.SubjectPlacementsMoved(),
         "rows");
  Places("frames the subject drew shadowed", (double)Device.ShadowedFrames(), "frames");
  {
    std::vector<float> depth;
    if (Steps < 2 && Device.ReadShadowAtlas(depth) == Render::ReadState::Ready) {
      double least = 1.0e30, most = -1.0e30, written = 0.0;
      for (const float one : depth) {
        if ((double)one < least) { least = (double)one; }
        if ((double)one > most) { most = (double)one; }
        if (one > 0.0f) { written += 1.0; }
      }
      Places("the shadow atlas, least depth", least, "");
      Places("its most", most, "");
      Places("texels above the clear", written, "texels");
      Places("the shadow radius it stood on", Standing->ShadowRadiusStanding(), "m");
    }
    Places("bytes the frame's drawing left behind", (double)Clients::Live::TookDrawing(),
           "bytes");
    Places("its centre, east", Standing->ShadowCentreStanding()[0], "m");
    Places("its centre, up", Standing->ShadowCentreStanding()[1], "m");
    if (Steps < 2) {
      Places("the exposure the picture applied", (double)Device.ExposureApplied(), "1/(cd/m2)");
      std::vector<float> linear;
      if (Device.ReadSceneLinear(linear) == Render::ReadState::Ready) {
        double brightest = 0.0;
        for (size_t at = 0; at + 3 < linear.size(); at += 4) {
          for (int channel = 0; channel < 3; ++channel) {
            brightest = (double)linear[at + channel] > brightest ? (double)linear[at + channel]
                                                                 : brightest;
          }
        }
        Places("the brightest the scene's linear buffer reached", brightest, "");
      }
      std::vector<uint8_t> shown;
      if (Device.ReadPixels(shown) == Render::ReadState::Ready) {
        double peak = 0.0;
        for (size_t at = 0; at + 3 < shown.size(); at += 4) {
          for (int channel = 0; channel < 3; ++channel) {
            peak = (double)shown[at + channel] > peak ? (double)shown[at + channel] : peak;
          }
        }
        Places("the brightest the presented frame shows", peak, "of 255");
      }
    }
  }
}

double Engine::StepS(void) const { return S_->Declared.Motion.StepS; }

bool Engine::Advance(double elapsedS) {
  if (elapsedS > 0.0) { S_->OwedS += elapsedS; }
  bool stood = true;
  for (int step = 0; step < S_->Declared.Motion.MostStepsInArrears && S_->OwedS >= S_->Declared.Motion.StepS; ++step) {
    S_->OwedS -= S_->Declared.Motion.StepS;
    stood = Advance();
    if (!stood) { break; }
  }
  if (S_->OwedS > S_->Declared.Motion.MostStepsInArrears * S_->Declared.Motion.StepS) { S_->OwedS = 0.0; }
  return stood;
}

bool Engine::Run() {
  if (!S_->Standing) {
    S_->Error = "no scenario is standing, so there is nothing to run";
    return false;
  }
  while (Advance()) {
  }
  return S_->Error.empty();
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

bool Engine::Standing() const { return S_->Standing != nullptr; }
const std::string &Engine::Error() const { return S_->Error; }

}
