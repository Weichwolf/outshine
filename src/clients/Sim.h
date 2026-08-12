/* THE SIMULATION HALF. One declared scene in, a world that answers questions out — and it never
 * draws: no device, no renderer, no camera basis leaves this object as anything but numbers. Both
 * targets link it; a renderer over it is the picture half's and is not named here. */
#ifndef SIM_H
#define SIM_H

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Buildings.h"
#include "ContentStore.h"
#include "EyeTelemetry.h"
#include "Forest.h"
#include "GeneratorSet.h"
#include "Ground.h"
#include "GroundMaterials.h"
#include "GroundTable.h"
#include "Infrastructure.h"
#include "RegionForge.h"
#include "RegionPool.h"
#include "Scene.h"
#include "Stage.h"
#include "SceneWeather.h"
#include "Schedule.h"
#include "EyeColumn.h"
#include "Water.h"
#include "WaterDepth.h"
#include "State.h"
#include "StreamTelemetry.h"
#include "Telemetry.h"
#include "SourceSet.h"
#include "TreeSpecies.h"
#include "VegetationTemplates.h"
#include "World.h"

namespace outshine::Clients {

class Sim {
public:
  /* The engine's own declarations, by path, because the two toolchains mount them differently — a
   * preloaded virtual FS in the browser, the working directory natively. Nothing here is content. */
  struct Assets {
    std::string Vegetation, GroundMaterials, Species, Moon, Stars;
  };
  struct Stance {
    double Lat = 0.0, Lon = 0.0, YawDeg = 0.0, PitchDeg = 0.0;
  };
  /* WHAT STANDS AT A PLACE, with no buffer existing and no device anywhere — the point query of
   * doc/architecture.md's product table. Metres are above the DEM's own datum. */
  struct Place {
    bool GroundResolved = false;
    double GroundAslM = 0.0;
    int Class = -1;                            /* vegetation table row; -1 = OSM has no datum here */
    double ClassEdgeM = 0.0;                   /* distance to that class's nearest edge */
    /* False while this place's outlines are still streaming. Without it "no house here" and "the
     * vector tile has not landed" are the same empty answer, which is the defect the ground's own
     * states exist to prevent. */
    bool OutlinesResolved = false;
    std::optional<double> StructureHeightM;    /* what stands here, from the ground up */
    std::optional<Generators::Infrastructure::Made> Made;   /* the made surface under the foot */
    WaterDepth Water = WaterDepth::Dry();
  };

  /* THE ORDER CONTENT CLAIMS SPACE IN, declared once and in one place: the picture's own draw
   * sources are matched to the generators by it, so a second literal somewhere else is a silent
   * mismatch between what stands and what is drawn. */
  static constexpr Generators::Rank kBuiltRank{0};
  static constexpr Generators::Rank kWaterRank{1};
  static constexpr Generators::Rank kWayRank{2};
  static constexpr Generators::Rank kTreeRank{3};

  Sim(const Scenario::Scene &scene, const Assets &assets);

  /* THE ONLY THREE THINGS A CLIENT STILL SAYS. The transport is how this machine reaches an
   * upstream and is no property of the world; whether the content store is used is the run's
   * decision and changes timing and nothing else; the stance is overridden only by a snapshot
   * another run wrote. All three are read by LoadTables/Open, so they are refused
   * afterwards rather than silently ignored. */
  void SetTransport(Data::Transport &transport) { if (!Opened_) Wire_ = &transport; }
  void SetContentStore(const Data::ContentStore::Config &config) { if (!Opened_) Store_ = config; }
  void SetStance(const Stance &s) { if (!Opened_) Stance_ = s; }

  /* The declared tables, before anything asks the world a question. */
  [[nodiscard]] bool LoadTables();

  /* Opens the tile stream, resolves the ground under the standpoint and places sun and moon. Ends
   * with the eye standing where the scene declared it.
   *
   * A DEM ANSWER IS A NETWORK ANSWER, so this is a poll and not a call: the first turn opens the
   * stream, every turn after asks the ground again, and the caller keeps its own turn until the
   * answer stops being Waiting. Inventing a plateau instead would move the whole picture. */
  enum class Bring { Waiting, Open, Failed };
  [[nodiscard]] Bring Open();
  [[nodiscard]] bool Opened() const { return Opened_; }

  /* What one region became. It is the forge's product under this object's name because a region
   * that stands and a region that has just been generated are the same thing. */
  using Populated = RegionForge::Grown;

  /* Move the eye. The ground rides the DEM, so the height above sea level follows from wherever the
   * stance now is and a cold tile leaves the last resolved answer standing. */
  void Look(const Stance &s);
  /* One simulation pass over the place the eye stands at. */
  void Advance();
  /* Once the vectors are in: the roof the eye may be inside, and the stand it may be inside. */
  void Settle();

  Place At(double lat, double lon) const;

  /* THE SKY CLOCK MOVES, the wind clock is a different one. `s` is an offset on the scene's own
   * instant, in seconds. */
  void SetSkyOffsetS(double s);

