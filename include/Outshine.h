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

#include "Logging.h"
#include "generate/Generate.h"
#include "scenario/Event.h"
#include "scenario/Scenario.h"
#include "scene/Geometry.h"
#include "scene/Scene.h"

namespace outshine {

using Result = std::expected<void, std::string>;

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
  [[nodiscard]] Result preload(double patienceS);
  [[nodiscard]] Result preload(double patienceS, const std::function<void(const Loading &)> &tell);
  [[nodiscard]] Loading loading() const;
  [[nodiscard]] double loadProgress() const;

  [[nodiscard]] bool sampleHeight(double latitudeDeg, double longitudeDeg, double &heightM) const;
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

  void logsTo(LogSink *sink);

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
