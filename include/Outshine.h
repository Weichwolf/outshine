#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <cstdint>
#include <memory>
#include <span>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "Earth.h"
#include "Logging.h"
#include "generation/Generate.h"
#include "scenario/Event.h"
#include "scenario/Scenario.h"
#include "scene/Geometry.h"
#include "world/EntityRegistry.h"

namespace outshine {

/// What a door verb gives back: the value it was asked for, or the reason it could not be had.
///
/// A refusal that carries only `false` makes the client invent the reason, and the reasons here are
/// not interchangeable -- a tile that has not arrived and a place outside the declared world are
/// different answers to the same question.
template <typename Value> using Holds = std::expected<Value, std::string>;

/// A verb with nothing to give back but its refusal.
using Result = Holds<void>;

/// Value snapshot of terrain/vector requests and tile-pool accounting. Copies own
/// their values and may be read independently; they do not track later engine changes.
/// Counts describe different streaming stages, not completed GPU residency.
struct Loading {
  size_t GroundWanted = 0;  ///< Terrain requests in the current engine request set.
  size_t GroundArrived = 0; ///< Requested terrain entries no longer pending.
  size_t VectorWanted = 0;  ///< Retained OSM tiles plus currently pending OSM tiles.
  size_t VectorArrived = 0; ///< OSM tiles retained by the vector field.
  size_t Outstanding = 0;   ///< Outstanding tile-pool work; not the sum of missing entries.
  double FetchedMB = 0.0;   ///< Cumulative tile-pool payload in MiB (2^20 bytes), despite the name.
  double Megabits = 0.0;    ///< Preload callback estimate: FetchedMB * 8 / ElapsedS, in Mibit/s.
  double MeanFetchMs = 0.0; ///< Accumulated fetch time divided by pool post count, in milliseconds.
  double ElapsedS = 0.0;    ///< Seconds since this preload call began; zero in loading() snapshots.

  /// Ratio of arrived to wanted terrain/vector entries, without allocation.
  /// Returns one when nothing is wanted. Does not clamp inconsistent caller-created
  /// snapshots or establish readiness of generated geometry, uploads or rendering.
  /// Integer sums cannot overflow; large counters are approximated in double precision.
  /// @return Received-entry ratio, or one for an empty request set.
  [[nodiscard]] constexpr double share() const noexcept {
    const double wants = static_cast<double>(GroundWanted) + static_cast<double>(VectorWanted);
    return wants == 0.0
               ? 1.0
               : (static_cast<double>(GroundArrived) + static_cast<double>(VectorArrived)) / wants;
  }
};

/// Owned filesystem configuration for subsequent asset/provider setup. Relative
/// paths resolve against the process working directory; no validation occurs here.
/// This value does not own files, open providers or already loaded resources.
struct Roots {
  std::string Assets;  ///< Base directory for scenario asset URIs resolved by the engine.
  std::string Shipped; ///< Base directory for shipped fonts, sky and generator data.
  std::string Cache;   ///< Tile/provider cache directory; backend policies govern persistence.
  bool Offline =
      false; ///< Reject creation of the engine's fetching service; not a process-wide network ban.
};

class Engine;

/// Readback attachment selector. Availability depends on the compiled render plan.
/// Diagnostic attachments describe rasterized surfaces, not persistent world objects.
enum class Buffer {
  Colour,        ///< Display-encoded RGBA8; use the byte readPixels overload.
  Linear,        ///< Four floats per pixel: scene-linear RGBA before display encoding.
  Depth,         ///< One float per pixel: raw device depth, not distance in metres.
  ShadingNormal, ///< Four floats: renderer-space normal XYZ and facing sign; unlit XYZ may be zero.
  SurfaceIdentity, ///< Four floats: renderer surface identifier in X, remaining channels
                   ///< diagnostic.
  Velocity         ///< Two floats: current minus previous normalized-device XY position.
};

/// Borrowed access to an Engine's current presentation target, not an independently owned window.
/// Copies refer to the same Engine and observe subsequent target changes. The Engine and its
/// borrowed SDL window must outlive all uses. Calls follow Engine's thread restrictions.
class SwapChain {
public:
  /// Current drawable dimensions in physical pixels; zero before target configuration.
  /// @return Current target extent in physical pixels.
  [[nodiscard]] Extent extent() const;
  /// Whether the current target presents to an SDL window rather than an offscreen buffer.
  /// @return True for a window target, false otherwise.
  [[nodiscard]] bool presents() const;

private:
  friend class Engine;
  friend class Renderer;

