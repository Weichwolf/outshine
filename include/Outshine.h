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
#include "generate/Generate.h"
#include "scenario/Event.h"
#include "scenario/Scenario.h"
#include "scene/Geometry.h"
#include "scene/Scene.h"

namespace outshine {

/// What a door verb gives back: the value it was asked for, or the reason it could not be had.
///
/// A refusal that carries only `false` makes the client invent the reason, and the reasons here are
/// not interchangeable -- a tile that has not arrived and a place outside the declared world are
/// different answers to the same question.
template <typename Value> using Holds = std::expected<Value, std::string>;

/// A verb with nothing to give back but its refusal.
using Result = Holds<void>;

struct Loading {
  size_t GroundWanted = 0, GroundArrived = 0;
  size_t VectorWanted = 0, VectorArrived = 0;
  size_t Outstanding = 0;
  double FetchedMB = 0.0;
  double Megabits = 0.0;
  double MeanFetchMs = 0.0;
  double ElapsedS = 0.0;

  [[nodiscard]] double share() const {
    const size_t wants = GroundWanted + VectorWanted;
    return wants == 0
               ? 1.0
               : static_cast<double>(GroundArrived + VectorArrived) / static_cast<double>(wants);
  }
};

struct Roots {
  std::string Assets;
  std::string Shipped;
  std::string Cache;
  bool Offline = false;
};

class Engine;

enum class Buffer { Colour, Linear, Depth, ShadingNormal, SurfaceIdentity, Velocity };

/// Borrowed access to an Engine's current presentation target, not an independently owned window.
/// Copies refer to the same Engine and observe subsequent target changes. The Engine and its
/// borrowed SDL window must outlive all uses. Calls follow Engine's thread restrictions.
class SwapChain {
public:
  /// Current drawable dimensions in physical pixels; zero before target configuration.
  [[nodiscard]] Extent extent() const;
  /// Whether the current target presents to an SDL window rather than an offscreen buffer.
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
  /// either Engine. The target must have positive dimensions; rendering setup may fail.
  [[nodiscard]] Result beginFrame(SwapChain &into);
  /// Close a frame opened by beginFrame() on the same Engine, including facade copies.
  /// Window targets draw/present the current scene; offscreen targets only close the scope.
  /// An unopened frame returns an owned error. The scope is closed even if presentation fails.
  /// Runs on the Engine's video thread and may wait for GPU/presentation resources.
  [[nodiscard]] Result endFrame();

  /// Block the Engine's video thread until submitted GPU work has completed.
  /// No scene is a successful no-op. Does not open, close, draw or present a frame.
  /// Serialize with other Engine calls; use outside latency-critical callbacks.
  /// @return Success or an owned device-state/GPU-wait error, including the SDL diagnosis.
  [[nodiscard]] Result flushAndWait();

  /// Request a frame from this Engine's current scene. A zero extent uses the configured
  /// target; a positive extent must match it. Calls are serialized with other Engine work.
  /// A camera must be bound or derivable from object bounds. advance() applies a declared
  /// scenario view; an empty scene without a prepared view returns an error.
  /// @param frame Optional check of the target size in physical pixels; zero selects the target.
  /// @return An error if scene/camera preparation fails; submission does not imply GPU completion.
  [[nodiscard]] Result render(Extent frame);
  [[nodiscard]] Result saveScreenshot(std::string_view path);
  /// Draw a frame and copy the target into contiguous RGBA8 rows, top row first.
  /// Requires a configured target, a camera and a presentable render output. Calls may wait
  /// for GPU completion; use outside latency-critical callbacks. No reference to rgba is kept.
  /// @param rgba Caller-owned output in the target's transfer encoding, valid only on success.
  /// @return Success after readback, or a scene/camera/render/readback error.
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);

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
  void offers(Host *host);
  /// Borrow a generator until Engine destruction. Registration retains its address and never
  /// takes ownership. Duplicate kind names retain the first registration.
  void offers(const Generators::Generator &maker);
  [[nodiscard]] Result setView(std::string_view view);
  [[nodiscard]] Result handleEvent(const SDL_Event &event);
  /// Configure an offscreen target in physical pixels. Requires SDL_INIT_VIDEO and positive
  /// dimensions and no open frame. Borrowed facades retain their Engine identity.
  /// On failure, the previous target and extent remain configured. On success, the
  /// previous pixel readback is invalidated; subsequent pixel requests render this target.
  /// @param offscreen Required width and height in physical pixels.
  /// @return Success or an error describing invalid input or device configuration failure.
  [[nodiscard]] Result drawsInto(Extent offscreen);
  void setRoots(Roots roots);
  /// Borrow a facade bound to this Engine; no GPU resources are allocated by this call.
  [[nodiscard]] Renderer renderer();
  /// Borrow access to this Engine's current target; the result cannot target another Engine.
  [[nodiscard]] SwapChain swapChain();
  [[nodiscard]] Result inspect();
  [[nodiscard]] bool settled() const;

