#ifndef OUTSHINE_ENGINE_ENGINEHELD_H
#define OUTSHINE_ENGINE_ENGINEHELD_H

#include <algorithm>
#include <array>
#include <string_view>
#include <cstdint>
#include <Outshine.h>
#include "math/Units.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Fetching.h"
#include "HeapProbe.h"
#include "Shipped.h"
#include "RoadMesher.h"
#include "StructureMesher.h"
#include "Wayfinding.h"
#include "Rigid.h"
#include "GroundSnapshot.h"
#include "RegionPool.h"
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
#include "GroundMesher.h"
#include "spatial/Drape.h"
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
  if (under.empty() || named.empty() || named.front() == '/' || named.contains("://")) {
    return named;
  }
  return under.back() == '/' ? under + named : under + "/" + named;
}

[[nodiscard]] inline std::expected<std::string, std::string> SlurpFile(const std::string &held) {
  std::FILE *const file = std::fopen(held.c_str(), "rb");
  if (file == nullptr) { return std::unexpected(held + ": no scenario at that path"); }
  std::string text;
  std::array<char, 4096> block{};
  size_t read = 0;
  while ((read = std::fread(block.data(), 1, block.size(), file)) > 0) {
    text.append(block.data(), read);
  }
  std::fclose(file);
  return text;
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
  if (scenario.Motion.Declared) { carried.emplace_back("a physics dial"); }
  if (scenario.Time.Declared) { carried.emplace_back("a clock"); }
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
  Column<Traits> Kinds;
  Assembled Stood;
};

struct Surrounds {
  std::unique_ptr<Data::Transport> Wire;
  Ground::GroundStack Stack;
  Generators::Registry Offering;
  Generators::Shipping Shipping;

  struct Standing {
    uint32_t Body = 0;
    uint32_t Cluster = 0;
    Generators::Scattered Where;
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
  std::chrono::steady_clock::time_point LaidAt;
  std::shared_ptr<const Generators::GroundTable> Table;
  size_t GroundTiles = 0;
  size_t Placed = 0;
  size_t Instanced = 0;
  int Reached = 0;
  TriangleBvh Blocking;
};

struct Spent {
  class Counter {
  public:
    [[nodiscard]] double LastMs() const { return LastMs_; }

    [[nodiscard]] double LeastMs() const { return LeastMs_; }

    [[nodiscard]] double MostMs() const { return MostMs_; }

    [[nodiscard]] uint64_t Taken() const { return Count_; }

    void Took(double ms) {
      LastMs_ = ms;
      LeastMs_ = Count_ == 0 || ms < LeastMs_ ? ms : LeastMs_;
      MostMs_ = std::max(ms, MostMs_);
      ++Count_;
      if (Kept_.empty()) { return; }
      Kept_[At_] = ms;
      At_ = At_ + 1 == Kept_.size() ? 0 : At_ + 1;
      Filled_ = Filled_ || At_ == 0;
    }

    void Keeps(size_t deep) {
      Kept_.assign(deep, 0.0);
      At_ = 0;
      Filled_ = false;
    }

    void Into(std::vector<double> &out) const {
      if (Kept_.empty()) {
        out.clear();
        return;
      }
      if (!Filled_) {
        out.assign(Kept_.begin(), Kept_.begin() + static_cast<long>(At_));
        return;
      }
      out.assign(Kept_.begin(), Kept_.end());
      if (At_ != 0) { std::ranges::rotate(out, out.begin() + static_cast<long>(At_)); }
    }

  private:
    double LastMs_ = 0.0;
    double LeastMs_ = 0.0;
    double MostMs_ = 0.0;
    uint64_t Count_ = 0;
    std::vector<double> Kept_;
    size_t At_ = 0;
    bool Filled_ = false;
  };

  Counter Advance;
  double StreamedMs = 0.0;
  size_t StreamedTiles = 0;
  Counter Render;
};

struct Ticks {
  std::vector<Physics::Rigid> Freestanding;
  double OwedS = 0.0;

  double ElapsedS = 0.0;
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
  [[nodiscard]] bool Watches();

  struct Classed {
    std::vector<float> Tinted;
    std::vector<float> Uv;
    std::vector<float> Palette;
    std::shared_ptr<const ClassStructure> Structure;
  };

  static constexpr size_t kPaletteStride = 4u;

  [[nodiscard]] static std::vector<float> PaletteOver(const Ground::VegetationTemplates &wearing,
                                                      const Render::Medium &fallback);

  struct Dividing {
    Patchwork &Laid;
    std::vector<float> &InFrame;
    std::vector<float> &Tinted;
    std::vector<float> &Uv;
  };

  struct Halving {
    uint32_t From = 0;
    uint32_t To = 0;
  };

