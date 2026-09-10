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

/// Directional key-light declaration. Scalar values are copied; this aggregate validates none.
struct Light {
  /// Illuminance in lux on a surface normal to the incoming light; zero declares no intensity.
  double Lux = 0.0;
  /// Angle toward the source above the horizon, in degrees.
  double ElevationDeg = 0.0;
  /// Azimuth toward the source, clockwise from north (-Z in the East-Up-South frame).
  double BearingDeg = 0.0;
};

/// Owned scenario metadata, copied with Document; copying strings may allocate.
/// Mutation requires exclusive access. Version is metadata, not a schema-version validator.
struct Identity {
  std::string Name;    ///< Human-readable scenario name; empty is unnamed.
  std::string Version; ///< Opaque version text preserved by scenario import/export.
  double Epoch = 0.0;  ///< Opaque numeric metadata; no runtime time unit or behavior is assigned.
  double Decay = 0.0;  ///< Opaque numeric metadata; no runtime decay rule is assigned.
  std::string Active;  ///< Space-separated layer-set names; layers without a set are always active.
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
/// XML parsing and Engine::declare validate kind, scalar values and coordinate pairs
/// before publication. Direct field mutation does not validate; polygon topology and
/// total scene budgets require separate checks. Per-feature validation allocates nothing.
struct Structure {
  /// What the map would call it: `residential`, `motorway`, `track`, `building`, `water`.
  std::string Kind;

  /// The carriageway's width in metres where the map states one, and zero where it does not --
  /// which is the case a derivation has to survive. XML requires finite nonnegative
  /// decimal/exponent metres; missing selects zero, invalid explicit values fail parsing.
  double WidthM = 0.0;

  /// Height in finite nonnegative metres; zero means unspecified. XML accepts complete
  /// decimal/exponent values, defaults to zero when absent and rejects invalid values.
  double HeightM = 0.0;

  /// Whether it encloses ground rather than running over it.
  bool Area = false;

  /// Whether the map calls it a bridge, so it carries over what it crosses rather than through it.
  bool Bridge = false;

  /// Whether the map calls it a tunnel, in which case nothing is drawn on the surface.
  bool Tunnel = false;

  /// Relative level at crossings: negative below, positive above, zero at grade.
  /// XML requires a complete decimal integer in the int range; missing selects zero.
  /// This is a topological ordering hint, not an elevation in metres.
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

/// Normalized image region, measured from the target's upper-left corner.
/// Scalar descriptor; no allocation, clamping or validation. Mutation requires exclusive access.
struct Patch {
  double LeftFrac = 0.0;   ///< Left edge divided by target width.
  double TopFrac = 0.0;    ///< Top edge divided by target height, increasing downward.
  double WidthFrac = 1.0;  ///< Width divided by target width; one spans the full width.
  double HeightFrac = 1.0; ///< Height divided by target height; one spans the full height.

