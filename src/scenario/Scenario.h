#ifndef OUTSHINE_SCENARIO_H
#define OUTSHINE_SCENARIO_H

#include <cstdint>
#include <string>
#include <vector>

#include <SourceProvider.h>
#include <AudioScene.h>
#include <RenderConfiguration.h>
#include <PrismaticJoint.h>

#include "Earth.h"
#include "Mat4.h"
#include "Material.h"
#include "Quat.h"
#include "Vec3.h"

#include "Extent.h"
#include "Camera.h"

namespace outshine::Scenario {

constexpr double kPatienceUnsaidS = 30.0;

constexpr double kSightUnsaidM = 240000.0;

constexpr double kFovUnsaidDeg = 55.0;

constexpr double kRisesByUnsaid = 0.35;

constexpr double kPitchLimitUnsaidDeg = 89.0;

constexpr double kEyeHeightUnsaidM = 1.7;

constexpr double kWalkUnsaidMs = 1.4;
constexpr double kRunUnsaidMs = 4.5;

constexpr double kStepUnsaidS = 1.0 / 60.0;

constexpr double kWheelStepUnsaidPx = 48.0;

struct Light {

  double Lux = 0.0;

  double ElevationDeg = 0.0;

  double BearingDeg = 0.0;
};

struct Identity {
  std::string Name;
  std::string Version;
  double Epoch = 0.0;
  double Decay = 0.0;
  std::string Active;
};

struct Layer {

  std::string Id;

  std::string Path;

  std::string Set;
};

struct Georeference {

  double LatitudeDeg = 0.0;

  double LongitudeDeg = 0.0;

  double RadiusM = kEarthMeanRadiusM;
};

struct Weather {

  double CloudCover = 0.0;

  double CloudLow = 0.0;

  double CloudMid = 0.0;

  double CloudHigh = 0.0;

  double CloudBaseAglM = 0.0;

  double WindDeg = 0.0;

  double WindMs = 0.0;

  double Haze = 1.0;
};

struct Relief {

  std::string Kind;

  double AmplitudeM = 0.0;

  double WavelengthM = 0.0;

  double Gradient = 0.0;

  double BearingDeg = 0.0;

  uint64_t Seed = 0;
};

struct Structure {

  std::string Kind;

  double WidthM = 0.0;

  double HeightM = 0.0;

  bool Area = false;

  bool Bridge = false;

  bool Tunnel = false;

  int Level = 0;

  std::vector<double> LatLon;
};

struct WorldSettings {

  bool Declared = false;

  bool VegetationEnabled = true;

  Georeference Origin;

  Relief Shape;

  std::vector<Structure> Osm;

  double GravityMs2 = kStandardGravityMs2;

  double AirDensityKgM3 = kIsaSeaLevelDensityKgM3;

  Weather Sky;

  double PatienceS = kPatienceUnsaidS;

  double SightM = kSightUnsaidM;
};

struct Setting {
  std::string Name;
  std::string Value;
};

struct Generating {
  std::string Kind;

  std::vector<Setting> Parameters;
};

struct Patch {
  double LeftFrac = 0.0;
  double TopFrac = 0.0;
  double WidthFrac = 1.0;
  double HeightFrac = 1.0;

