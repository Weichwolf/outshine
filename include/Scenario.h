#ifndef OUTSHINE_SCENARIO_H
#define OUTSHINE_SCENARIO_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace outshine {

struct Extent {
  int WidthPx = 0;
  int HeightPx = 0;
};

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

// WHERE ON THE EARTH THE SCENE STANDS, and Cesium's word for it. A `WorldSettings` that also carries
// gravity, air density, wind and a streaming patience is not a georeference; this is the part that
// is, and separating it is why the name can be used honestly at all.
struct Georeference {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  double RadiusM = 6371008.8;
};

struct WorldSettings {
  bool Declared = false;
  Georeference Origin;
  double GravityMs2 = 9.80665;
  double AirDensityKgM3 = 1.2250;
  double WindDeg = 0.0;
  double WindMs = 0.0;
  double CloudCover = 0.0;
  double PatienceS = 30.0;
  double SightM = 240000.0;
};

struct Provider {
  std::string Kind;
  std::string Pin;
  int Rank = 0;
  std::string WhenAbsent;
};

struct Setting {
  std::string Name;
  std::string Value;
};

struct Generator {
  std::string Kind;
  std::vector<Setting> Parameters;
};

struct Compositor {
  std::string Kind;
  double BudgetPx = 0.0;
  bool On = true;
};

struct Patch {
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;