  /* THE IDENTITY SOURCE IS BORROWED and registered ahead of every other, so every row names its
   * run. Whoever owns a further source registers it on the same bus. */
  void SetTelemetrySink(TelemetrySink *sink) { Bus_.SetSink(sink); }
  void SetTelemetryIdentity(TelemetrySource *id) { Identity_ = id; }
  TelemetryBus &Bus() { return Bus_; }
  void StartTelemetry();
  StreamTelemetry &Streaming() { return Stream_; }
  const StreamTelemetry &Streaming() const { return Stream_; }
  /* WHERE THE EYE HAS BEEN, fed by Look and by nothing else. Travel is the path the eye walked, so
   * a run's own record answers "did the camera move" without a second instrument. */
  const EyeTelemetry &Where() const { return Where_; }

  World::World &Scenery() { return W_; }
  const World::World &Scenery() const { return W_; }
  /* The regions that carry occupancy right now, in the order they were generated. */
  Span<const Populated> Regions() const { return Span<const Populated>(Grown_.data(), Grown_.size()); }
  const Generators::GeneratorSet &Content() const { return Gens_; }
  /* Moves whenever a region is generated or released, which is what a collector re-reads on. */
  uint64_t RegionVersion() const { return Version_; }
  /* THE REGION THE EYE STANDS IN. The ring is anchored on it, so a change of this key IS a region
   * crossing — which is what makes the crossing frames of a run identifiable in its own profile
   * rather than reconstructed from a distance afterwards. */
  Generators::Region Here() const { return Generators::Region::Of(Ring_.Zoom(), Stance_.Lat, Stance_.Lon); }
  size_t RegionsStanding() const { return Grown_.size(); }
  /* A region is being grown right now, off this thread. */
  [[nodiscard]] bool RegionBusy() const { return Forge_ && !Forge_->Idle(); }
  /* Every region the ring reaches stands. The loading phase holds the world back until it does:
   * one region is generated per pass, so a run that opened its shutter on residency alone would
   * photograph a forest with three quarters of itself missing. */
  [[nodiscard]] bool RingStands() const;
  const Generators::TreeSpecies &Species() const { return Species_; }
  size_t GeneratorHeapBytes() const { return Pool_ ? Pool_->HeapBytes() : 0; }
  /* What one region costs while it stands: its slot in the pool plus its own ground patch. */
  size_t RegionSlotBytes() const { return Pool_ ? Pool_->SlotBytes() : 0; }
  long StandCount() const;
  /* WHAT THE RING COST THE CALLING THREAD in the pass just run. Separate from the world's own pass
   * because it runs beside it and not inside it, and a crossing is where the two are told apart. */
  double PopulateMs() const { return PopulateMs_; }
  /* [SET] metres. How far from the eye the world is populated, and the same number the picture cuts
   * its instances at. It is a COLLECTION BUDGET and not a pixel threshold: at the declared 720p and
   * 60 deg the pixel focal length is 623.5, so a 30 m stand at this range is still 20.8 px tall and
   * nothing about it has stopped being legible. What ends here is what the client is willing to
   * carry — pi * 900^2 * 0.033/m2 is 84 000 instances at the densest declared class, which is what
   * `Outshine`'s collection reserves. */
  static constexpr double kReachM = 900.0;

  const World::EyeColumn &EyeColumn() const { return Stand_; }
  const World::VegetationTemplates &Vegetation() const { return Veg_; }
  const World::GroundMaterials &Materials() const { return Mats_; }
  const Scenario::Scene &Declared() const { return Scene_; }
  /* WHICH STAGE THIS SIMULATION IS ON, and null is the other arm (`F.60`). Everything that only
   * exists because there is an Earth under the scene is reached through the first; a studio has no
   * place, no civil time and no met wind, and therefore no member here to hold them. */
  const Scenario::WorldStage *WorldStage() const { return Scene_.Staged().AsWorld(); }
  const Scenario::StudioStage *StudioStage() const { return Scene_.Staged().AsStudio(); }
  const Assets &Files() const { return Assets_; }
  const SceneWeather &Weather() const { return Wind_; }
  const State &SceneState() const { return State_; }

  double Lat() const { return Stance_.Lat; }
  double Lon() const { return Stance_.Lon; }
  double YawDeg() const { return Stance_.YawDeg; }
  double PitchDeg() const { return Stance_.PitchDeg; }
  const Stance &Standing() const { return Stance_; }
  const double *Eye() const { return Eye_; }
  const double *Fwd() const { return Fwd_; }
  const double *Right() const { return Right_; }
  const double *Up() const { return Up_; }
  World::World::Eye Sight() const { return {Stance_.Lat, Stance_.Lon, Eye_, Fwd_}; }

  float SunElDeg() const { return SunEl_; }
  float SunAzDeg() const { return SunAz_; }
  double SkyClockS() const { return Clk_; }
  double WindDeg() const { return WindDeg_; }
  /* Where the flow clock starts, seconds. Not the sky clock: the sun stands still while wind runs. */
  double WindClockS() const { return WindClockS_; }
  double WindMs() const { return WindMs_; }
  double ViewM() const { return ViewM_; }
  double OrthoM() const { return OrthoM_; }