  /// @return Whether all four components exactly describe the full target; no tolerance.
  /// Constant time, no allocation or mutation; does not validate other regions.
  [[nodiscard]] constexpr bool whole() const noexcept {
    return LeftFrac == 0.0 && TopFrac == 0.0 && WidthFrac == 1.0 && HeightFrac == 1.0;
  }
};

/// Owned render configuration copied with Document; strings and lists may allocate on copy.
/// Mutation requires exclusive access. Names are checked during render-plan construction;
/// numeric validation is not uniform yet. Construction alone does not validate a plan.
struct RenderPlan {
  bool Declared = false; ///< Whether this section participates in scenario declaration/merging.
  Extent Frame;  ///< Requested image dimensions in pixels; the host supplies the actual target.
  Patch Picture; ///< Normalized image region; currently available through the native API only.
  /// Nominal frames per second, also used by the current asset-animation sampling path.
  /// Positive values replace the engine default; this is not a wall-clock pacing guarantee.
  double Fps = kFpsUnsaid;
  /// Dimensionless automatic camera-framing fill. Positive values request framing;
  /// an explicitly bound camera takes precedence. Nonpositive values use contextual defaults.
  double Fill = kFillUnsaid;
  double OrbitDegPerFrame =
      0.0; ///< Automatic camera orbit increment per update, in degrees; zero disables.
  /// Additional named render resources to retain. Standard frame/presentation outputs remain;
  /// empty requests no extras. Unknown names fail plan construction.
  std::vector<std::string> Outputs;
  /// Explicit stage selection; nonempty replaces automatic selection, not dependency ordering.
  /// Empty selects stages from scene content. Unknown or incompatible stages fail construction.
  std::vector<std::string> Stages;
  /// Display transfer: empty keeps the default, otherwise "linear" or "filmic".
  /// An explicit transfer requires a display-transfer stage in the compiled plan.
  std::string Transfer;
  /// Positive linear exposure multiplier, not EV. Nonpositive values use the current
  /// light-meter/default path. Explicit exposure conflicts with an autoExposure stage.
  double Exposure = 0.0;
  /// Scene-radiance storage: empty keeps the default, "half" uses 16-bit floats,
  /// "float" uses 32-bit floats. An explicit choice requires scene-radiance resources.
  std::string Precision;
  bool Audits = false; ///< Enable CPU mesh-quality diagnostics; work scales with geometry size.
};

/// Owned lighting declaration; scalar values only, no allocation on copy.
/// Mutation requires exclusive access. This aggregate does not validate numeric values.
struct Lighting {
  bool Declared = false; ///< Whether this section participates in scenario declaration/merging.
  Light Key; ///< Directional illuminance and source angles; clock-driven sun selection is separate.
  Vec3 IndirectLight; ///< Additive environment radiance in scene-linear RGB; zero adds no constant
                      ///< term.
  /// Positive shadow-frame radius in metres. Nonpositive values derive half the mesh-bounds
  /// diagonal where geometry is available; this does not specify shadow-map resolution.
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

/** Owned trigger declaration, copied by Engine::declare and validated by Engine::assemble.
 * Assembly accepts at most 256 volumes and preserves the previous simulation on rejection.
 * Triggers sample simulated body centers in the local world frame; they do not intersect
 * body shapes or sweep motion between ticks. Boundary points count as inside.
 * Runtime occupancy is separate from this record and resets on successful assembly.
 * Mutating the caller's record does not change the engine's copy. Synchronize access
 * to a shared record externally; copying its strings may allocate.
 */
struct Volume {
  /// Diagnostic label and scenario-layer replacement key; assembly does not require uniqueness.
  std::string Id;
  /// Region metadata only: currently neither resolved nor used to transform or filter probes.
  std::string In;
  /// Exact shape name: "box" (also an empty string) or "sphere"; other names reject assembly.
  std::string Shape;
  /// Finite center in local world meters, in the same frame as simulated body positions.
  Vec3 AtM;
  /** Finite nonnegative dimensions in meters, validated on every axis.
   * Box uses axis-aligned half-extents; sphere uses X as radius and ignores Y/Z.
   * Zero dimensions are valid, including a point volume when all dimensions are zero.
   */
  Vec3 ExtentM;
  /// Exact name of a declared Event; missing targets reject assembly.
  std::string Fires;
  /** Exact transition: "enter", "exit" or "dwell"; empty/unknown values reject assembly.
   * Enter fires on first observed inclusion; exit requires a previously observed inclusion.
   * Dwell fires once per uninterrupted occupancy; leaving and reentering starts it again.
   * Both XML import and native assembly require a nonempty transition.
   */
  std::string When;
  /// Finite positive simulation seconds required for "dwell"; unused for other transitions.
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

/// Owned typed table declaration, copied by declare() and validated/parsed by assemble().
/// Assembly publishes tables with the simulation only after the complete candidate succeeds;
/// invalid tables preserve the previous simulation. Later source edits do not update it.
/// Configure outside simulation steps; serialize mutation with reads of this descriptor.
/// Setup copies strings, parses numeric cells and allocates storage plus a key index.
struct Table {
  std::string Id; ///< Nonempty, case-sensitive identifier, unique among declared tables.
  /// Ordered nonempty, unique names; at least one column. Names are matched exactly.
  std::vector<std::string> Columns;
  /// Positional types: true means number, false means text. Omitted trailing types mean text;
  /// more type entries than columns is an error. Numeric and text lookups never coerce types.
  std::vector<bool> Types;
  /// At most 4096 rows, each exactly Columns.size() cells; zero rows is valid.
  /// The first cell's unchanged spelling is the unique row key, including numeric spellings
  /// and the empty string. Numeric cells require finite decimal values with optional sign
  /// and exponent, no whitespace/suffix and no overflow/underflow. Text is stored unchanged.
  std::vector<std::vector<std::string>> Rows;
};

/// Owned event declaration, copied by declare() and prepared with the simulation by assemble().
/// Catalogs may contain at most 65536 events (16-bit indices 0..65535). Empty catalogs are
/// valid. Assembly validates names even without trigger volumes; rejection retains the old
/// simulation. Source mutations do not update the prepared catalog. Serialize writes with
/// reads of this descriptor; copying names/field lists and preparing a catalog may allocate.
struct Event {
  std::string Name; ///< Nonempty, case-sensitive identifier, unique within the event catalog.
  /// Owned, case-sensitive field names; assembly rejects empty names and duplicates within
  /// this event. An empty list is valid; field names may recur in other events.
  /// Internal listener registration checks its requested names against this list.
  /// This declares permitted names only: trigger emission currently carries event index and
  /// entity handle, not values for these fields. No scripted payload execution is implied.
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

/// Owned camera configuration. Engine::declare copies it; later edits do not update the Engine.
/// IDs/mode/follow labels/clock scale are checked during declaration. Following requires
/// assemble(); geographic resolution and camera/projection checks can fail during advance().
/// Configure outside frames; serialize mutation with reads of the same descriptor. Strings
/// may allocate. This descriptor owns no renderer, entity or device resource.
struct View {
  std::string Id; ///< Nonempty, case-sensitive identifier, unique within the view catalog.
  /// Projection/lens settings; also the pose for Local. FollowEntity replaces the pose;
  /// Geodetic replaces the position and, without LooksAt, derives direction from bearing/pitch.
  Camera Sees;
  /// One of the three declared modes; unsupported enum values reject declaration.
  CameraPlacement Placement = CameraPlacement::FollowEntity;
  /// Geographic inputs used only when Placement is Geodetic; terrain may require streaming.
  GeographicCameraPlacement Geographic;
  Patch Viewport; ///< Stored normalized viewport metadata; not applied by the current renderer.

