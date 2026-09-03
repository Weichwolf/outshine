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

class SwapChain {
public:
  [[nodiscard]] Extent extent() const;
  [[nodiscard]] bool presents() const;

  void logsTo(LogSink *sink);

private:
  friend class Engine;

  explicit SwapChain(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

class Renderer {
public:
  [[nodiscard]] Result beginFrame(SwapChain &into);
  [[nodiscard]] Result endFrame();

  [[nodiscard]] Result flushAndWait();

  [[nodiscard]] Result render(Extent frame);
  [[nodiscard]] Result saveScreenshot(std::string_view path);
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);

  [[nodiscard]] int settleFrames() const;

private:
  friend class Engine;

  explicit Renderer(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

class Engine {
public:
  Engine();
  ~Engine();
  Engine(Engine &&) noexcept;
  Engine &operator=(Engine &&) noexcept;
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  [[nodiscard]] Result drawsInto(SDL_Window *presents);
  void offers(Host *host);
  void offers(const Generators::Generator &maker);
  [[nodiscard]] Result setView(std::string_view view);
  [[nodiscard]] Result handleEvent(const SDL_Event &event);
  [[nodiscard]] Result drawsInto(Extent offscreen);
  void setRoots(Roots roots);
  [[nodiscard]] Renderer renderer();
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

  [[nodiscard]] Holds<double> sampleHeight(const LongitudeLatitudeHeight &at) const;
  [[nodiscard]] Result mix(std::span<float> stereo, int rate);

  [[nodiscard]] Result readScenario(std::string_view path);

  /// The declaration this engine stands on, written back in the spelling `readScenario` accepts.
  ///
  /// A format that is only ever read cannot be diffed against what the engine HOLDS, and that
  /// asymmetry is how a grammar and its reader drift. With this, `read -> write -> read` is a
  /// counter-control a client can run: the two texts are the same one, or a section is missing a
  /// spelling.
  [[nodiscard]] std::string writeScenario() const;
  [[nodiscard]] Result setGeometry(const Geometry &geometry);
  [[nodiscard]] Result declare(const Scenario::Document &scenario);
  [[nodiscard]] Result setSurfaces(const std::vector<Scenario::Surface> &surfaces);

  [[nodiscard]] const Scenario::Document &declaration() const;
  [[nodiscard]] Scene &scene();
  [[nodiscard]] const Scene &scene() const;
  [[nodiscard]] const std::vector<std::string> &unacted() const;
  [[nodiscard]] const std::vector<Measure> &measures() const;

  void keepSamples(size_t steps);
  void stepTimesMs(std::vector<double> &out) const;
  void frameTimesMs(std::vector<double> &out) const;

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

  [[nodiscard]] bool camera(Scenario::Camera &out) const;
  [[nodiscard]] bool presenting() const;

  friend class SwapChain;
  struct State;
  [[nodiscard]] bool readScenarioInto(std::string_view path, Scenario::Document &out);
  [[nodiscard]] bool generated(const Scenario::Document &scenario);
  void ships();
  std::unique_ptr<State> S_;
};

} // namespace outshine

#endif