  [[nodiscard]] size_t NamesEveryVertex(Dividing over,
                                        const ClassStructure &classes,
                                        std::vector<int> &classOf,
                                        std::vector<double> &atGeo) const;

  [[nodiscard]] uint32_t HalvesEdge(Dividing over,
                                    const ClassStructure &classes,
                                    Halving across,
                                    std::vector<int> &classOf,
                                    std::vector<double> &atGeo) const;

  [[nodiscard]] size_t DividesAtClassEdges(Dividing over,
                                           const ClassStructure &classes,
                                           std::vector<int> &classOf,
                                           std::vector<double> &atGeo) const;

  [[nodiscard]] Classed Classify(Patchwork &laid, std::vector<float> &inFrame);

  struct Phasing {
    std::chrono::steady_clock::time_point PhaseAt;
    std::chrono::steady_clock::time_point CensusAt;
    std::chrono::steady_clock::time_point WiresAt;
  };

  void TellsHowTheRingSinks(std::span<const float> inFrame);
  void TellsWhatTheGroundHolds(const TangentFrame &standing, std::span<const float> inFrame);
  void Models(const TangentFrame &standing,
              std::span<const float> inFrame,
              LongitudeLatitude stands,
              Geometry &ground,
              Phasing &clocks);

  struct Meets {
    double EastM = 0.0;
    double SouthM = 0.0;
    uint64_t Named = 0;
  };

  struct Paving {
    const Ground::StreetField &Ways;
    const Ground::OsmField &Vectors;
    std::span<const double> Points;
    const std::unordered_map<uint64_t, uint32_t> &SharedNodes;
    const Drape &Draped;
    const TangentFrame &Standing;
    const std::shared_ptr<const ClassStructure> &Classes;
    int WaterRow = -1;
  };

  struct Paved {
    std::vector<std::vector<RoadStation>> Designed;
    std::vector<RoadStation> Along;
    std::vector<RoadStation> Finer;
    double FitRadiusTightestM = 0.0;
    size_t FitsMeasured = 0;
    std::vector<double> FitEastNorth;
    double TightestDemandM = 0.0;
    std::vector<double> DeckM;
    std::vector<double> TrimM;
    std::unordered_map<uint64_t, std::vector<Meets>> AtCrossing;
    std::unordered_map<uint64_t, std::vector<RoadGate>> Gates;
    std::unordered_map<uint64_t, double> EndM;
    std::unordered_map<uint64_t, double> GroundEndM;
    RoadTallied Swept;
    size_t ChordAdded = 0;
    size_t DecksOverWater = 0;
    size_t AskedOverBridge = 0;
    size_t NamedOverBridge = 0;
    size_t WetOverBridge = 0;
    size_t RefusedWays = 0;
    size_t LaidWays = 0;
    size_t GroundWays = 0;
    size_t FitLaid = 0;
    size_t FitRefused = 0;
    size_t FitUndrivable = 0;
    size_t FitTooTight = 0;
    size_t FitUnsplittable = 0;
    size_t FitCuts = 0;
    size_t CrossingsSeen = 0;
    size_t PairsTested = 0;
    size_t PairsPruned = 0;
    size_t FullestCell = 0;
    double CrossNetworkMs = 0.0;
    double CrossSweepMs = 0.0;
    double CrossFilingMs = 0.0;
    double CrossDecksMs = 0.0;
    size_t DecksRaised = 0;
    double MostRaisedM = 0.0;
    size_t RampsRaised = 0;
    double SteepestRamp = 0.0;
    size_t EndsTrimmed = 0;
    size_t EndsStillCrossing = 0;
    double SharpestForkDeg = 0.0;
    double DeepestTrimM = 0.0;
    double ShortestByM = 0.0;
    double LongestShortM = 0.0;
    size_t CapsBit = 0;
    double MostOverWaterM = 0.0;
    double FitMs = 0.0;
    double WaterMs = 0.0;
    double SweepMs = 0.0;
    double RestMs = 0.0;
  };

  static void RefineChords(const Paving &on, Paved &into);

  struct Spanning {
    size_t Here = 0;
    size_t Next = 0;
  };

  [[nodiscard]] static size_t StepsAcross(const Paving &on, Spanning between);

  [[nodiscard]] static uint64_t SharedNodeAt(const Paving &on, double latDeg, double lonDeg);

  [[nodiscard]] static bool
  StationsAlong(const Paving &on,
                const Ground::StreetField::Way &lane,
                const std::function<bool(LongitudeLatitude, uint64_t)> &station);

  void DesignLane(const Paving &on,
                  const Ground::StreetField::Way &lane,
                  size_t laneAt,
                  Paved &into) const;

  enum class Laid : uint8_t { Refused, Unchanged, Wanted };

  [[nodiscard]] Laid Focuses(const Around &over, LongitudeLatitude at, bool alsoWhenTilesLanded);