  /// Copy camera values; no validation, resource access or retained reference to the input.
  /// @param sees Projection and pose configuration, resolved later according to Placement.
  void setCamera(const Camera &sees) { Sees = sees; }

  /// Copy viewport metadata; does not configure a render region or validate the patch.
  /// @param over Normalized patch, borrowed only for this copy.
  void setViewport(const Patch &over) { Viewport = over; }

  /// Replace the owned scene label; no scene lookup or rendering effect is implemented.
  /// @param named Label transferred into this descriptor.
  void setScene(std::string named) { In = std::move(named); }

  /// Borrow the stored scene label, without allocation or scene lookup.
  /// @return Reference to In, valid until this View is destroyed or relocated. Assignment
  /// changes the observed value; character pointers can be invalidated by label mutation.
  [[nodiscard]] const std::string &scene() const { return In; }

  std::string In; ///< Owned scene-label metadata; does not select a runtime world or scene.
  /// Owned name of the unique placed body resolved by assemble() for FollowEntity.
  /// A removed target must be rebound by assembling again. Empty rejects FollowEntity.
  std::string Follows;
  /// With nonempty Follows, must be "first" or "third". The label does not position the
  /// camera: DistanceM selects whether the current follower places the eye behind the body.
  std::string Person;
  /// Metres: body-local seat offset for FollowEntity; world-local displacement otherwise.
  /// Geodetic local axes are east/up/south. Numeric validity is not fully checked at declare.
  Vec3 OffsetM;
  /// FollowEntity only: positive metres behind the seat; nonpositive puts the eye at the seat.
  /// A trailing camera aims at the seat; a seated camera looks along the body's forward axis.
  double DistanceM = 0.0;
  /// FollowEntity with positive DistanceM: dimensionless rise along body-up per metre behind.
  double RisesBy = kRisesByUnsaid;
  /// Stored pitch-limit metadata in degrees; no runtime clamp is currently implemented.
  double PitchLimitDeg = kPitchLimitUnsaidDeg;
  /// Stored dimensionless factor, required finite and positive by declare. Currently does
  /// not scale simulation time or presentation time; it is not a working time-dilation control.
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