  explicit SwapChain(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

/// Borrowed renderer facade. Copies share frame state; they do not create separate renderers.
/// The originating Engine must outlive every use. Target changes preserve facade identity;
/// scenario changes affect subsequent calls. Concurrent calls are not supported.
class Renderer {
public:
  /// Begin a frame on this renderer's Engine. A foreign target is rejected without changing
  /// either Engine. A repeated begin is rejected while preserving the existing open scope.
  /// The target must have positive dimensions; rendering setup may fail.
  /// @param into Borrowed target facade belonging to this renderer's Engine.
  /// @return Success or an owned target/state/setup error.
  [[nodiscard]] Result beginFrame(SwapChain &into);
  /// Close a frame opened by beginFrame() on the same Engine, including facade copies.
  /// Window targets draw/present the current scene; offscreen targets only close the scope.
  /// An unopened frame returns an owned error. The scope is closed even if presentation fails.
  /// Runs on the Engine's video thread and may wait for GPU/presentation resources.
  /// @return Success or an owned frame-state/presentation error.
  [[nodiscard]] Result endFrame();

  /// Block the Engine's video thread until submitted GPU work has completed.
  /// No scene is a successful no-op. Does not open, close, draw or present a frame.
  /// Serialize with other Engine calls; use outside latency-critical callbacks.
  /// @return Success or an owned device-state/GPU-wait error, including the SDL diagnosis.
  [[nodiscard]] Result flushAndWait();

  /// Request a frame from this Engine's current scene. A zero extent uses the configured
  /// target; a positive extent must match it. Invalid extents are rejected before scene setup.
  /// Calls are serialized with other Engine work.
  /// A camera must be bound or derivable from object bounds. advance() applies a declared
  /// scenario view; an empty scene without a prepared view returns an error.
  /// @param frame Optional check of the target size in physical pixels; zero selects the target.
  /// @return An error if scene/camera preparation fails; submission does not imply GPU completion.
  [[nodiscard]] Result render(Extent frame);
  /// Draw and synchronously read back the current target, then write an RGBA PNG.
  /// Runs on the Engine/video thread; may allocate, wait for the GPU and perform IO.
  /// Parent directories are created as needed; an existing file is overwritten.
  /// @param path Borrowed filesystem path, copied for writing; relative to the process directory.
  /// @return Render, readback, encoding or filesystem error, or success. File writes are not
  /// atomic.
  [[nodiscard]] Result saveScreenshot(std::string_view path);
  /// Draw a frame and copy the target into contiguous RGBA8 rows, top row first.
  /// Requires a configured target, a camera and a presentable render output. Calls may wait
  /// for GPU completion; use outside latency-critical callbacks. No reference to rgba is kept.
  /// @param rgba Caller-owned output in the target's transfer encoding, valid only on success.
  /// @return Success after readback, or a scene/camera/render/readback error.
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  /// Draw and synchronously read a float attachment into packed, top-row-first pixels.
  /// Runs on the Engine/video thread and may allocate or wait for the GPU. No output
  /// reference is retained. Components per pixel follow Buffer; no depth linearization.
  /// @param which Float attachment; Colour and unknown values fail before rendering.
  /// @param out Caller-owned vector, replaced on success; consume contents only on success.
  /// @return Success or input, render, unavailable-attachment or GPU-readback error.
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);

  /// Query the compiled plan's suggested temporal warm-up frame count without rendering.
  /// @return One before plan compilation; otherwise one plus its temporal-resolve allowance.
  /// This heuristic does not establish streaming readiness or numerical convergence.
  [[nodiscard]] int settleFrames() const;

private:
  friend class Engine;

  explicit Renderer(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

/// Stable owner of simulation, rendering and streaming state; neither copyable nor movable.
/// Calls on an Engine and its borrowed facades must be serialized, including mix(). Window
/// operations run on the thread that created the SDL window. Parallel snapshot access is not
/// provided by this interface. Stop callbacks and workers using the API before destruction.
/// Initialize SDL_INIT_VIDEO before configuring a render target and retain it until destruction.
class Engine {
public:
  /// Construct an unconfigured Engine; configure a target and declare content before rendering.
  Engine();
  /// Release owned resources. All borrowed facades and references become invalid.
  /// If a window is still targeted, destroy this Engine on that window's creation thread.
  ~Engine();
  Engine(Engine &&) = delete;
  Engine &operator=(Engine &&) = delete;
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  /// Borrow a non-null SDL window as the target. Call on its creation thread with SDL video
  /// initialized. The window must outlive its use as this Engine's target; ownership stays
  /// external. Requires no open frame. On failure, the previous target and extent
  /// remain configured. A successful switch invalidates the previous pixel readback.
  /// @param presents Borrowed window whose creation thread is executing this call.
  /// @return Success or an error describing invalid input or device configuration failure.
  [[nodiscard]] Result drawsInto(SDL_Window *presents);
  /// Borrow a host for subsequent action callbacks. nullptr detaches it. The host must remain
  /// alive until replaced or this Engine is destroyed; replacement must not overlap a callback.
  /// @param host Borrowed callback receiver, or nullptr to detach.
  void offers(Host *host);
  /// Borrow a generator until Engine destruction. Registration retains its address and never
  /// takes ownership. Duplicate kind names retain the first registration.
  /// @param maker Borrowed generator whose lifetime covers its registration.
  void offers(const Generators::Generator &maker);
  /// Select an exact declared view identifier; camera application occurs during advance().
  /// @param view Borrowed identifier, not retained. No case folding or fallback lookup.
  /// @return Error for missing/unknown views, preserving the active selection; otherwise success.
  /// Serialize with Engine calls. Following/terrain errors may occur later during advance().
  [[nodiscard]] Result setView(std::string_view view);
  /// Dispatch a borrowed SDL event to declared input bindings or UI surfaces.
  /// Call on the Engine/video thread. Host callbacks run synchronously and must not
  /// reenter or destroy the Engine; input data is not retained beyond this call.
  /// @param event Event supplied by the caller; this function does not poll the SDL queue.
  /// @return True when an action fired or scrolling changed, false when unhandled,
  /// or an owned processing error. No declared bindings or active UI means unhandled.
  /// Bindings work without a render target and take precedence over UI hit testing;
  /// a refused bound action does not fall through to the UI. Unbound mouse events do.
  /// Buttons deliver 1/0, sticks [-1, 1], triggers [0, 1], mouse motion pixel deltas.
  /// Motion dispatches X before Y; key repeats are ignored. No deadzone is applied.
  /// Processing can allocate and mutate UI/input state; failures do not roll it back.
  [[nodiscard]] Holds<bool> handleEvent(const SDL_Event &event);
  /// Configure an offscreen target in physical pixels. Requires SDL_INIT_VIDEO and positive
  /// dimensions and no open frame. Borrowed facades retain their Engine identity.
  /// On failure, the previous target and extent remain configured. On success, the
  /// previous pixel readback is invalidated; subsequent pixel requests render this target.
  /// @param offscreen Required width and height in physical pixels.
  /// @return Success or an error describing invalid input or device configuration failure.
  [[nodiscard]] Result drawsInto(Extent offscreen);
  /// Store owned paths for subsequent setup; performs no filesystem validation or IO.
  /// Call before declaring/loading content and serialize with every other Engine call.
  /// Existing assets, providers and fetch services are not reopened or migrated.
  /// @param roots Configuration moved into the Engine; no references to the argument remain.
  void setRoots(Roots roots);
  /// Borrow a facade bound to this Engine; no GPU resources are allocated by this call.
  /// @return Renderer facade borrowing this Engine.
  [[nodiscard]] Renderer renderer();
  /// Borrow access to this Engine's current target; the result cannot target another Engine.
  /// @return Target facade borrowing this Engine.
  [[nodiscard]] SwapChain swapChain();
  /// Refresh published diagnostic measurements, including available GPU readbacks.
  /// May lazily create the render scene and apply pending geometry; this is not a const query
  /// or a new render. Missing readbacks are skipped rather than reported as errors.
  /// Call on the Engine/video thread, serialized with all Engine work. May allocate and wait
  /// for device work. Failure can retain partial setup; no rollback is promised.
  /// @return Success or an owned render-target/setup error.
  [[nodiscard]] Result inspect();
  /// Query whether the currently requested world tiles and derived products have caught up.
  /// Checks terrain, classifications, vectors and enabled vegetation without waiting.
  /// False when no world tiles were requested; imported geometry alone does not satisfy this
  /// predicate. A true result is transient as the camera or requested world changes, and does
  /// not imply a render target, an open frame or completion of all device work.
  /// Serialize with Engine mutations. No ownership or references are transferred.
  /// @return Current world-streaming readiness, not general Engine readiness.
  [[nodiscard]] bool settled() const;

  /// Advance streaming until the current scene is resident or the time budget expires.
  /// Runs synchronously on the Engine/video thread; may allocate, perform IO and wait.
  /// Work units may overrun the budget; this is not a hard execution-time bound.
  /// @param patienceS Finite nonnegative seconds; zero permits a readiness attempt without waiting.
  /// @return Success when ready (including a scene without ground), or an owned error.
  /// Invalid budgets fail before work. Other failures may retain partial streaming progress.
  [[nodiscard]] Result preload(double patienceS);
  /// Preload with synchronous progress notifications under the same budget/error contract.
  /// @param patienceS Finite nonnegative time budget in seconds.
  /// @param tell Optional callback, borrowed for this call. Its Loading reference is valid
  /// only during that invocation. Do not reenter the Engine, destroy it, or throw.
  /// Notifications are not guaranteed for every error or at fixed time intervals.
  /// @return Readiness or an owned input, streaming, build, capacity or timeout error.
  [[nodiscard]] Result preload(double patienceS, const std::function<void(const Loading &)> &tell);
  /// Copy current request and tile-pool counters without advancing streaming.
  /// Serialize with Engine mutations; this is not concurrent snapshot publication.
  /// @return Owned values; elapsed time and rate are zero outside preload notifications.
  [[nodiscard]] Loading loading() const;
  /// Fraction of current ground requests received, clamped to [0,1]; no requests yields 1.
  /// Excludes vector processing, mesh construction and GPU readiness. Does not advance work.
  /// Serialize with Engine mutations; this scalar is not a concurrent streaming snapshot.
  /// @return Received ground-request fraction in [0,1].
  [[nodiscard]] double loadProgress() const;

  /// Source DEM height above mean sea level, in metres; not ellipsoidal height.
  /// The input height is ignored, including nonfinite values. A datum conversion is
  /// required before geodetic placement. Longitude must be finite in [-180,180] degrees,
  /// latitude finite in [-90,90]; locations outside Mercator terrain coverage are rejected.
  /// Serialize with Engine operations. May prepare terrain tiles and allocate; not a
  /// realtime residency-only query. Missing world/data returns an owned error.
  /// Invalid coordinates fail before terrain access; failed queries may retain cache work.
  /// @param at Borrowed geographic position; only longitude and latitude are sampled.
  /// @return Source height in metres above mean sea level, or an owned query error.
  [[nodiscard]] Holds<double> sampleHeight(const LongitudeLatitudeHeight &at) const;
  /// Prepare the current declared audio scene at a positive sample rate in Hz.
  /// Call after declaring/assembling content and before starting audio output. This call
  /// allocates and validates DSP state; it needs no SDL audio device or render target.
  /// Sources bound by Sound::On require successful assembly of the current declaration.
  /// Missing, unplaced or ambiguously named bodies are rejected before changing audio state.
  /// Success resets oscillator/filter/delay state and publishes an initial source snapshot.
  /// Failure preserves the previous prepared mixer. Replacing the declaration or successfully
  /// assembling simulation invalidates it.
  /// Serialize this call with all Engine operations, including mix(); no callback may overlap.
  /// @param sampleRateHz Output frames per second, normally negotiated with the audio device.
  /// @return Success or an owned error for missing declaration, invalid data or exceeded budgets.
  [[nodiscard]] Result prepareAudio(int sampleRateHz);

  /// Fill borrowed interleaved left/right float PCM at the prepared sample rate.
  /// Requires successful prepareAudio() for the current declaration. Does not retain the span.
  /// Success overwrites all samples with unclipped linear PCM and advances DSP state; an empty
  /// span advances nothing. Valid prepared calls use bounded scratch without heap allocation.
  /// Unprepared calls and odd sample counts fail before changing samples or DSP state; error
  /// construction may allocate. Serialize with all Engine calls; no concurrent callback API.
  /// @param stereo Writable buffer containing an even number of samples (two per frame).
  /// @return Success or an owned preparation/buffer error; never performs implicit setup.
  [[nodiscard]] Result mix(std::span<float> stereo);

  /// Read a scenario file and selected layer files, then pass the owned result to declare().
  /// Relative layer paths resolve against the scenario file's directory. The path is borrowed
  /// only during this call and must contain no embedded NUL. Synchronous file IO and allocation;
  /// the scenario and selected layer files together are limited to 16 MiB of input bytes.
  /// Call on the Engine/video thread outside frames.
  /// Parsing failure preserves the active declaration but may update layer diagnostics;
  /// declaration failure has declare()'s partial-state guarantee.
  /// @param path Filesystem path to the scenario document.
  /// @return Success or an owned IO, parsing or declaration error.
  [[nodiscard]] Result readScenario(std::string_view path);

  /// Serialize the owned declaration, not the current simulated state; synchronous allocation,
  /// no file IO. Call on the Engine/video thread outside frames. Temporary table preparation
  /// validates schemas and values using the same contracts as assemble(), before writing XML.
  /// Failure leaves the declaration and simulation unchanged. Missing trailing table types
  /// become explicit text types. Section coverage and validation beyond tables remain incomplete;
  /// success is not a guarantee that every declared section can already be persisted.
  /// @return Owned XML text or an owned declaration-validation error; no partial XML on failure.
  [[nodiscard]] std::expected<std::string, std::string> writeScenario() const;
  /// Copy native geometry into engine-owned storage; the source may then be changed or destroyed.
  /// Positions are local metres with the geometry's part placements; materials use native indices.
  /// Missing normals produce flat shading in the derived render mesh; source data stays intact.
  /// Call on the engine/video thread, outside concurrent engine work. Requires well-formed data.
  /// Allocates CPU copies and may prepare GPU resources; use during scene setup, not per frame.
  /// Invalid input is rejected before replacement. GPU setup errors are returned; renderer
  /// recovery after a setup failure is not yet transactional. Allocation failure may throw.
  /// @param geometry Non-moved-from source, held immutable for the duration of this call.
  /// @return Success, or a diagnostic describing validation or setup failure.
  [[nodiscard]] Result setGeometry(const Geometry &geometry);
  /// Copy a scenario definition and configure its scene, input and generator declarations.
  /// The source is borrowed for this call; stored data is owned by the Engine. This does not
  /// replace assemble() or imply settled(). Changed declarations invalidate prepared audio.
  /// Call on the Engine/video thread outside frames and concurrent Engine work. May allocate,
  /// create device resources and wait for outstanding world jobs during replacement.
  /// View catalogs are validated before setup and published only on success, together with
  /// input bindings. Rejection preserves the previous catalog; success with no views clears it.
  /// Validation is incomplete; failures after setup begins may retain partial changes and
  /// invalidate prior scene/declaration views. There is no whole-operation rollback yet.
  /// @param scenario Definition in native scenario units and coordinate conventions.
  /// @return Success or an owned validation, generator or scene-setup error.
  [[nodiscard]] Result declare(const Scenario::Document &scenario);
  /// Copy replacement UI surfaces, ordered by increasing Z with stable ties.
  /// Empty input removes the declared surfaces. Requires an existing render scene; this does
  /// not lazily create one. Call on the Engine/video thread outside frames and concurrent work.
  /// Font preparation and redeclaration may allocate. The stored declaration is changed before
  /// renderer redeclaration, so a renderer error does not restore the previous surfaces.
  /// @param surfaces Borrowed definitions; copied text and layout data are retained.
  /// @return Success or an owned missing-scene, font or redeclaration error.
  [[nodiscard]] Result setSurfaces(const std::vector<Scenario::Surface> &surfaces);

  /// Borrow the stored declaration, initially default-initialized; no copy or allocation.
  /// Valid until Engine destruction, but content may change during declaration/loading/restore.
  /// Nested references may be invalidated by those operations, including partial failures.
  /// Serialize access with Engine mutations; copy if a stable snapshot is required.
  /// @return Borrowed current declaration, not an immutable snapshot.
  [[nodiscard]] const Scenario::Document &declaration() const;
  /// Borrow the current simulation EntityRegistry until successful assemble() or Engine
  /// destruction. Successful assembly invalidates this reference and all prior entity/component
  /// references. Failed assembly preserves the EntityRegistry and its contents; reacquire after
  /// successful assembly.
  /// @return Mutable registry borrowed from the current simulation.
  [[nodiscard]] EntityRegistry &entities();
  /// Read-only borrowed EntityRegistry, with the same lifetime and mutation restrictions as
  /// entities().
  /// @return Read-only registry borrowed from the current simulation.
  [[nodiscard]] const EntityRegistry &entities() const;
  /// Borrow diagnostic names of declaration sections carried without implementation.
  /// Additional generation diagnostics may be appended. This is not a capability registry.
  /// The vector belongs to the Engine until destruction; mutations may replace its contents
  /// and invalidate element references. Serialize access with all Engine mutations.
  /// @return Borrowed diagnostic names; copy to retain across mutations.
  [[nodiscard]] const std::vector<std::string> &unacted() const;
  /// Borrow declared and published diagnostics; each Measure supplies its own unit.
  /// Values may come from different updates and persist when not refreshed; not a frame snapshot.
  /// The vector lives until Engine destruction. Publication/declaration may change values or
  /// invalidate element references. Serialize access with Engine mutations; copying allocates.
  /// @return Borrowed measurements with per-entry units and update histories.
  [[nodiscard]] const std::vector<Measure> &measures() const;

  /// Allocate two CPU timing rings with up to steps entries each, discarding saved samples.
  /// Zero disables retention; aggregate counters remain. Call during setup and serialize with
  /// Engine operations. Allocation failure may throw; replacing both rings is not transactional.
  /// @param steps Maximum retained samples per timing ring; zero disables retention.
  void keepSamples(size_t steps);
  /// Replace out with retained advance() CPU wall times in milliseconds, oldest first.
  /// Only calls reaching the timing recorder contribute; early failures produce no sample.
  /// Includes synchronous streaming/draw preparation, not just physics; elapsed-time advance
  /// contributes one sample per executed step. Copy may allocate; serialize with Engine calls.
  /// @param out Caller-owned vector overwritten with retained CPU step durations.
  void stepTimesMs(std::vector<double> &out) const;
  /// Replace out with retained successful render-call CPU wall times in milliseconds,
  /// oldest first. Measures Draw execution, not GPU completion or presentation latency.
  /// Readbacks do not contribute. Copy may allocate; serialize with Engine operations.
  /// @param out Caller-owned vector overwritten with retained CPU render durations.
  void frameTimesMs(std::vector<double> &out) const;

  /// Build entity, component, table, trigger and physics state for the current declaration.
  /// Total capacity (reserve, bodies, kinds, instances and player mind) is limited to 65536
  /// entity slots. Allocates on the calling thread; serialize with all Engine operations. No render
  /// target is required. With a target and no entities, world composition may also run.
  /// Success replaces simulation state and invalidates borrowed EntityRegistry references and
  /// prepared audio. Failure preserves previous simulation state; this does not roll back
  /// declare().
  /// @return Success or an owned validation/build error. Fatal allocation failure is separate.
  [[nodiscard]] Result assemble();

  /// Execute one configured fixed simulation step and update streaming and render-scene state.
  /// Does not sample wall-clock time, consume the elapsed-time accumulator, poll SDL events or
  /// present a frame. Use the explicit rendering API for drawing/presentation.
  /// Call on the Engine/video thread, serialized with Engine work. May allocate and perform
  /// streaming/device setup; failures may retain an advanced clock or partial scene changes.
  /// @return Success or an owned update/render-scene error; no rollback on failure.
  [[nodiscard]] Result advance();
  /// Add elapsed seconds and execute due fixed steps up to the declared catch-up limit.
  /// Call on the Engine/video thread; steps may allocate and perform streaming/render setup.
  /// Invalid input or accumulator overflow fails before mutation. A failed step may retain
  /// partial state and consumes its queued time. Excess backlog may be discarded.
  /// @param elapsedS Finite nonnegative seconds; zero may still process existing backlog.
  /// @return Success or an owned time-validation/step error. No due steps is successful.
  [[nodiscard]] Result advance(double elapsedS);
  /// Return the declared fixed simulation step in seconds without advancing the clock.
  /// This is configuration, not measured frame duration. Serialize with declaration changes.
  /// @return Configured simulation step duration in seconds.
  [[nodiscard]] double stepSeconds() const;
  /// Repeatedly call advance() until it fails; requires an existing render scene.
  /// This synchronous loop has no pacing, event polling or separate cancellation argument.
  /// It may run indefinitely. Use advance() under the host's loop for scheduled execution.
  /// Call on the Engine/video thread with no concurrent Engine work. State and error guarantees
  /// are those of advance(); this operation does not provide a simulation snapshot.
  /// @return Missing-scene/update error, or success if the loop ends with no diagnostic.
  [[nodiscard]] Result run();

  /// Copy the active named declaration into the bounded parked set and release its render scene.
  /// Requires an existing render scene and a unique nonempty name. This stores a declaration,
  /// not a simulation snapshot; simulation and all streaming resources are not fully suspended.
  /// Clears generated world pieces and may wait for their jobs. Call on the Engine/video thread
  /// outside frames, serialized with Engine work. Released render resources invalidate views.
  /// @return Success or an owned missing-scene/name, duplicate-name or capacity error.
  [[nodiscard]] Result park();
  /// Declare a parked definition and remove its parked entry only after declaration succeeds.
  /// Requires no existing render scene. Does not restore simulated time or dynamic state from
  /// parking. Failure retains the parked entry but may partially change the Engine via declare().
  /// Call on the Engine/video thread outside frames; setup costs and borrowing invalidation
  /// follow declare(). Serialize with all Engine work.
  /// @param name Exact parked name, borrowed only for this call.
  /// @return Success or an owned active-scene, unknown-name or declaration error.
  [[nodiscard]] Result resume(std::string_view name);
  /// Remove a parked declaration without changing the active scene or performing disk IO.
  /// Serialize with Engine work. Erasing releases owned declaration storage and may move entries.
  /// @param name Exact parked name, borrowed only for this call.
  /// @return Success, or an owned unknown-name error with the parked set unchanged.
  [[nodiscard]] Result discard(std::string_view name);
  /// Write declared instance.trait persistence rows for the assembled simulation.
  /// Stores selected numeric traits and scenario name/version, not a complete world snapshot.
  /// Synchronous, allocating IO; requires no embedded NUL in the path and serialized Engine
  /// access. Validation and the 1 MiB output-size check precede opening the destination.
  /// Publishes by replacing the destination directory entry only after a temporary sibling
  /// file was fully written and closed. IO errors preserve the previous destination. Concurrent
  /// successful writers publish complete files; the last replacement wins. No crash-durability
  /// or preservation of the replaced file's metadata is promised.
  /// @param path Borrowed output path; no reference is retained.
  /// @return Success or an owned missing-state/trait, size or IO error; simulation is unchanged.
  [[nodiscard]] Result save(std::string_view path) const;
  /// Apply saved numeric traits to an already assembled matching scenario name/version.
  /// Reads synchronously into owned storage, limited to 1 MiB of input bytes. Parses
  /// finite values and validates staged trait rows before applying them. Parse/validation
  /// errors preserve traits. All target components are checked before the first replacement;
  /// publication performs no allocation and any reported failure preserves prior trait values.
  /// Does not load assets, assemble a scenario or restore a complete world snapshot.
  /// Serialize with all Engine work; borrowed simulation views may observe replaced traits.
  /// @param path Borrowed input path without embedded NUL; not retained after this call.
  /// @return Success or an owned IO, identity, parsing or trait-publication error.
  [[nodiscard]] Result restore(std::string_view path);
  /// Copy parked declaration names in insertion order; no filesystem access.
  /// Serialize with Engine mutations. Allocates an independent snapshot; later park/resume/
  /// discard operations do not invalidate the returned strings.
  /// @return Owned names of currently parked declarations, possibly empty.
  [[nodiscard]] std::vector<std::string> parked() const;

  /// Replace the process-wide borrowed diagnostic sink, or detach it with nullptr.
  /// @param sink Sink that outlives registration and outstanding callbacks. Change registration
  /// only while all log producers are quiescent; callbacks may run on multiple emitting threads.
  static void logsTo(LogSink *sink);

  /// Report whether an internal render scene exists; does not imply streaming readiness,
  /// successful simulation assembly or an open frame. Serialize with Engine mutations.
  /// @return True when an internal render scene exists.
  [[nodiscard]] bool standing() const;
  /// Borrow the legacy diagnostic string until Engine destruction; later calls may change it
  /// and invalidate character pointers. Serialize with Engine operations. Some expected-returning
  /// calls do not update or clear it: use their returned error as the authoritative result.
  /// @return Borrowed legacy diagnostic, possibly empty or stale.
  [[nodiscard]] const std::string &error() const;

private:
  friend class Renderer;
  [[nodiscard]] bool render(Extent frame);
  [[nodiscard]] bool saveScreenshot(std::string_view path);
  [[nodiscard]] bool readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] bool readPixels(Buffer which, std::vector<float> &out);
  [[nodiscard]] bool beginFrame();
  [[nodiscard]] bool endFrame();
  [[nodiscard]] bool flushAndWait();
  [[nodiscard]] Extent canvas() const;

  [[nodiscard]] bool camera(Camera &out) const;
  [[nodiscard]] bool presenting() const;

  friend class SwapChain;
  struct State;
  [[nodiscard]] bool readScenarioInto(std::string_view path, Scenario::Document &out);
  [[nodiscard]] bool generated(const Scenario::Document &scenario);
  void ships();
  std::unique_ptr<State> S_;
};

}

#endif