  /// What @ref Engine::bench measured: the preload, then one row per frame.
  ///
  /// The three per-frame series are the whole point. `AdvanceMs` is what the simulation took and
  /// `StreamedMs` is how much of that was WAITING FOR DATA -- their difference is the simulation's
  /// own work, and it is the only one of the three a renderer change can move. Measured here: a
  /// place reported a worst advance of 15 177 ms, none of which was simulation.
  struct Benched {
    /// What the blocking phase cost, and what it fetched. Zero when the caller preloaded first.
    double PreloadMs = 0.0;
    size_t PreloadTiles = 0;

    /// One entry per frame that ran, in the order they ran.
    std::vector<double> AdvanceMs;
    std::vector<double> RenderMs;
    std::vector<double> StreamedMs;

    /// How many frames waited for data at all, and how many tiles they pulled in doing it.
    ///
    /// **This is the number that says whether the preload was WHOLE.** Zero of them means the
    /// walk asked for nothing the preload had not already fetched -- which is the claim a refusal
    /// would have made, except this one carries how far off it was rather than only that it was.
    size_t FramesThatStreamed = 0;
    size_t TilesInFrames = 0;

    [[nodiscard]] size_t frames() const { return AdvanceMs.size(); }
  };

  /// Runs @p frames frames from where the engine stands, timing each one.
  ///
  /// The engine is NOT required to be settled first. Whether to preload and then bench, or to
  /// bench the preload itself with `frames == 0`, is the caller's measurement to choose -- so this
  /// reports what streaming cost rather than refusing to run beside it. `before` is called with
  /// each frame's index before it is timed, for a caller that moves the camera; passing nothing
  /// leaves the camera where it stands.
  [[nodiscard]] Result bench(int frames, Benched &into);
  [[nodiscard]] Result bench(int frames, Benched &into, const std::function<bool(int)> &before);

  [[nodiscard]] Result preload(double patienceS);
  [[nodiscard]] Result preload(double patienceS, const std::function<void(const Loading &)> &tell);
  [[nodiscard]] Loading loading() const;
  [[nodiscard]] double loadProgress() const;

  /// Source DEM height above mean sea level, in metres; not ellipsoidal height.
  /// The input height is ignored. A datum conversion is required before geodetic placement.
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

  [[nodiscard]] Result readScenario(std::string_view path);

  /// The declaration this engine stands on, written back in the spelling `readScenario` accepts.
  ///
  /// A format that is only ever read cannot be diffed against what the engine HOLDS, and that
  /// asymmetry is how a grammar and its reader drift. With this, `read -> write -> read` is a
  /// counter-control a client can run: the two texts are the same one, or a section is missing a
  /// spelling.
  [[nodiscard]] std::string writeScenario() const;
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
  [[nodiscard]] Result declare(const Scenario::Document &scenario);
  [[nodiscard]] Result setSurfaces(const std::vector<Scenario::Surface> &surfaces);

  [[nodiscard]] const Scenario::Document &declaration() const;
  /// Borrow the current simulation Scene until successful assemble() or Engine destruction.
  /// Successful assembly invalidates this reference and all prior entity/component references.
  /// Failed assembly preserves the Scene and its contents; reacquire after successful assembly.
  [[nodiscard]] Scene &scene();
  /// Read-only borrowed Scene, with the same lifetime and mutation restrictions as scene().
  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const std::vector<std::string> &unacted() const;
  [[nodiscard]] const std::vector<Measure> &measures() const;

  void keepSamples(size_t steps);
  void stepTimesMs(std::vector<double> &out) const;
  void frameTimesMs(std::vector<double> &out) const;

  /// Build entity, component, table, trigger and physics state for the current declaration.
  /// Total capacity (reserve, bodies, kinds, instances and player mind) is limited to 65536
  /// entity slots. Allocates on the calling thread; serialize with all Engine operations. No render
  /// target is required. With a target and no entities, world composition may also run.
  /// Success replaces simulation state and invalidates borrowed Scene references and prepared
  /// audio. Failure preserves previous simulation state; this does not roll back declare().
  /// @return Success or an owned validation/build error. Fatal allocation failure is separate.
  [[nodiscard]] Result assemble();

  [[nodiscard]] Result advance();
  [[nodiscard]] Result advance(double elapsedS);
  [[nodiscard]] double stepSeconds() const;
  [[nodiscard]] Result run();

  [[nodiscard]] Result park();
  [[nodiscard]] Result resume(std::string_view name);
  [[nodiscard]] Result discard(std::string_view name);
  [[nodiscard]] Result save(std::string_view path) const;
  [[nodiscard]] Result restore(std::string_view path);
  [[nodiscard]] std::vector<std::string> parked() const;

  /// Replace the process-wide borrowed diagnostic sink, or detach it with nullptr.
  /// @param sink Sink that outlives registration and outstanding callbacks. Change registration
  /// only while all log producers are quiescent; callbacks may run on multiple emitting threads.
  static void logsTo(LogSink *sink);

  [[nodiscard]] bool standing() const;
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
