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
#include "VegetationStreaming.h"
#include "WorldReadiness.h"
#include "GroundPublication.h"
#include "StructureMesher.h"
#include "Rigid.h"
#include "GroundSnapshot.h"
#include "RegionPool.h"
#include "Unwired.h"
#include "HeightSheets.h"
#include "Wayfinding.h"
#include "TilePieces.h"
#include "WorldPlacement.h"
#include "StructureBuildQueue.h"
#include "RoadAlignmentBuildQueue.h"
#include "RouteSpeedProfile.h"
#include "Tasks.h"
#include "Log.h"

#include <chrono>
#include <thread>
#include <numbers>
#include <charconv>
#include <cmath>
#include <vector>

#include "Assembly.h"
#include "SimulationState.h"
#include "TriangleBvh.h"
#include "Ledger.h"
#include "Mixer.h"
#include "Tables.h"
#include "ScenarioLayer.h"
#include "RuntimeScene.h"
#include "Typeface.h"
#include "InputPump.h"
#include "Triggers.h"
#include "Views.h"
#include "Sink.h"
#include "DeclaredSources.h"
#include "GroundStack.h"
#include "OsmTransportLoader.h"
#include "GroundMesher.h"
#include "spatial/Drape.h"
#include "TileGeodesy.h"
#include "SceneRenderer.h"
#include "ScenarioRead.h"