  /* The ladder measures its screen-space error in pixels of the declared frame, so a field of view
   * that moves mid-run moves it too. */
  void SetPixelFocalLength(double px) { W_.SetPixelFocalLength(px); }

private:
  /* WHAT ONE SNAPSHOT COST THE CALLING THREAD, and the outline cut inside it: the two halves have
   * different shapes — the ground block is a fixed lattice, the outlines are what the place holds —
   * and one number for both cannot say which of them moved. */
  struct SnapshotCost {
    double TotalMs = 0.0, FeatureMs = 0.0;
  };
  [[nodiscard]] Bring ResolveGround(double lat, double lon, double *out) const;
  [[nodiscard]] bool OpenPool();
  /* ONE PASS OF THE RING: release what it no longer names, cancel what is under way and unnamed,
   * collect what the forge finished, and request the next region that is missing. */
  void Populate();
  void Release();
  void Gather();
  void Ask();
  void Say(const Populated &grown, const SnapshotCost &cost) const;

  [[nodiscard]] bool Names(const Generators::Region &region) const;
  [[nodiscard]] bool Standing(const Generators::Region &region) const;
  [[nodiscard]] bool Reached(const Generators::Region &region) const;
  /* WHAT A REGION'S GROUND CAME TO. Three answers because the DEM has three (world/TerrainLoader.h
   * FbGroundBlock::State), and a caller that cannot tell "not yet" from "never" waits for one of
   * them for ever. */
  enum class Snapped { Taken, Waiting, NoGround };
  [[nodiscard]] Snapped Snapshot(const Generators::Region &region,
                                 Generators::Ground::Snapshot *out, SnapshotCost *cost) const;
  /* Null while this region's vector tile is still out. */
  std::shared_ptr<const Generators::FeatureField> Features(const Generators::Region &region) const;
  /* The region containing a place, snapshotted for one question and dropped again. */
  std::optional<Generators::Ground> GroundAt(double lat, double lon) const;

  Scenario::Scene Scene_;
  Assets Assets_;
  SceneWeather Wind_;
  Stance Stance_;
  double WindDeg_ = 0.0, WindMs_ = 0.0, WindClockS_ = 0.0, Clk_ = 0.0;

  /* Borrowed from the client, which owns the host's wire; the registry and the store beneath it are
   * this object's, because a world that streams is what needs them. DECLARED BEFORE THE WORLD
   * (`C.13`): the world's tile pool runs threads that read all four, and reverse-order destruction
   * is what joins those threads after the last read, not before it. */
  Data::Transport *Wire_ = nullptr;
  Data::ContentStore::Config Store_;
  std::unique_ptr<Data::ContentStore> Content_;
  std::unique_ptr<Data::SourceSet> Sources_;

  World::GroundMaterials Mats_;
  World::VegetationTemplates Veg_;
  World::World W_;
  World::EyeColumn Stand_;

  Generators::TreeSpecies Species_;
  std::vector<float> StandsPerM2_;                     /* by ground class row */
  std::shared_ptr<const Generators::GroundTable> Table_;
  Generators::Schedule Ring_;
  Generators::GeneratorSet Gens_;
  std::optional<Generators::Forest> Trees_;
  std::optional<Generators::Buildings> Structures_;
  std::optional<Generators::Water> Lakes_;
  std::optional<Generators::Infrastructure> Ways_;
  /* Which row of the declared table a building and a water outline classify as, resolved once: the
   * class model is the only place that says which OSM layer a generator's features come from. A
   * way's row is per feature and travels with it (world/StreetField.h), because one layer maps to
   * three templates — asphalt, track and trodden path are not one surface. */
  int BuiltRow_ = -1, WetRow_ = -1;
  /* Both are finished only once the declared tables are read: the pool is sized on what the
   * generators say they can claim, and the forge runs them (`C.41` — the constructor cannot). */
  std::optional<Generators::RegionPool> Pool_;
  std::optional<RegionForge> Forge_;
  std::vector<Populated> Grown_;
  /* Regions whose content does not fit the budget. Kept by key so the ring does not ask again every
   * turn; dropped when the ring stops naming them, so walking back is a fresh attempt. */
  std::vector<Generators::Region> Refused_;
  SnapshotCost SnapshotCost_;
  double PopulateMs_ = 0.0;
  /* Where the next turn's one snapshot attempt starts. Round robin, so a region whose DEM never
   * lands cannot stand in front of the rest of the ring. */
  size_t Asked_ = 0;
  uint64_t Version_ = 0;

  State State_;
  EyeTelemetry Where_;
  StreamTelemetry Stream_;
  TelemetrySource *Identity_ = nullptr;
  TelemetryBus Bus_;

  double ViewM_ = 60000.0, OrthoM_ = 0.0;
  float SunEl_ = 0.0f, SunAz_ = 0.0f;
  double Eye_[3] = {}, Fwd_[3] = {}, Right_[3] = {}, Up_[3] = {};
  bool Opened_ = false;
  bool Streaming_ = false;   /* the tile stream is up; the ground under the standpoint is not in yet */
  bool RoofChecked_ = false;
};

} // namespace outshine::Clients
#endif
