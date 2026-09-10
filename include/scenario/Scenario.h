#ifndef OUTSHINE_SCENARIO_H
#define OUTSHINE_SCENARIO_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "Earth.h"
#include "math/Mat4.h"
#include "scene/Material.h"
#include "math/Quat.h"
#include "math/Vec3.h"

#include "Extent.h"
#include "render/Camera.h"

namespace outshine::Scenario {

/// What PatienceS stands at when a scenario declares none.
constexpr double kPatienceUnsaidS = 30.0;

/// What SightM stands at when a scenario declares none.
constexpr double kSightUnsaidM = 240000.0;

/// What Fps stands at when a scenario declares none.
constexpr double kFpsUnsaid = 60.0;

/// What a view's FovDeg stands at when it declares none: the vertical field of view, in degrees.
constexpr double kFovUnsaidDeg = 55.0;

/// What Fill stands at when a scenario declares none.
constexpr double kFillUnsaid = 0.9;

/// What RisesBy stands at when a scenario declares none.
constexpr double kRisesByUnsaid = 0.35;

/// How far a view may pitch before it would look past straight up or down, in degrees.
constexpr double kPitchLimitUnsaidDeg = 89.0;

/// The height a person's eye stands at when a scenario declares none, in metres.
constexpr double kEyeHeightUnsaidM = 1.7;

/// How fast a person walks and runs when a scenario declares neither, in metres per second.
constexpr double kWalkUnsaidMs = 1.4;
constexpr double kRunUnsaidMs = 4.5;

/// The simulation step a scenario gets when it declares none: one sixtieth of a second.
constexpr double kStepUnsaidS = 1.0 / kFpsUnsaid;

/// How far one notch of a wheel scrolls, in pixels.
constexpr double kWheelStepUnsaidPx = 48.0;

struct Light {
  double Lux = 0.0;
  /// Angle toward the source above the horizon, in degrees.
  double ElevationDeg = 0.0;
  /// Azimuth toward the source, clockwise from north (-Z in the East-Up-South frame).
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
  double CloudLow = 0.0, CloudMid = 0.0, CloudHigh = 0.0;
  double CloudBaseAglM = 0.0;
  double WindDeg = 0.0;
  double WindMs = 0.0;

  /// How much the air between eye and subject is allowed to whiten it, from 0 to 1.
  ///
  /// AERIAL PERSPECTIVE IS PHYSICS AND NOT DECORATION -- Rayleigh scattering over a long path is
  /// why a distant ridge is pale blue, and a renderer that omits it draws a cardboard cutout. But
  /// the amount is WEATHER: the same Jura ridge is razor-sharp on a cold clear morning and gone by
  /// noon in summer haze, and a scenario that wants to see the Alps from Venice is declaring the
  /// morning rather than switching off a shader.
  ///
  /// So this scales the scattering the atmosphere already computes. One is the physical amount for
  /// the declared air; zero is the clearest air the model can state. The DEFAULT IS ONE, because a
  /// scenario that says nothing about the weather gets the physics rather than a preference.
  double Haze = 1.0;
};

/// A ground STATED as a function of place instead of fetched as tiles.
///
/// A corpus that generates its own input can only grade itself unless something outside it states
/// the answer, and for terrain that something is arithmetic: on `z = f(x, y)` the correct height is
/// known at every point, so "the road hovers" becomes a subtraction rather than a look. It is also
/// how a case runs offline and in milliseconds.
struct Relief {
  /// Which function. Empty means the ground is fetched, which is every scenario that is not a test.
  std::string Kind;

  /// How far the function reaches from its middle, in metres. What it means depends on `Kind`: the
  /// height of a ridge, the throw of an escarpment, the strength of a noise field.
  double AmplitudeM = 0.0;

  /// The length of one period, in metres, for anything periodic.
  double WavelengthM = 0.0;

  /// A constant slope, as a ratio rather than a percentage, added under whatever else the function
  /// does. On its own it is a plane.
  double Gradient = 0.0;

  /// Which way the slope or the ridge faces, in degrees clockwise from north.
  double BearingDeg = 0.0;