  static void FitAlongLane(Paved &into);
  static void TrimLaneEnds(size_t laneAt, Paved &into);
  static void FitLane(size_t laneAt, Paved &into);

  static void LayLanesIntoNetwork(const Ground::StreetField &ways,
                                  std::span<const double> points,
                                  Path::Network &net,
                                  std::vector<size_t> &netToLane);
  static void
  FileCrossing(const Path::Network::Crossing &one, const TangentFrame &standing, Paved &into);
  void RaiseDeckOver(const Path::Network::Crossing &one,
                     const Ground::StreetField &ways,
                     std::span<const size_t> netToLane,
                     const TangentFrame &standing,
                     const Drape &drapedOver,
                     Paved &into) const;
  void Crosses(const Ground::StreetField &ways,
               const Ground::OsmField &vectors,
               const TangentFrame &standing,
               const Drape &drapedOver,
               Paved &into) const;

  struct Ends {
    std::array<uint64_t, 2> Key{};
    std::array<double, 4> At{};
  };

  [[nodiscard]] static std::optional<Ends> EndsOf(const Ground::OsmField &vectors,
                                                  const Ground::StreetField::Way &lane);

  struct Grounded {
    double EastM = 0.0;
    double SouthM = 0.0;
    double GradeM = 0.0;
  };

  [[nodiscard]] std::optional<Grounded>
  GroundUnder(const TangentFrame &standing, const Drape &drapedOver, LongitudeLatitude at) const;

  static void RaisesEnds(std::span<const uint64_t> key, double deckM, Paved &into);

  [[nodiscard]] static double HighestDeckM(const Paved &over);

  static void EasesRamps(const Ground::StreetField &ways,
                         const Ground::OsmField &vectors,
                         double mostDeckM,
                         Paved &into);

  void GradesApproaches(const Ground::StreetField &ways,
                        const Ground::OsmField &vectors,
                        const TangentFrame &standing,
                        const Drape &drapedOver,
                        Paved &into) const;

  void SeedsBridgeEnds(const Ground::StreetField &ways,
                       const Ground::OsmField &vectors,
                       const TangentFrame &standing,
                       const Drape &drapedOver,
                       Paved &into) const;

  void Bridges(const Ground::StreetField &ways,
               const Ground::OsmField &vectors,
               const TangentFrame &standing,
               const Drape &drapedOver,
               Paved &into);

  static void
  Shortens(const Ground::StreetField &ways, const Ground::OsmField &vectors, Paved &into);

  [[nodiscard]] static double StepAlongM(std::span<const RoadStation> along, size_t at);

  [[nodiscard]] static std::vector<double> ReachedAlong(std::span<const RoadStation> along);

  void MarksWaterCrossing(const Paving &on, size_t laneAt, Paved &into) const;

  static void LevelsDeckOrApproach(const Paving &on,
                                   const Ground::StreetField::Way &lane,
                                   size_t laneAt,
                                   Paved &into);

  enum class Pass : uint8_t { Designing, Paving };

  [[nodiscard]] static constexpr std::string_view Doing(Pass pass) {
    return pass == Pass::Designing ? "designing" : "paving";
  }

  void PaveLane(const Paving &on,
                Pass pass,
                size_t laneAt,
                Paved &into,
                std::vector<Yields> &corridor,
                RoadRaised &pavement) const;

  [[nodiscard]] static double LevelsWhereWaysMeet(Paved &into);

  [[nodiscard]] size_t RaisesTheJunctionBodies(Paved &into, RoadRaised &pavement) const;

  void TellsWhatTheFitFound(Paved &into);
  void HandsThePavingOver(const RoadRaised &pavement, Geometry &ground);

  void Paves(const TangentFrame &standing,
             const std::shared_ptr<const ClassStructure> &classStructure,
             const Drape &drapedOver,
             std::vector<Yields> &corridor,
             RoadRaised &pavement,
             Geometry &ground,
             Phasing &clocks);

  struct Relieved {
    double Tallest = 0.0;
    double Lowest = 0.0;
    double TallestOutM = 0.0;
  };

  void TellsTheRelief(Relieved over);
  void TellsWhatCrossed(const Geometry &ground);
  [[nodiscard]] bool Grounds(bool alsoWhenTilesLanded);
  [[nodiscard]] bool Asks();
  [[nodiscard]] bool Carries(const Physics::Rigid &body, const Vec3 &shiftM);
  [[nodiscard]] bool Carries(size_t which, const Physics::Rigid &body, const Vec3 &shiftM);
  void Falls();
  [[nodiscard]] bool Composes();
  bool Grows(double atLat, double atLon);
  [[nodiscard]] bool GrowsOver(const Generators::Tile &region, Generators::Detail coarseness);
  [[nodiscard]] LongitudeLatitude WhereTheEyeStands() const;
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
