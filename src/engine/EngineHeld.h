#ifndef OUTSHINE_ENGINE_ENGINEHELD_H
#define OUTSHINE_ENGINE_ENGINEHELD_H

#include <array>
#include <Outshine.h>
#include "Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Fetching.h"
#include "HeapProbe.h"
#include "Shipped.h"
#include "BuildingMesh.h"
#include "RoadMesh.h"
#include "Wayfinding.h"
#include "RoofSurface.h"
#include "GroundSnapshot.h"
#include "RegionPool.h"
#include "Structures.h"
#include "Unwired.h"

#include <chrono>
#include <thread>
#include <numbers>
#include <charconv>
#include <cmath>
#include <vector>

#include "Assembly.h"
#include "TriangleBvh.h"
#include "Ledger.h"
#include "Mixer.h"
#include "Tables.h"
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
#include "GroundSupport.h"
#include "DriveAssembly.h"
#include "GroundPatchwork.h"
#include "TileGeodesy.h"
#include "SceneRenderer.h"
#include "ScenarioRead.h"

namespace outshine {

constexpr int kFrameUnsaidWidePx = 1280;
constexpr int kFrameUnsaidHighPx = 720;

inline constexpr size_t kParkedBound = 8;
inline constexpr size_t kMostSaveBytes = 1u << 20u;

class Collecting : public Sink {
public:
  void Number(const char *what, double how, const char *unit) override {
    std::string held = std::string(what) + " = " + Rounded(how);
    if (unit != nullptr && unit[0] != '\0') { held += " " + std::string(unit); }
    Held.push_back(std::move(held));
    Took.push_back(Measure{
        .What = what, .How = how, .Unit = unit == nullptr ? std::string() : std::string(unit)});
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

  [[nodiscard]] std::vector<std::string> &Lines() { return Held; }

  [[nodiscard]] std::vector<Measure> &Numbers() { return Took; }

private:
  std::vector<std::string> Held;
  std::vector<Measure> Took;

  [[nodiscard]] static std::string Rounded(double how) {
    std::array<char, 32> held{};
    std::snprintf(held.data(), held.size(), "%.6g", how);
    return held.data();
  }

  std::string Why;
};

[[nodiscard]] inline std::string Said(double value) {
  std::array<char, 32> held = {{}};
  std::snprintf(held.data(), held.size(), "%.5f", value);
  return held.data();
}

[[nodiscard]] inline std::string Beneath(const std::string &under, const std::string &named) {
  if (under.empty() || named.empty() || named.front() == '/' ||
      named.find("://") != std::string::npos) {
    return named;
  }
  return under.back() == '/' ? under + named : under + "/" + named;
}

[[nodiscard]] inline bool
SlurpFile(const std::string &held, std::string &text, std::string &error) {
  std::FILE *const file = std::fopen(held.c_str(), "rb");
  if (file == nullptr) {
    error = held + ": no scenario at that path";
    return false;
  }
  text.clear();
  std::array<char, 4096> block{};
  size_t read = 0;
  while ((read = std::fread(block.data(), 1, block.size(), file)) > 0) {
    text.append(block.data(), read);
  }
  std::fclose(file);
  return true;
}

class Forwarding final : public Script::Host {
public:
  explicit Forwarding(outshine::Host *client) : Client_(client) {}

  [[nodiscard]] Script::Value Global(std::string_view name) override {
    for (size_t at = 0; at < Named_.size(); ++at) {
      if (Named_[at] == name) { return Script::Value::OfRef(static_cast<int>(at) + 1); }
    }
    Named_.emplace_back(name);
    return Script::Value::OfRef(static_cast<int>(Named_.size()));
  }

  [[nodiscard]] bool Call(const Script::Value &callee,
                          std::span<const Script::Value> args,
                          Script::Value &out) override {
    out = Script::Value();
    if (Client_ == nullptr || callee.What != Script::Kind::Ref) { return false; }
    const size_t which = static_cast<size_t>(callee.Ref) - 1;
    if (callee.Ref <= 0 || which >= Named_.size()) { return false; }

    std::vector<Argument> handed(args.size());
    for (size_t at = 0; at < args.size(); ++at) {
      if (args[at].What == Script::Kind::Text) {
        handed[at].Is = Argument::Kind::Text;
        handed[at].Text = args[at].Text;
      } else {
        handed[at].Is = Argument::Kind::Number;
        handed[at].Number = args[at].Number;
      }
    }
    Fired_ = true;
    return Client_->calls(Named_[which], handed);
  }

