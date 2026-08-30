#ifndef OUTSHINE_SCENARIO_H
#define OUTSHINE_SCENARIO_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Material.h"

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

// WHERE ON THE EARTH THE SCENE STANDS, and Cesium's word for it. A `WorldSettings` that also
// carries gravity, air density, wind and a streaming patience is not a georeference; this is the
// part that is, and separating it is why the name can be used honestly at all.
struct Georeference {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  double RadiusM = 6371008.8;
};

// WHAT A CLIENT DECLARES INSTEAD OF A SKY. There is no picture to hand in, so these are the two
// things that decide what the sky looks like: WHEN it is, which puts the sun, the moon and the
// stars, and WHAT THE WEATHER IS, which decides how their light arrives. Both belong to the door
// because without them a client can only take whatever the engine happened to default to.
//
// The sun's ELEVATION and BEARING are not here and must not be: they are computed from the place
// and the hour, and a scenario that declares both a clock and a hand-set sun is REFUSED, because
// over a place on Earth only one of the two can be true.
struct Weather {
  double CloudCover = 0.0;
  double CloudLow = 0.0, CloudMid = 0.0, CloudHigh = 0.0;
  double CloudBaseAglM = 0.0;
  double WindDeg = 0.0;
  double WindMs = 0.0;
};

// THERE IS NO SKYBOX HERE AND THAT IS THE BETTER ANSWER, not a missing one. Filament's `Skybox` is
// an image or a colour a scene shows where nothing else does; this engine's sun, moon and stars are
// REAL -- they stand where the georeference and the clock put them, and the sky is computed from
// that and the weather. A client hands in no picture of the sky because there is nothing it could
// hand in that would agree with its own shadows the moment the clock moves. What it declares is
// the weather, and `Weather` above is that.
struct WorldSettings {
  bool Declared = false;
  Georeference Origin;
  double GravityMs2 = 9.80665;
  double AirDensityKgM3 = 1.2250;
  Weather Sky;
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

// WHETHER A CLIP REPEATS IS A DECLARATION, not a default. glTF states what SAMPLING does outside
// a clip's range -- it clamps to the last keyframe -- and says nothing about whether an engine
// starts it again; Unreal makes it `bLooping` on the instance and RAGE carries a loop flag on the
// clip, so both agree it is the client's word. `Play` runs once and holds its end pose, which is
// glTF's own rule; `Loop` wraps.
enum class AssetAnimation { Play, Loop, Ignore, Driven };

// WHAT A SURFACE OF SOMEBODY ELSE'S FILE IS, SAID BY THE CLIENT THAT LOADS IT. Unreal overrides a
// component's material per slot (`SetMaterial`); Filament hands out a `MaterialInstance` per
// primitive and lets a client set its parameters. **They agree** that the file's own materials are
// a DEFAULT rather than a fact, and a client that renders another party's asset against a
// reference has to be able to say what the surfaces are -- 107 of the 148 Khronos cases here do
// exactly that, and before this they could only do it by reaching past the door.
//
// The match is by NAME because that is what a file states and what a manifest quotes; an index
// moves when the file is re-exported and a name does not.
struct SurfaceOverride {
  std::string Named;

  // OR THE NODE. Unreal overrides a material per COMPONENT SLOT and Filament hands out a
  // `MaterialInstance` per PRIMITIVE; **they agree** that the key is the PART rather than the
  // material, and a file where two parts share one material has no other way to tell them apart.
  // Measured: the five quads of Khronos's AlphaBlendModeTest share a single material and the case
  // declares a colour for each, which a key on the material alone cannot say.
  std::string Node;

  // OR THE PART'S INDEX, which is the only key a file that names NEITHER can be told apart by.
  // Filament keys `getMaterialInstanceAt(instance, primitiveIndex)` and Unreal keys
  // `SetMaterial(int32 ElementIndex, ...)`: both address the slot by ORDINAL, and neither asks the
  // asset for a name it may not carry. Measured: Khronos's BoxInterleaved names no material and no
  // node, so `Named` and `Node` have nothing to match and the row cannot be reached at all.
  int Part = -1;

  // AND WHETHER THE ASSET'S MAPS SURVIVE IT. Unreal has BOTH verbs and they are not the same one:
  // `SetMaterial(ElementIndex, ...)` swaps the material entire and the mesh's own textures go with
  // it, while `CreateDynamicMaterialInstance` keeps the parent and changes only its parameters.
  // Filament draws the same line -- a fresh `MaterialInstance` against one duplicated from the
  // existing. Replacing is the default because it is the one a client reaches for when it states
  // what a surface IS; keeping is for a client that means "this asset's surface, with THIS row".
  // Measured both ways on the Khronos corpus: a declared flat emission that kept the avocado's
  // photograph came out a hundred hues, and a case whose colour is the FILE's base map came out
  // flat when the map was dropped.
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
  // THE NEAR PLANE A FRAME STANDS ON WHEN NOTHING DECLARES ONE, in metres. A client that reads a
  // DEPTH attachment back needs it to turn what it read into a range, and it had to reach into the
  // renderer to find out. It is stated here and the renderer takes it from here, so there is one
  // holder and not two that agree until they do not.
  static constexpr double kNearestM = 0.05;