  [[nodiscard]] constexpr bool whole() const noexcept {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

struct Lighting {
  bool Declared = false;
  Light Key;
  Vec3 IndirectLight;

  double ShadowRadiusM = 0.0;
};

enum class AssetAnimation { Play, Loop, Ignore, Driven };

struct SurfaceOverride {
  std::string Named;
  std::string Node;

  int Part = -1;

  bool KeepsMaps = false;
  Material Row;
};

struct Asset {
  std::string Uri;
  std::string Digest;
  std::string Kind;
  std::string Variant;

  AssetAnimation Animation = AssetAnimation::Play;

  int Clip = 0;
  std::vector<SurfaceOverride> Surfaces;
};

struct Standing {
  Vec3 AtM;

  Quat Facing;

  Vec3 ScaleXyz = {{1.0, 1.0, 1.0}};

  bool GlobeAnchor = false;

  outshine::LongitudeLatitudeHeight Geodetic;

  bool SamplesHeight = false;
  double BearingDeg = 0.0;
  double PitchDeg = 0.0;
};

struct Placement {
  std::string Asset;
  Standing Stands;
};

struct Surface {
  std::string Document;

  std::string Style;

  std::string Programme;

  Patch Where;

  int Z = 0;
};

struct Mind {
  std::string Tier;
  std::string Uses;
  std::string Programme;
  std::string Prompt;
  std::string Model;
  std::string Meanwhile;
  double Hz = 0.0;
  double EverySeconds = 0.0;
  long long StepBudget = 0;
  int TokenBudget = 0;
  double LatencyBudgetMs = 0.0;
  double Temperature = 0.0;
  long long Seed = 0;
};

struct Kind {
  std::string Name;

  std::string Inherits;
  std::string Asset;
  std::vector<Mind> Minds;

  std::vector<std::string> Capabilities;

  std::vector<Setting> Attributes;
};

struct Instance {
  std::string Of;
  std::string Id;

  std::string In;
  Standing Stands;

  std::vector<Setting> Attributes;

  std::vector<std::string> Holds;
};

struct Region {

  std::string Id;
  std::string Kind;
  Vec3 OriginM;
  double RadiusM = 0.0;
  bool Streams = true;
  std::vector<std::string> Uses;
};

struct Door {
  std::string Id;
  std::string From;
  std::string To;
  Vec3 AtM;
};

struct Volume {

  std::string Id;

  std::string In;

  std::string Shape;

  Vec3 AtM;

  Vec3 ExtentM;

  std::string Fires;

  std::string When;

  double DwellS = 0.0;
};

struct Table {
  std::string Id;

  std::vector<std::string> Columns;

  std::vector<bool> Types;

  std::vector<std::vector<std::string>> Rows;
};

struct Event {
  std::string Name;

  std::vector<std::string> Carries;
};

enum class CameraPlacement { FollowEntity, Local, Geodetic, Route };

struct GeographicCameraPlacement {

  LongitudeLatitudeHeight Geodetic;

  bool SamplesHeight = false;

  double BearingDeg = 0.0;

  double PitchDeg = 0.0;
};

struct RouteCameraPlacement {
  static constexpr double kDefaultEyeHeightM = 1.6;
  static constexpr double kDefaultLookAheadM = 12.0;
  static constexpr double kDefaultMaximumSpeedMps = 30.0;
  static constexpr double kDefaultAccelerationMs2 = 3.0;
  static constexpr double kDefaultBrakingMs2 = 6.0;
  static constexpr double kDefaultLateralAccelerationMs2 = 5.0;

  std::string RouteId;
  double EyeHeightM = kDefaultEyeHeightM;
  double LateralOffsetM = 0.0;
  double LookAheadM = kDefaultLookAheadM;
  double MaximumSpeedMps = kDefaultMaximumSpeedMps;
  double AccelerationMs2 = kDefaultAccelerationMs2;
  double BrakingMs2 = kDefaultBrakingMs2;
  double LateralAccelerationMs2 = kDefaultLateralAccelerationMs2;
};

struct View {
  std::string Id;

  Camera Sees;

  CameraPlacement Placement = CameraPlacement::FollowEntity;

  GeographicCameraPlacement Geographic;
  RouteCameraPlacement Route;
  Patch Viewport;

  void setCamera(const Camera &sees) { Sees = sees; }

  void setViewport(const Patch &over) { Viewport = over; }

  void setScene(std::string named) { In = std::move(named); }

  [[nodiscard]] std::string_view scene() const noexcept { return In; }

  std::string In;

  std::string Follows;

  std::string Person;

  Vec3 OffsetM;

  double DistanceM = 0.0;

  double RisesBy = kRisesByUnsaid;

  double PitchLimitDeg = kPitchLimitUnsaidDeg;

  double TimeScale = 1.0;
};

struct Slip {
  double Grip = 0.0;
  double RadiusM = 0.0;
  double CorneringNPerRad = 0.0;
  double RelaxationM = 0.0;

  double LoadFalloff = 0.0;
};

struct Contact {
  std::string At;
  Vec3 AtM;
  Physics::PrismaticJoint Strut;
  Slip Touches;
};

enum class Drives : uint8_t { Effort, Motion };

struct Drive {

  Drives Does = Drives::Effort;
  bool Opposes = false;

  bool Turns = true;

  Vec3 AxisXyz = {{0.0, 1.0, 0.0}};
  double PeakNm = 0.0;
  double PeakN = 0.0;
  double Ratio = 1.0;
  double CircleM = 0.0;
};

struct Slot {
  std::string At;
  Vec3 AtM;
};

struct Body {

  std::string Name;

  std::string Asset;

  bool Placed = false;

  Standing Stands;
  double MassKg = 0.0;
  double WidthM = 0.0;

  Vec3 CentreOfMassM;

  Vec3 InertiaKgM2;

  std::vector<Contact> Contacts;

  std::vector<Drive> Driven;
  double DragCoefficient = 0.0;
  double FrontalM2 = 0.0;
  std::vector<Slot> Slots;
};

struct Player {
  bool Declared = false;
  std::string Is;
  std::string Starts;

  std::string View;
  double EyeHeightM = kEyeHeightUnsaidM;
  double WalkMs = kWalkUnsaidMs;
  double RunMs = kRunUnsaidMs;
};

struct PhysicsSettings {
  bool Declared = false;
  std::string Dial;
  double StepS = kStepUnsaidS;

  int MostStepsInArrears = 8;
};

struct Clock {
  bool Declared = false;

  bool Live = false;

  std::string Start;
  double Rate = 1.0;
};

struct Binding {

  std::string Event;

  std::string Action;
};

struct Persisted {

  std::string What;
};

struct RouteDeclaration {

  std::string Id;

  uint64_t OsmRelationId = 0;

  [[nodiscard]] bool operator==(const RouteDeclaration &) const = default;
};

struct Document {

  Identity Named;

  std::vector<Layer> Layers;

  WorldSettings Ground;

  std::vector<Data::SourceProvider> Providers;

  std::vector<RouteDeclaration> Routes;

  std::vector<Generating> Generators;

  Render::Configuration Render;

  Lighting Lit;

  std::vector<Asset> Assets;

  std::vector<Placement> Placements;

  std::vector<Surface> Surfaces;

  std::vector<Kind> Kinds;

  size_t Room = 0;

  std::vector<Instance> Instances;

  std::vector<Region> Regions;

  std::vector<Door> Doors;

  std::vector<Volume> Volumes;

  std::vector<Audio::SoundSource> Sounds;

  std::vector<Audio::MixBus> Buses;

  std::vector<Table> Tables;

  std::vector<Event> Events;

  std::vector<View> Views;

  std::vector<Body> Bodies;

  Player Played;

  PhysicsSettings Motion;

  Clock Time;

  std::vector<Binding> Input;

  double WheelStepPx = kWheelStepUnsaidPx;

  std::vector<Persisted> State;
};

}

#endif