  /// The seed every random choice in the function descends from, so one declaration is one ground
  /// twice.
  uint64_t Seed = 0;
};

/// One piece of map a scenario states itself, instead of fetching it.
///
/// A corpus needs an input it can vary one thing at a time -- a hairpin on a flat plain, the same
/// hairpin on a cliff -- and real map data cannot be varied at all. What it must never state is the
/// ANSWER: the terrain function and the design standards do that.
struct Structure {
  /// What the map would call it: `residential`, `motorway`, `track`, `building`, `water`.
  std::string Kind;

  /// The carriageway's width in metres where the map states one, and zero where it does not --
  /// which is the case a derivation has to survive.
  double WidthM = 0.0;

  /// How tall, in metres, for anything that rises.
  double HeightM = 0.0;

  /// Whether it encloses ground rather than running over it.
  bool Area = false;

  /// Whether the map calls it a bridge, so it carries over what it crosses rather than through it.
  bool Bridge = false;

  /// Whether the map calls it a tunnel, in which case nothing is drawn on the surface.
  bool Tunnel = false;

  /// Which level it runs on where two things cross: negative under, positive over, zero at grade.
  int Level = 0;

  /// Ordered WGS84 latitude/longitude degree pairs; latitudes in [-90,90], longitudes
  /// in [-180,180], all finite. XML accepts complete decimal/exponent pairs separated
  /// by XML whitespace, with no whitespace inside a pair; leading plus is unsupported.
  /// At least two points for ways, three for areas; maximum 65536 points per feature.
  /// This aggregate does not validate geometry; parsing rejects malformed coordinates
  /// without publishing a partial document. Ring simplicity and total budgets are separate.
  std::vector<double> LatLon;
};

struct WorldSettings {
  bool Declared = false;
  Georeference Origin;

  /// The ground as a function, when a scenario states one instead of fetching tiles.
  Relief Shape;

  /// The map a scenario states itself. Empty means the map is fetched, which is every scenario that
  /// is not a test.
  std::vector<Structure> Osm;
  double GravityMs2 = kStandardGravityMs2;
  double AirDensityKgM3 = kIsaSeaLevelDensityKgM3;
  Weather Sky;
  double PatienceS = kPatienceUnsaidS;
  double SightM = kSightUnsaidM;
};

struct Provider {
  std::string Kind;
  std::string Pin;
  int Rank = 0;
  std::string WhenAbsent;
};

/// Owned named textual setting. Copies retain both strings independently.
/// Values are not parsed here; each consumer defines names, units and value syntax.
/// This value type provides no validation or synchronization; string storage may allocate.
struct Setting {
  std::string Name;  ///< Exact case-sensitive setting identifier.
  std::string Value; ///< Owned value text, preserved without normalization.
};

/// Owned request for a registered generator, executed during Engine::declare preparation.
/// Copies own the registration name and settings. The engine borrows parameter views
/// only during the producer call; unknown registrations and producer refusals fail declare.
/// Generation may allocate and query providers. This declaration provides no synchronization.
struct Generating {
  std::string Kind; ///< Exact registered generator identity; empty or unknown names fail declare.
  /// Ordered settings forwarded without parsing. The generator validates supported values.
  /// Empty selects the generator's defaults; unsupported settings must be refused.
  std::vector<Setting> Parameters;
};

struct Compositor {
  std::string Kind;
  double BudgetPx = 0.0;
  bool On = true;
};

struct Patch {
  double LeftFrac = 0.0, TopFrac = 0.0, WidthFrac = 1.0, HeightFrac = 1.0;