  bool Placed = false;
  Standing Stands;
  double FovDeg = 0.0;

  double NearM = 0.0;
  double FarM = 0.0;

  // GLTF HAS TWO CAMERAS AND SO DOES FILAMENT: a perspective one stating a field of view, and an
  // orthographic one stating how many metres the frame spans. A door that carried only the first
  // could not express half of what a file declares, and a conformance case that renders an
  // orthographic asset against a reference has no way to say so.
  bool Orthographic = false;
  double XMagM = 0.0, YMagM = 0.0;

  // FILAMENT'S CAMERA IS GIVEN ITS PROJECTION AS ONE CALL -- a field of view, a near and a far --
  // and a client that knows that reaches for the verb rather than for three fields. Here it says
  // the same thing into a declaration, which is the tree's own shape: the engine still behaves
  // rather than obeys, and a section left undeclared keeps the engine's own default.
  void setProjection(double fovDeg, double nearM, double farM) {
    Orthographic = false;
    FovDeg = fovDeg;
    NearM = nearM;
    FarM = farM;
  }

  void setProjection(
      double leftM, double rightM, double bottomM, double topM, double nearM, double farM) {
    Orthographic = true;
    XMagM = 0.5 * (rightM - leftM);
    YMagM = 0.5 * (topM - bottomM);
    NearM = nearM;
    FarM = farM;
  }

  // THE MATRICES A CLIENT PROJECTS WITH. Filament hands out `Camera::getViewMatrix()` and
  // `getProjectionMatrix()` for exactly this reason: a client that wants to know WHERE a point
  // lands on the frame -- to place a label, to test a pick, to score a render against a reference
  // -- would otherwise rebuild the arithmetic and be wrong in a way nothing catches. `clipMatrix`
  // is the two composed, which is the one a point is multiplied by. Column-major, sixteen doubles.
  [[nodiscard]] bool viewMatrix(double outM16[16]) const;
  [[nodiscard]] bool projectionMatrix(double aspect, double outM16[16]) const;
  [[nodiscard]] bool clipMatrix(double aspect, double outM16[16]) const;

  bool LooksAt = false;
  double LookAtM[3] = {0.0, 0.0, 0.0};
  // WHICH WAY IS UP, AS FILAMENT SPELLS IT. `Camera::lookAt(eye, center, up)` takes a vector and
  // not an angle, and the reason is that an angle needs a convention: measured from what, positive
  // which way. This door carried `RollRad` and never stated one, so a client holding the camera's
  // own up had to recover an angle and guess -- and the guess came out negated on the one corpus
  // case that rolls. The default is world up, which is what an unrolled camera means.
  double UpM[3] = {0.0, 1.0, 0.0};

  // FILAMENT'S CAMERA TAKES A PHOTOGRAPHIC EXPOSURE -- aperture in f-stops, shutter in seconds,
  // sensitivity in ISO -- and computes the scale from them, because those three are what a
  // photographer knows and a bare multiplier is not. The engine keeps the multiplier (`Exposure`
  // in the render section) and this DERIVES it, so a scenario may say either and they cannot
  // disagree: the derivation is the standard one, EV = log2(N^2 / t) - log2(S / 100).
  double ApertureFStops = 0.0;
  double ShutterS = 0.0;
  double SensitivityIso = 0.0;

  [[nodiscard]] bool exposed(void) const {
    return ApertureFStops > 0.0 && ShutterS > 0.0 && SensitivityIso > 0.0;
  }

  void setExposure(double apertureFStops, double shutterS, double sensitivityIso) {
    ApertureFStops = apertureFStops;
    ShutterS = shutterS;
    SensitivityIso = sensitivityIso;
  }

  [[nodiscard]] double exposureScale(void) const;
};

// FILAMENT'S VIEW IS SET WITH VERBS -- `setCamera`, `setViewport`, `setScene` -- because its scene
// is assembled by calls. Here a scenario DECLARES, so the same three names stand on the same three
// fields and mean "say what this view is". A reader who knows Filament reaches for the verb and
// finds it; the engine still behaves rather than obeys, which is this tree's own rule.
struct View {
  std::string Id;
  Camera Sees;
  Patch Viewport;

  void setCamera(const Camera &sees) { Sees = sees; }

  void setViewport(const Patch &over) { Viewport = over; }

  void setScene(std::string named) { In = std::move(named); }

  [[nodiscard]] const std::string &scene(void) const { return In; }

  std::string In;
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
    return ahead > 0 && behind > 0 ? std::fabs(behindM / (double)behind - aheadM / (double)ahead)
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

// WHEN IT IS, AND WHETHER THAT KEEPS MOVING. `Live` is the CLOCK OF THE MACHINE: the scene stands
// at the real hour and goes on standing there as it passes, which is what a client wants when it
// is showing a place rather than reproducing a picture. `Start` is one stated instant in ISO 8601
// UTC, which is what a client wants when the picture has to be the SAME one tomorrow.
//
// UNDECLARED USED TO MEAN LIVE AND NOBODY COULD SAY SO. The engine fell back to `std::time` when a
// clock was absent, so real time was reachable only by leaving something out -- and a default is
// not an answer a client can state, argue with, or read back. Both are now sayable and the
// fallback is unchanged, so nothing that stood before moves.
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

} // namespace outshine

#endif