  [[nodiscard]] bool whole(void) const {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

struct RenderPlan {
  bool Declared = false;
  Extent Frame;
  Patch Picture;
  double Fps = 60.0;
  double Fill = 0.9;
  double OrbitDegPerFrame = 0.0;
  std::vector<std::string> Outputs;
  std::vector<std::string> Stages;
  std::string Transfer;
  double Exposure = 0.0;
  std::string Precision;
};

struct Lighting {
  bool Declared = false;
  Light Key;
  double IndirectLight[3] = {0.0, 0.0, 0.0};
  double ShadowRadiusM = 0.0;
};

enum class AssetAnimation { Play, Ignore, Driven };

struct Asset {
  std::string Uri;
  std::string Digest;
  std::string Kind;
  std::string Variant;
  AssetAnimation Animation = AssetAnimation::Play;
  int Clip = 0;
};

// A PLACE ON THE EARTH, spelled the way Cesium spells it. Three loose doubles said the same thing
// and named nothing: a reader who has used Cesium for Unreal already owns this word, and a reader
// who has not can see from it that the numbers belong together.
struct LongitudeLatitudeHeight {
  double LongitudeDeg = 0.0;
  double LatitudeDeg = 0.0;
  double HeightM = 0.0;
};

struct Standing {
  double AtM[3] = {0.0, 0.0, 0.0};
  double FacingXyzw[4] = {0.0, 0.0, 0.0, 1.0};
  double ScaleXyz[3] = {1.0, 1.0, 1.0};

  bool GlobeAnchor = false;
  LongitudeLatitudeHeight Geodetic;
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
  double OriginM[3] = {0.0, 0.0, 0.0};
  double RadiusM = 0.0;
  bool Streams = true;
  std::vector<std::string> Uses;
};

struct Door {
  std::string Id;
  std::string From;
  std::string To;
  double AtM[3] = {0.0, 0.0, 0.0};
};

struct Volume {
  std::string Id;
  std::string In;
  std::string Shape;
  double AtM[3] = {0.0, 0.0, 0.0};
  double ExtentM[3] = {0.0, 0.0, 0.0};
  std::string Fires;
  std::string When;
  double DwellS = 0.0;
};

enum class Falls : uint8_t { Linear, Inverse, Exponential };

struct Emitter {
  bool Positional = false;
  Falls By = Falls::Inverse;
  double RefM = 1.0;
  double MostM = 0.0;
  double Rolloff = 1.0;
  double InnerRad = 0.0;
  double OuterRad = 0.0;
  double OuterGain = 0.0;
  double BlockedGain = 1.0;
  double BlockedHz = 0.0;
};

enum class Makes : uint8_t { Oscillator, Noise, Biquad, Delay, Gain, Shaper, Convolver, Mix };

struct Voice {
  std::string Id;
  Makes Does = Makes::Oscillator;
  std::vector<std::string> From;
  std::vector<Setting> Parameters;
};

struct Sound {
  std::string Id;
  std::string Uri;
  std::vector<Voice> Graph;
  bool Streamed = false;
  std::string On;
  std::string Bus;
  Emitter Heard;
  bool Loops = false;
  double GainDb = 0.0;
  double SendShare = 0.0;
};

struct Room {
  bool Declared = false;
  double SecondsRt60 = 0.0;
  double Damping = 0.5;
  double WetShare = 0.0;
};

struct Bus {
  std::string Id;
  std::string Into;
  double GainDb = 0.0;
  Room Reverberates;
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

// FILAMENT'S CAMERA CARRIES ITS OWN PROJECTION -- `setProjection(fov, aspect, near, far)` and
// `lookAt(eye, centre, up)` -- and a declaration that states a clip range or an aim point means it.
// Unreal's `FMinimalViewInfo` is the same three: a location, a rotation and a projection. The
// engine's own defaults stand where a field is left at zero, which is what an UNDECLARED section
// means everywhere else on this page.
struct Camera {
  bool Placed = false;
  Standing Stands;
  double FovDeg = 0.0;

  double NearM = 0.0;
  double FarM = 0.0;

  bool LooksAt = false;
  double LookAtM[3] = {0.0, 0.0, 0.0};
  double RollRad = 0.0;
};

struct View {
  std::string Id;
  Camera Sees;
  Patch Viewport;
  std::string Follows;
  std::string Person;
  double OffsetM[3] = {0.0, 0.0, 0.0};
  double DistanceM = 0.0;
  double RisesBy = 0.35;
  double PitchLimitDeg = 89.0;
  double TimeScale = 1.0;
};

struct Prismatic {
  double ReachM = 0.0;
  double StiffnessNPerM = 0.0;
  double DampingNsPerM = 0.0;
  double TravelM = 0.0;
  double StopNPerM = 0.0;
  double LimitN = 0.0;
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
  double AtM[3] = {0.0, 0.0, 0.0};
  Prismatic Strut;
  Slip Touches;
};

enum class Drives : uint8_t { Effort, Motion };

struct Drive {
  Drives Does = Drives::Effort;
  bool Opposes = false;
  bool Turns = true;
  double AxisXyz[3] = {0.0, 1.0, 0.0};
  double PeakNm = 0.0;
  double PeakN = 0.0;
  double Ratio = 1.0;
  double CircleM = 0.0;
};

struct Slot {
  std::string At;
  double AtM[3] = {0.0, 0.0, 0.0};
};

struct Body {
  std::string Name;
  std::string Asset;
  bool Placed = false;
  Standing Stands;
  double MassKg = 0.0;
  double WidthM = 0.0;

  double AssetSpanM = 0.0;

  double AssetGround = 0.0;
  double AssetCentreX = 0.0, AssetCentreZ = 0.0;
  double CentreOfMassM[3] = {0.0, 0.0, 0.0};
  double InertiaKgM2[3] = {0.0, 0.0, 0.0};
  std::vector<Contact> Contacts;
  std::vector<Drive> Driven;
  double DragCoefficient = 0.0;
  double FrontalM2 = 0.0;
  std::vector<Slot> Slots;

  [[nodiscard]] const Drive *can(Drives does) const {
    for (const Drive &one : Driven) {
      if (one.Does == does) { return &one; }
    }
    return nullptr;
  }

  [[nodiscard]] const Drive *efforts(bool opposing) const {
    for (const Drive &one : Driven) {
      if (one.Does == Drives::Effort && one.Opposes == opposing) { return &one; }
    }
    return nullptr;
  }

  [[nodiscard]] double spanM() const {
    double aheadM = 0.0, behindM = 0.0;
    int ahead = 0, behind = 0;
    for (const Contact &one : Contacts) {
      if (one.AtM[2] < CentreOfMassM[2]) {
        aheadM += one.AtM[2];
        ++ahead;
      } else if (one.AtM[2] > CentreOfMassM[2]) {
        behindM += one.AtM[2];
        ++behind;
      }
    }
    return ahead > 0 && behind > 0
               ? std::fabs(behindM / (double)behind - aheadM / (double)ahead)
               : 0.0;
  }

  [[nodiscard]] double acrossM() const {
    double leastM = 0.0, mostM = 0.0;
    for (const Contact &one : Contacts) {
      leastM = one.AtM[0] < leastM ? one.AtM[0] : leastM;
      mostM = one.AtM[0] > mostM ? one.AtM[0] : mostM;
    }
    return mostM - leastM;
  }
};

enum class Travels : uint8_t { Walk, Drive, Fly, Rail };

struct Journey {
  bool Declared = false;
  Travels By = Travels::Drive;
  double FromLatDeg = 0.0;
  double FromLonDeg = 0.0;
  double ToLatDeg = 0.0;
  double ToLonDeg = 0.0;
};

struct Player {
  bool Declared = false;
  std::string Is;
  std::string Starts;
  std::string View;
  double EyeHeightM = 1.7;
  double WalkMs = 1.4;
  double RunMs = 4.5;
};

struct PhysicsSettings {
  bool Declared = false;
  std::string Dial;
  double StepS = 1.0 / 60.0;
  int MostStepsInArrears = 8;
};

struct Clock {
  bool Declared = false;
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

struct Scenario {
  Identity Named;
  std::vector<Layer> Layers;
  WorldSettings Ground;
  std::vector<Provider> Providers;
  std::vector<Generator> Generators;
  std::vector<Compositor> Compositors;
  RenderPlan Render;
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
  std::vector<Sound> Sounds;
  std::vector<Bus> Buses;
  std::vector<Table> Tables;
  std::vector<Event> Events;
  std::vector<View> Views;
  std::vector<Body> Bodies;
  Player Played;
  Journey Routed;

  PhysicsSettings Motion;
  Clock Time;
  std::vector<Binding> Input;
  double WheelStepPx = 48.0;
  std::vector<Persisted> State;

  [[nodiscard]] const Asset *subject(void) const;
};

}

#endif
