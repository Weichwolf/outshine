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

  struct Assets {
    std::string Vegetation, GroundMaterials, Species, Moon, Stars;
  };
  struct Stance {
    double Lat = 0.0, Lon = 0.0, YawDeg = 0.0, PitchDeg = 0.0;
  };

  struct Place {
    bool GroundResolved = false;
    double GroundAslM = 0.0;
    int Class = -1;
    double ClassEdgeM = 0.0;

    bool OutlinesResolved = false;
    std::optional<double> StructureHeightM;
    std::optional<Generators::Infrastructure::Made> Made;
    WaterDepth Water = WaterDepth::Dry();
  };

  static constexpr Generators::Rank kBuiltRank{0};
  static constexpr Generators::Rank kWaterRank{1};
  static constexpr Generators::Rank kWayRank{2};
  static constexpr Generators::Rank kTreeRank{3};

  Sim(const Scenario::Scene &scene, const Assets &assets);

  void SetTransport(Data::Transport &transport) { if (!Opened_) Wire_ = &transport; }
  void SetContentStore(const Data::ContentStore::Config &config) { if (!Opened_) Store_ = config; }
  void SetStance(const Stance &s) { if (!Opened_) Stance_ = s; }

  [[nodiscard]] bool LoadTables();

  enum class Bring { Waiting, Open, Failed };
  [[nodiscard]] Bring Open();
  [[nodiscard]] bool Opened() const { return Opened_; }

  using Populated = RegionForge::Grown;

  void Look(const Stance &s);

  void Advance();

  void Settle();

  Place At(double lat, double lon) const;

  void SetSkyOffsetS(double s);

  void SetTelemetrySink(TelemetrySink *sink) { Bus_.SetSink(sink); }
  void SetTelemetryIdentity(TelemetrySource *id) { Identity_ = id; }
  TelemetryBus &Bus() { return Bus_; }
  void StartTelemetry();
  StreamTelemetry &Streaming() { return Stream_; }
  const StreamTelemetry &Streaming() const { return Stream_; }

  const EyeTelemetry &Where() const { return Where_; }

  World::World &Scenery() { return W_; }
  const World::World &Scenery() const { return W_; }

  Span<const Populated> Regions() const { return Span<const Populated>(Grown_.data(), Grown_.size()); }
  const Generators::GeneratorSet &Content() const { return Gens_; }

  uint64_t RegionVersion() const { return Version_; }

  Generators::Region Here() const { return Generators::Region::Of(Ring_.Zoom(), Stance_.Lat, Stance_.Lon); }
  size_t RegionsStanding() const { return Grown_.size(); }

  [[nodiscard]] bool RegionBusy() const { return Forge_ && !Forge_->Idle(); }

  [[nodiscard]] bool RingStands() const;
  const Generators::TreeSpecies &Species() const { return Species_; }
  size_t GeneratorHeapBytes() const { return Pool_ ? Pool_->HeapBytes() : 0; }

  size_t RegionSlotBytes() const { return Pool_ ? Pool_->SlotBytes() : 0; }
  long StandCount() const;

  double PopulateMs() const { return PopulateMs_; }

  static constexpr double kReachM = 900.0;

  const World::EyeColumn &EyeColumn() const { return Stand_; }
  const World::VegetationTemplates &Vegetation() const { return Veg_; }
  const World::GroundMaterials &Materials() const { return Mats_; }
  const Scenario::Scene &Declared() const { return Scene_; }

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

  double WindClockS() const { return WindClockS_; }
  double WindMs() const { return WindMs_; }
  double ViewM() const { return ViewM_; }
  double OrthoM() const { return OrthoM_; }

  void SetPixelFocalLength(double px) { W_.SetPixelFocalLength(px); }

private:

  struct SnapshotCost {
    double TotalMs = 0.0, FeatureMs = 0.0;
  };
  [[nodiscard]] Bring ResolveGround(double lat, double lon, double *out) const;
  [[nodiscard]] bool OpenPool();

  void Populate();
  void Release();
  void Gather();
  void Ask();
  void Say(const Populated &grown, const SnapshotCost &cost) const;

  [[nodiscard]] bool Names(const Generators::Region &region) const;
  [[nodiscard]] bool Standing(const Generators::Region &region) const;
  [[nodiscard]] bool Reached(const Generators::Region &region) const;

  enum class Snapped { Taken, Waiting, NoGround };
  [[nodiscard]] Snapped Snapshot(const Generators::Region &region,
                                 Generators::Ground::Snapshot *out, SnapshotCost *cost) const;

  std::shared_ptr<const Generators::FeatureField> Features(const Generators::Region &region) const;

  std::optional<Generators::Ground> GroundAt(double lat, double lon) const;

  Scenario::Scene Scene_;
  Assets Assets_;
  SceneWeather Wind_;
  Stance Stance_;
  double WindDeg_ = 0.0, WindMs_ = 0.0, WindClockS_ = 0.0, Clk_ = 0.0;

  Data::Transport *Wire_ = nullptr;
  Data::ContentStore::Config Store_;
  std::unique_ptr<Data::ContentStore> Content_;
  std::unique_ptr<Data::SourceSet> Sources_;

  World::GroundMaterials Mats_;
  World::VegetationTemplates Veg_;
  World::World W_;
  World::EyeColumn Stand_;

  Generators::TreeSpecies Species_;
  std::vector<float> StandsPerM2_;
  std::shared_ptr<const Generators::GroundTable> Table_;
  Generators::Schedule Ring_;
  Generators::GeneratorSet Gens_;
  std::optional<Generators::Forest> Trees_;
  std::optional<Generators::Buildings> Structures_;
  std::optional<Generators::Water> Lakes_;
  std::optional<Generators::Infrastructure> Ways_;

  int BuiltRow_ = -1, WetRow_ = -1;

  std::optional<Generators::RegionPool> Pool_;
  std::optional<RegionForge> Forge_;
  std::vector<Populated> Grown_;

  std::vector<Generators::Region> Refused_;
  SnapshotCost SnapshotCost_;
  double PopulateMs_ = 0.0;

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
  bool Streaming_ = false;
  bool RoofChecked_ = false;
};

}
#endif