  [[nodiscard]] bool whole() const {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

struct RenderPlan {
  bool Declared = false;
  Extent Frame;
  Patch Picture;
  double Fps = kFpsUnsaid;
  double Fill = kFillUnsaid;
  double OrbitDegPerFrame = 0.0;
  std::vector<std::string> Outputs;
  std::vector<std::string> Stages;
  std::string Transfer;
  double Exposure = 0.0;
  std::string Precision;

  /// Whether the engine walks its own geometry to publish what it finds there -- coincident
  /// corners, edges on one triangle, needles, triangles reaching too far. It answers questions
  /// about MESH QUALITY, which change when a generator changes and not between two frames, and it
  /// costs 11.3 s of Shibuya's 19 s load. Off by default: a tool is used surgically, never by
  /// habit.
  bool Audits = false;
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
  /** Numeric traits parsed as complete finite decimal values during assembly.
   * Instance values override inherited defaults; invalid or out-of-range values reject assembly.
   */
  std::vector<Setting> Attributes;
};

struct Instance {
  std::string Of;
  std::string Id;
  std::string In;
  Standing Stands;
  /** Numeric traits parsed as complete finite decimal values during assembly.
   * Instance values override inherited defaults; invalid or out-of-range values reject assembly.
   */
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

/** Owned declarative DSP node; setup validates IDs, inputs and processor parameters. */
struct Voice {
  /** Nonempty identifier, unique within the containing Sound::Graph. */
  std::string Id;
  /** Processor kind; generators produce signals, other processors consume summed inputs. */
  Makes Does = Makes::Oscillator;
  /** Input node IDs in summation order; forward references allowed, cycles rejected.
   * Repeated IDs contribute repeatedly. Feedback belongs inside delay processors.
   */
  std::vector<std::string> From;
  /** Owned processor settings; validated and copied into runtime state at setup. */
  std::vector<Setting> Parameters;
};

struct Sound {
  std::string Id;
  std::string Uri;
  /** Owned acyclic signal graph; its last declared node is the mono source output.
   * Declaration order otherwise does not constrain execution. Empty selects no synth.
   */
  std::vector<Voice> Graph;
  bool Streamed = false;
  /** Owned body name; audio setup requires a unique placed body in the current assembly.
   * Empty leaves the source unbound; positional unbound sources are silent.
   */
  std::string On;
  std::string Bus;
  Emitter Heard;
  bool Loops = false;
  double GainDb = 0.0;
  double SendShare = 0.0;
};

/** Owned reverberation settings; setup validates declared settings before publication. */
struct Room {
  /** Enable interpretation of these settings; false ignores the remaining fields. */
  bool Declared = false;
  /** Finite nonnegative decay time in seconds to fall by 60 dB; zero disables the effect. */
  double SecondsRt60 = 0.0;
  /** Finite damping coefficient in [0,1]; larger values suppress high frequencies more. */
  double Damping = 0.5;
  /** Finite wet output gain in [0,1], added to the dry signal; zero mutes the effect. */
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

/// Source of the camera pose configured by a scenario view.
enum class CameraPlacement {
  FollowEntity, ///< Resolve the named followed entity during simulation.
  Local,        ///< Use the camera's local world pose directly.
  Geodetic      ///< Resolve geographic position and optional terrain height before rendering.
};

/// Geographic camera placement, resolved by scenario/world setup; never an import type.
struct GeographicCameraPlacement {
  /// Longitude/latitude in degrees and ellipsoidal height in metres, or height above ground.
  LongitudeLatitudeHeight Geodetic;
  /// Interpret height as metres above the sampled terrain instead of absolute height.
  bool SamplesHeight = false;
  /// Viewing azimuth clockwise from north, degrees.
  double BearingDeg = 0.0;
  /// Viewing elevation above the horizon, degrees.
  double PitchDeg = 0.0;
};

struct View {
  std::string Id;
  Camera Sees;
  /// Selects how the scenario resolves the camera pose.
  CameraPlacement Placement = CameraPlacement::FollowEntity;
  /// Geographic inputs used only when Placement is Geodetic.
  GeographicCameraPlacement Geographic;
  Patch Viewport;

  void setCamera(const Camera &sees) { Sees = sees; }

  void setViewport(const Patch &over) { Viewport = over; }

  void setScene(std::string named) { In = std::move(named); }

  [[nodiscard]] const std::string &scene() const { return In; }

  std::string In;
  /** Owned name of the unique placed body followed by this view.
   * Assembly resolves it; a removed target must be rebound by assembling again.
   */
  std::string Follows;
  std::string Person;
  Vec3 OffsetM;
  double DistanceM = 0.0;
  double RisesBy = kRisesByUnsaid;
  double PitchLimitDeg = kPitchLimitUnsaidDeg;
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
  Vec3 AtM;
  Prismatic Strut;
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

  double AssetSpanM = 0.0;

  double AssetGround = 0.0;
  double AssetCentreX = 0.0, AssetCentreZ = 0.0;
  Vec3 CentreOfMassM;
  Vec3 InertiaKgM2;
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
    double aheadM = 0.0;
    double behindM = 0.0;
    int ahead = 0;
    int behind = 0;
    for (const Contact &one : Contacts) {
      if (one.AtM[2] < CentreOfMassM[2]) {
        aheadM += one.AtM[2];
        ++ahead;
      } else if (one.AtM[2] > CentreOfMassM[2]) {
        behindM += one.AtM[2];
        ++behind;
      }
    }
    return ahead > 0 && behind > 0 ? std::fabs(behindM / static_cast<double>(behind) -
                                               aheadM / static_cast<double>(ahead))
                                   : 0.0;
  }

  [[nodiscard]] double acrossM() const {
    double leastM = 0.0;
    double mostM = 0.0;
    for (const Contact &one : Contacts) {
      leastM = one.AtM[0] < leastM ? one.AtM[0] : leastM;
      mostM = one.AtM[0] > mostM ? one.AtM[0] : mostM;
    }
    return mostM - leastM;
  }
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

/// Owned fixed-step configuration copied by Engine::declare; not a running clock.
/// Mutate only outside concurrent access. Declaration rejects nonfinite/nonpositive
/// StepS, nonpositive catch-up limits and an unrepresentable product of the two.
struct PhysicsSettings {
  bool Declared = false; ///< Presence in serialization; Engine validates timing regardless.
  std::string Dial;      ///< Owned clock label retained by the schema; no runtime routing yet.
  double StepS = kStepUnsaidS; ///< Fixed simulation interval in seconds, finite and positive.
  /// Maximum fixed steps per elapsed-time advance call; positive, not a wall-time budget.
  /// The current runtime may discard excessive backlog after reaching this limit.
  int MostStepsInArrears = 8;
};

/// Owned world-time declaration, distinct from simulation step accumulation.
/// Current runtime samples the astronomical sun during declaration; this structure
/// does not itself advance time or synchronize access. Copying Start may allocate.
struct Clock {
  bool Declared = false; ///< Whether a clock section was explicitly supplied.
  /// Permit system UTC fallback when Start is absent or invalid during sky setup.
  /// Does not currently promise continuous resampling of the system clock.
  bool Live = false;
  /// Owned ISO 8601 UTC starting instant. During automatic sun setup an invalid value
  /// fails when Declared and not Live; otherwise system UTC is used as fallback.
  std::string Start;
  double Rate = 1.0; ///< Declared world-time scale, retained but not yet applied by runtime.
};

/// Owned input-event to host-action mapping, copied by Engine::declare.
/// Buttons deliver one numeric argument (1 pressed, 0 released); key repeats are ignored.
/// Stick axes deliver [-1, 1], triggers [0, 1], mouse axes relative pixel deltas.
/// No deadzone, smoothing, device selection or aggregation across bindings is applied.
struct Binding {
  /// Exact case-sensitive event: KeyW/KeyA/KeyS/KeyD, Space, Escape,
  /// ArrowUp/ArrowDown/ArrowLeft/ArrowRight, PageUp/PageDown, MouseLeft/MouseRight,
  /// GamepadSouth/GamepadEast, MouseX/MouseY, AxisLeftX/AxisLeftY,
  /// AxisRightX/AxisRightY, TriggerLeft/TriggerRight. Unknown names reject declare.
  std::string Event;
  /// Nonempty host action name, copied and passed literally to Host::calls, not parsed
  /// as a script. Multiple events may share an action; each event calls it separately.
  std::string Action;
};

struct Persisted {
  std::string What;
};

struct Document {
  Identity Named;
  std::vector<Layer> Layers;
  WorldSettings Ground;
  std::vector<Provider> Providers;
  std::vector<Generating> Generators;
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

  PhysicsSettings Motion;
  Clock Time;
  /// Owned bindings. Duplicate events or empty actions reject declare; failure retains
  /// prior bindings, success replaces them, and an empty list clears them. Dispatch via
  /// handleEvent requires an offered Host for bound actions, but no render target.
  std::vector<Binding> Input;
  /// Vertical UI pixels per wheel unit; finite and nonnegative, validated by declare.
  /// Zero disables wheel movement. Applied to SDL's already direction-adjusted event value.
  double WheelStepPx = kWheelStepUnsaidPx;
  std::vector<Persisted> State;

  [[nodiscard]] const Asset *subject() const;
};

}

#endif