  [[nodiscard]] bool Fired() const { return Fired_; }

private:
  outshine::Host *Client_ = nullptr;
  std::vector<std::string> Named_;
  bool Fired_ = false;
};

inline std::vector<std::string> Unacted(const Scenario::Document &scenario) {
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
  note(scenario.Bodies.size(), "bodies");
  note(scenario.Tables.size(), "tables");
  note(scenario.Buses.size(), "buses");
  note(scenario.Sounds.size(), "sounds");
  note(scenario.State.size(), "persisted values");
  if (scenario.Motion.Declared) { carried.push_back("a physics dial"); }
  if (scenario.Time.Declared) { carried.push_back("a clock"); }
  if (scenario.Assets.size() > 1) {
    carried.push_back(std::to_string(scenario.Assets.size() - 1) + " assets beside the subject");
  }
  carried.insert(carried.end(), quiet.begin(), quiet.end());
  return carried;
}

struct Seen {
  Render::SceneRenderer Device;
  std::unique_ptr<Core::Live> Standing;
  Extent Frame{.WidthPx = kFrameUnsaidWidePx, .HeightPx = kFrameUnsaidHighPx};
  bool Targeted = false;
  bool FrameOpen = false;
  Core::Declaration Shown;
  Ui::Typeface Face;
  Gltf::Subject Handed;
  bool Carrying = false;
};

struct Kept {
  Scenario::Document Declared;
  bool Taken = false;
  std::vector<std::string> Carried;
  std::vector<Scenario::Document> Asleep;
  std::vector<std::string> LayerTrace;
  Roots Under;
  std::optional<ViewBook> Views;
  InputMap Bound;
  Core::InputPump Pump;
  bool Pumping = false;
  std::optional<TriggerField> Volumes;
  size_t Fired = 0;
  std::optional<TableBook> Tabled;
  Audio::Mixer Sounding;
  bool Mixing = false;
  std::array<std::vector<Audio::Heard>, 2> Sources;
  std::array<Audio::Listening, 2> Ear{};
  std::atomic<unsigned> Told{0};
};

struct Players {
  Scene Scene;
  Column<Scenario::Body> Bodies;
  Column<Scenario::Journey> Drives;
  Column<Traits> Kinds;
  Assembled Stood;
};

struct Surrounds {
  std::unique_ptr<Data::Transport> Wire;
  Ground::GroundStack Stack;
  Generators::Structures Shipped;
  Generators::Registry Offering;
  Generators::Shipping Shipping;

  struct Standing {
    uint32_t Body = 0;
    uint32_t Cluster = 0;
    Generators::Instance Where;
  };

  std::vector<Standing> Instances;
  size_t Pending = 0;
  size_t Bare = 0;
  size_t Wanted = 0;
  size_t AskedPending = 0;
  size_t AskedWanted = 0;
  uint64_t LaidFrom = 0;
  size_t LaidResident = 0;
  uint64_t LaidClasses = 0;

  std::vector<float> WallPlaces, WallFacing, RoofPlaces, RoofFacing;
  size_t WallCarried = 0, RoofCarried = 0;
  Vec2 CarriedFrom = {{kBeyondAnyCoordinate, kBeyondAnyCoordinate}};
  bool EverLaid = false;
  size_t Relaid = 0;
  size_t Asked = 0;
  double RebuildMs = 0.0;
  size_t Rebuilds = 0;
  bool Grown = false;
  Generators::BuildingMesh Shaper;
  std::chrono::steady_clock::time_point LaidAt{};
  std::shared_ptr<const Generators::GroundTable> Table;
  size_t GroundTiles = 0;
  size_t Placed = 0;
  size_t Instanced = 0;
  int Reached = 0;
  TriangleBvh Blocking;
};

struct Spent {
  struct Counter {
    double LastMs = 0.0;
    double LeastMs = 0.0;
    double MostMs = 0.0;
    uint64_t Count = 0;

    std::vector<double> Kept;
    size_t At = 0;

    void Took(double ms) {
      LastMs = ms;
      LeastMs = Count == 0 || ms < LeastMs ? ms : LeastMs;
      MostMs = ms > MostMs ? ms : MostMs;
      ++Count;
      if (Kept.empty()) { return; }
      Kept[At] = ms;
      At = At + 1 == Kept.size() ? 0 : At + 1;
      Filled_ = Filled_ || At == 0;
    }

    void Keeps(size_t deep) {
      Kept.assign(deep, 0.0);
      At = 0;
      Filled_ = false;
    }

    void Into(std::vector<double> &out) const {
      if (Kept.empty()) {
        out.clear();
        return;
      }
      if (!Filled_) {
        out.assign(Kept.begin(), Kept.begin() + static_cast<long>(At));
        return;
      }
      out.assign(Kept.begin(), Kept.end());
      if (At != 0) { std::rotate(out.begin(), out.begin() + static_cast<long>(At), out.end()); }
    }

  private:
    bool Filled_ = false;
  };

  Counter Advance;
  Counter Render;
};

struct Ticks {
  Sim::DriveProduct Drive;
  std::vector<Physics::Rigid> Freestanding;
  std::unique_ptr<Sim::GroundSupport> Surface;
  bool Drove = false;
  double OwedS = 0.0;

  double ElapsedS = 0.0;
  size_t Steps = 0;
  size_t MostSteps = 0;
};

struct Engine::State {
  Seen Picture;
  Kept Session;
  Players Cast;
  Surrounds World;
  Spent Cost;
  Ticks Ticking;
  Core::Ledger Published;
  Host *Offered = nullptr;
  std::string Error;

  void Drew();
  void Inspected();
  [[nodiscard]] bool Rides();
  [[nodiscard]] bool Watches();
  [[nodiscard]] bool Grounds(bool alsoWhenTilesLanded);
  [[nodiscard]] bool Asks();
  [[nodiscard]] bool Carries(const Physics::Rigid &body, const Vec3 &shiftM);
  [[nodiscard]] bool Carries(size_t which, const Physics::Rigid &body, const Vec3 &shiftM);
  void Falls();
  [[nodiscard]] bool Composes();
  bool Grows(double atLat, double atLon);
  void WhereTheEyeStands(double &atLat, double &atLon) const;
  [[nodiscard]] bool Stood();
  [[nodiscard]] bool Updates();
  [[nodiscard]] bool Draws();
  void Tells();
  void Blocks(const Gltf::Subject &standing);
  [[nodiscard]] bool Blocked(const Vec3 &sourceM) const;
  [[nodiscard]] bool Routes();
};

} // namespace outshine
#endif
