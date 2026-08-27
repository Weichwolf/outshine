#ifndef OUTSHINE_ENGINE_ENGINEHELD_H
#define OUTSHINE_ENGINE_ENGINEHELD_H

#include <Outshine.h>
#include "Fetching.h"
#include "HeapProbe.h"
#include "Structures.h"
#include "Unwired.h"

#include <algorithm>
#include <chrono>
#include <thread>
#include <numbers>
#include <charconv>
#include <cmath>
#include <vector>

#include "Assembly.h"
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
#include "GroundUnderfoot.h"
#include "DriveAssembly.h"
#include "GroundPatchwork.h"
#include "TileGeodesy.h"
#include "Renderer.h"
#include "ScenarioRead.h"


namespace outshine {

inline constexpr size_t kParkedBound = 8;
inline constexpr size_t kMostSaveBytes = 1 << 20;


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
  [[nodiscard]] std::vector<std::string> &Lines() { return Held; }
  [[nodiscard]] std::vector<Measure> &Numbers() { return Took; }

private:
  std::vector<std::string> Held;
  std::vector<Measure> Took;

  [[nodiscard]] static std::string Rounded(double how) {
    char held[32];
    std::snprintf(held, sizeof held, "%.6g", how);
    return held;
  }

  std::string Why;
};


[[nodiscard]] inline std::string Said(double value) {
  char held[32] = {};
  std::snprintf(held, sizeof held, "%.5f", value);
  return held;
}

[[nodiscard]] inline std::string Beneath(const std::string &under, const std::string &named) {
  if (under.empty() || named.empty() || named.front() == '/' ||
      named.find("://") != std::string::npos) {
    return named;
  }
  return under.back() == '/' ? under + named : under + "/" + named;
}

[[nodiscard]] inline bool SlurpFile(const std::string &held, std::string &text, std::string &error) {
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

inline std::vector<std::string> Unacted(const Scenario &scenario) {
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
  Render::Renderer Device;
  std::unique_ptr<Core::Live> Standing;
  Extent Frame{1280, 720};
  bool Canvas = false;
  Core::Declaration Shown;
  Ui::Typeface Face;
  Gltf::Subject Handed;
  bool Carrying = false;
};

struct Kept {
  Scenario Declared;
  std::vector<std::string> Carried;
  std::vector<Scenario> Asleep;
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
};

struct Players {
  Store Scene;
  Column<Body> Bodies;
  Column<Journey> Drives;
  Column<Traits> Kinds;
  Assembled Stood;
};

struct Surrounds {
  std::unique_ptr<Data::Transport> Wire;
  Ground::GroundStack Stack;
  Generators::Structures Shipped;
  std::vector<const Generates *> Making;
  size_t GroundTiles = 0;
};

struct Ticks {
  Sim::DriveProduct Drive;
  std::vector<Physics::Rigid> Freestanding;
  std::unique_ptr<Sim::GroundUnderfoot> Surface;
  bool Drove = false;
  double OwedS = 0.0;
  size_t Steps = 0;
  size_t MostSteps = 0;
};

struct Engine::State {
  Seen Picture;
  Kept Session;
  Players Cast;
  Surrounds World;
  Ticks Ticking;
  Core::Ledger Published;
  Host *Offered = nullptr;
  std::string Error;

  void Drew(void);
  [[nodiscard]] bool Rides(void);
  [[nodiscard]] bool Carries(const Physics::Rigid &body, const double shiftM[3]);
  void Falls(void);
  [[nodiscard]] bool Composes(void);
  [[nodiscard]] bool Stood(void);
  [[nodiscard]] bool Updates(void);
  [[nodiscard]] bool Draws(void);
  [[nodiscard]] bool Routes(void);
};


}
#endif