namespace outshine {
struct GroundBuildProducts;
class GroundBuildState;
class GroundWorldCandidate;

constexpr int kFrameUnsaidWidePx = 1280;
constexpr int kFrameUnsaidHighPx = 720;

inline constexpr size_t kBakesLandedPerFrame = 2;
inline constexpr size_t kBakesLandedInPreload = size_t{1} << 20u;
inline constexpr size_t kMostSaveBytes = 1u << 20u;
inline constexpr size_t kMostScenarioBytes = 16u << 20u;

class Collecting : public Sink {
public:
  void Number(const char *what, double how, const char *unit) override {
    std::string held = std::string(what) + " = " + Rounded(how);
    if (unit != nullptr && unit[0] != '\0') { held += " " + std::string(unit); }
    Held.push_back(std::move(held));
    Took.push_back(DiagnosticSample{
        .Name = what, .Value = how, .Unit = unit == nullptr ? std::string() : std::string(unit)});
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

  [[nodiscard]] std::vector<DiagnosticSample> &Numbers() { return Took; }

private:
  std::vector<std::string> Held;
  std::vector<DiagnosticSample> Took;

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
  note(scenario.Generators.size(), "generators");
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

enum class FrameScope { Closed, Open, DrawSucceeded };

struct Seen {
  Render::SceneRenderer Device;
  std::unique_ptr<Core::RuntimeScene> Standing;
  Extent Frame{.WidthPx = kFrameUnsaidWidePx, .HeightPx = kFrameUnsaidHighPx};
  bool Targeted = false;
  FrameScope Scope = FrameScope::Closed;
  Core::Declaration Shown;
  Ui::Typeface Face;
  std::optional<Geometry> PendingGeometry;
  std::optional<TriangleBvh> PendingAudioOcclusion;
};

struct Kept {
  Scenario::Document Declared;
  uint64_t DeclarationRevision = 0;
  bool Taken = false;
  std::vector<std::string> Carried;
  std::vector<std::string> LayerTrace;
  Roots Under;
  std::optional<ViewBook> Views;
  InputMap Bound;
  size_t Fired = 0;
  std::optional<Audio::Mixer> Sounding;
  std::vector<std::optional<size_t>> AudioBodies;
  std::array<std::vector<Audio::Heard>, 2> Sources;
  std::array<Audio::Listening, 2> Ear{};
  std::atomic<unsigned> Told{0};
};

struct Surrounds {
  Surrounds();
  ~Surrounds();
  Surrounds(const Surrounds &) = delete;
  Surrounds &operator=(const Surrounds &) = delete;
  Surrounds(Surrounds &&) = delete;
  Surrounds &operator=(Surrounds &&) = delete;

  void BindSceneResources(Render::SceneRenderer &renderer) noexcept {
    Pieces.Into(&renderer);
    Sheets.Into(&renderer);
    if (Vegetation) { Vegetation->Into(renderer); }
  }

  std::unique_ptr<Data::Transport> Wire;
  Ground::GroundStack Stack;
  Generators::Registry Offering;
  Generators::Shipping Shipping;

  using Standing = WorldInstance;

  std::vector<Standing> Instances;
  std::unique_ptr<VegetationStreaming> Vegetation;
  size_t Pending = 0;
  size_t Bare = 0;
  size_t Wanted = 0;
  size_t AskedPending = 0;
  size_t AskedWanted = 0;
  size_t AskedPlayablePending = 0;
  GroundPublication GroundPublished;
  std::optional<GroundRevision> RequestedRefinedGround;
  std::unique_ptr<GroundBuildState> GroundBuild;
  std::unique_ptr<GroundBuildState> GroundRetirement;
  size_t GroundCandidates = 0;

  TilePieces Pieces;
  std::optional<TilePieces::Surfaces> StructureSurfaces;
  HeightSheets Sheets;
  std::shared_ptr<const Path::Network> StreetGraph;
  size_t StreetGraphWayCount = 0;
  std::vector<NamedRoadAlignment> RoadAlignments;
  bool PiecesFramed = false;
  std::unique_ptr<Tasks> Pool;
  RoadAlignmentBuildQueue RoadAlignmentBuilds;
  std::unique_ptr<World::OsmTransportLoader> OsmTransportLoader;
  StructureBuildQueue StructureBuilds;
  size_t Relaid = 0;
  size_t Asked = 0;
  double RebuildMs = 0.0;
  size_t Rebuilds = 0;
  bool Grown = false;
  std::chrono::steady_clock::time_point LaidAt;
  std::shared_ptr<const Generators::GroundTable> Table;
  size_t GroundTiles = 0;
  size_t RimsMissing = 0;
  size_t Placed = 0;
  size_t Instanced = 0;
  int Reached = 0;
  TriangleBvh AudioOcclusion;
  std::vector<float> GroundPositionsM;
  std::vector<uint32_t> GroundIndex;
};

struct Spent {
  static constexpr size_t kGroundPhaseCount = 18;

  struct UpdateComponents {
    double TotalMs = 0.0;
    double StreamingMs = 0.0;
    double PieceHandoffMs = 0.0;
    double RestandMs = 0.0;
    double BakesMs = 0.0;
    double GrowthMs = 0.0;
    double SimulationMs = 0.0;
    double GroundMs = 0.0;
    double CrownsMs = 0.0;
  };

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
  Counter Update;
  Counter FramePublication;
  Counter SceneAdvance;
  Counter Streaming;
  Counter PieceHandoff;
  Counter Restand;
  Counter Bakes;
  Counter BakeResume;
  Counter BakeLanding;
  Counter BakeTransfer;
  Counter BakeLiveTransfer;
  Counter BakeCandidateTransfer;
  Counter BakeCommit;
  Counter BakePosting;
  Counter Growth;
  Counter Simulation;
  Counter Ground;
  Counter GroundRequest;
  Counter GroundBuildBegin;
  Counter GroundBuildCreate;
  Counter GroundBuildPrepare;
  Counter GroundRetirement;
  std::array<Counter, kGroundPhaseCount> GroundPhases;
  Counter Crowns;
  UpdateComponents WorstSuccessfulUpdate;
  double StreamedMs = 0.0;
  size_t StreamedTiles = 0;
  Counter Render;

  void ObservesSuccessfulUpdate(double milliseconds, bool streamsGround) noexcept {
    if (milliseconds <= WorstSuccessfulUpdate.TotalMs) { return; }
    WorstSuccessfulUpdate = {.TotalMs = milliseconds,
                             .StreamingMs = streamsGround ? Streaming.LastMs() : 0.0,
                             .PieceHandoffMs = streamsGround ? PieceHandoff.LastMs() : 0.0,
                             .RestandMs = streamsGround ? Restand.LastMs() : 0.0,
                             .BakesMs = streamsGround ? Bakes.LastMs() : 0.0,
                             .GrowthMs = streamsGround ? Growth.LastMs() : 0.0,
                             .SimulationMs = Simulation.LastMs(),
                             .GroundMs = Ground.LastMs(),
                             .CrownsMs = Crowns.LastMs()};
  }
};

struct Ticks {
  double OwedS = 0.0;

  double ElapsedS = 0.0;
};

struct RouteCameraMotion {
  std::string ViewId;
  std::string RouteId;
  RouteInfo Route;
  Motion::RouteSpeedProfile Speed;
  double BeganAtS = 0.0;
};

namespace Says {
inline constexpr std::string_view kCaptureMutation =
    "a capture holds the published world; end it before mutating engine state";
}

struct Engine::State {
  Seen Picture;
  Kept Session;
  std::unique_ptr<SimulationState> Simulation = std::make_unique<SimulationState>();
  Surrounds World;
  Spent Cost;
  Ticks Ticking;
  std::optional<RouteCameraMotion> RouteCamera;
  Core::Ledger Published;
  Host *Offered = nullptr;
  LogSink *Diagnostics = nullptr;
  std::string Error;
  bool Capturing = false;

  [[nodiscard]] LogThreadSinkScope Logs() const { return LogThreadSinkScope(Diagnostics); }

  [[nodiscard]] Result MutationPermission() {
    if (!Capturing) { return {}; }
    Error = Says::kCaptureMutation;
    return std::unexpected(Error);
  }

  void Drew();
  void Inspected();
  [[nodiscard]] WorldReadiness Readiness(GroundQuality quality = GroundQuality::Playable) const;
  [[nodiscard]] bool StructuresReady(const Ground::BuildingField &footprints,
                                     const GroundRevision &revision) const;
  [[nodiscard]] bool RefinedGroundIngested(const GroundRevision &revision) const;
  [[nodiscard]] bool RefinedGroundClassified(const GroundRevision &revision) const;
  [[nodiscard]] bool CanFinishPreload() const;
  [[nodiscard]] bool CanBeginGroundCandidate() const;
  [[nodiscard]] bool CanAdvanceGroundCandidate() const;
  [[nodiscard]] Result PumpPreload();
  [[nodiscard]] Result PreloadOverflow();
  enum class PreloadFlush : uint8_t { Pending, Ready };

  [[nodiscard]] Result FinishesPreload();
  [[nodiscard]] std::expected<PreloadFlush, std::string>
  FlushPreloadGround(std::chrono::steady_clock::time_point began, double bound);
  [[nodiscard]] Result PreloadTimeout(double bound);
  void AwaitPreloadProgress(double seconds);
  [[nodiscard]] bool UpdateActiveCamera();
  [[nodiscard]] bool UpdateRouteCamera(const Scenario::View &view);
  [[nodiscard]] Holds<RouteInfo> PublishedRouteInfo(std::string_view name) const;
  [[nodiscard]] Holds<RoutePose> SamplePublishedRoute(std::string_view name, double stationM) const;

  struct Classed {
    std::vector<float> Palette;
    std::shared_ptr<const ClassStructure> Structure;
  };

  static constexpr size_t kPaletteStride = 4u;

  [[nodiscard]] static std::vector<float> PaletteOver(const Ground::VegetationTemplates &wearing,
                                                      const Medium &fallback);

  [[nodiscard]] Classed Classify(std::span<const float> groundPositionsM,
                                 GroundWorldCandidate &candidate);

  struct Phasing {
    std::chrono::steady_clock::time_point PhaseAt;
    std::chrono::steady_clock::time_point CensusAt;
    std::chrono::steady_clock::time_point WiresAt;
  };

  [[nodiscard]] bool PrepareBuildingSurfaces(const TangentFrame &standing,
                                             GroundBuildProducts &build,
                                             Phasing &clocks);

  enum class Laid : uint8_t { Refused, Pending, Unchanged, Wanted };

  struct GroundRequest {
    Around Coverage;
    GroundRevision Revision;
  };

  enum class GroundBuildProgress : uint8_t { Failed, Pending, Ready };

  [[nodiscard]] GroundBuildProgress AdvanceGroundCandidatePreparation(const GroundRequest &request);
  [[nodiscard]] GroundBuildProgress AdvanceGroundPatchwork(const Around &coverage);
  [[nodiscard]] GroundBuildProgress
  AdvanceGroundSheets(const TangentFrame &standing, Patchwork &patchwork, const Around &coverage);
  [[nodiscard]] GroundBuildProgress BeginGroundSheetRefinement(const TangentFrame &standing,
                                                               Patchwork &patchwork);
  [[nodiscard]] GroundBuildProgress AdvanceGroundClasses();
  [[nodiscard]] GroundBuildProgress AdvanceGroundSurface();
  [[nodiscard]] GroundBuildProgress AdvanceGroundBuildingModels(const TangentFrame &standing);
  [[nodiscard]] GroundBuildProgress AdvanceGroundStreetGraph();
  [[nodiscard]] GroundBuildProgress AdvanceGroundRoadAlignments(const TangentFrame &standing);
  [[nodiscard]] GroundBuildProgress AdvanceGroundStructureBakes(const TangentFrame &standing) const;
  [[nodiscard]] std::string_view GroundBuildStatus() const noexcept;
  [[nodiscard]] std::string GroundBuildDiagnostic() const;
  [[nodiscard]] size_t StructureCandidatesMost() const noexcept;
  [[nodiscard]] Ground::BuildingField *CandidateFootprints() const noexcept;
  [[nodiscard]] bool StagesGroundBakes(size_t landsMost);

  [[nodiscard]] Laid Focuses(GroundRequest &request,
                             LongitudeLatitude at,
                             bool alsoWhenTilesLanded,
                             GroundQuality quality);

  [[nodiscard]] std::expected<GroundRequest, Laid> RingWanted(bool alsoWhenTilesLanded,
                                                              GroundQuality quality);

  [[nodiscard]] GroundBuildProgress
  BuildGroundBuildingStamps(const TangentFrame &standing,
                            GroundBuildState &state,
                            const Ground::OsmField &shapes,
                            std::vector<EarthworkStamp> &yielding);
  [[nodiscard]] bool PressGroundEarthworks(const TangentFrame &standing,
                                           Patchwork &patchwork,
                                           GroundBuildState &state);
  [[nodiscard]] bool BuildGroundCorridors(const TangentFrame &standing,
                                          const Around &coverage,
                                          GroundBuildState &state);
  [[nodiscard]] bool BuildGroundTerrainMesh(const TangentFrame &standing,
                                            Patchwork &patchwork,
                                            GroundBuildState &state);
  [[nodiscard]] bool PublishGroundGeometry(GroundBuildState &state);
  [[nodiscard]] GroundBuildProgress AdvanceGroundConstructionStages(const TangentFrame &standing,
                                                                    Patchwork &patchwork,
                                                                    GroundBuildState &state);
  [[nodiscard]] bool
  BuildWaterSurfaces(const TangentFrame &standing, Geometry &ground, MaterialInstance ringSurface);
  [[nodiscard]] bool Grounds(bool alsoWhenTilesLanded, GroundQuality quality);
  [[nodiscard]] bool AdvancesGroundWithinBudget(GroundQuality quality);
  [[nodiscard]] bool AdvancesGroundRetirement();
  [[nodiscard]] bool GroundInputsReady(GroundQuality quality) const;
  [[nodiscard]] bool RequestTerrainCoverage();
  [[nodiscard]] bool FollowCamera(const ViewBook &views);
  [[nodiscard]] bool
  UpdateSceneBodyTransform(size_t which, const Physics::Rigid &body, const Vec3 &shiftM);
  [[nodiscard]] bool PrepareRuntimeWorld();
  [[nodiscard]] bool ConfigureSourceProviders(std::vector<Data::SourceProvider> &tileProviders);
  void DeclareGroundFeatures();
  void PollOsmTransport();
  bool GenerateInitialInstances(double atLat, double atLon);
  [[nodiscard]] bool GenerateInstancesForRegion(const Generators::Tile &region,
                                                LevelOfDetail coarseness);
  [[nodiscard]] LongitudeLatitude CurrentGeographicFocus() const;
  [[nodiscard]] bool EnsureRuntimeScene();
  void HandsPiecesOver();
  [[nodiscard]] bool UpdateVegetation(bool prepare);
  [[nodiscard]] bool AdvanceStructureBuilds(size_t landsMost);
  [[nodiscard]] bool UpdateTriggers();
  [[nodiscard]] bool Updates();
  [[nodiscard]] bool Draws();
  void PublishFrameMeasurements();
  void PublishResourcePayloadMeasurements();
  void PublishAudioSnapshot();
  [[nodiscard]] bool IsAudioOccluded(const Vec3 &sourceM) const;
};

}
#endif
