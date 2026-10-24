#ifndef OUTSHINE_ENGINE_ENGINE_H
#define OUTSHINE_ENGINE_ENGINE_H

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
#include "FrameBuffer.h"
#include "Logging.h"
#include "Generator.h"
#include "DiagnosticSample.h"
#include "Event.h"
#include "Scenario.h"
#include "Geometry.h"
#include "EntityRegistry.h"
#include "Route.h"
#include "Provider.h"

namespace outshine {

namespace Render {
struct GpuFrameSample;
}

template <typename Value> using Holds = std::expected<Value, std::string>;

using Result = Holds<void>;

enum class WorldQuality : uint8_t { Playable, Refined };

// Fetch volumes are MiB; Megabits is Mibit/s during preload.
struct Loading {
  size_t GroundWanted = 0;
  size_t GroundArrived = 0;
  size_t VectorWanted = 0;
  size_t VectorArrived = 0;
  size_t Outstanding = 0;
  double FetchedMB = 0.0;
  double Megabits = 0.0;
  double MeanFetchMs = 0.0;
  double ElapsedS = 0.0;
  double PreloadMs = 0.0;
  double PreloadPumpMs = 0.0;
  double PreloadFlushMs = 0.0;
  double PreloadAwaitMs = 0.0;
  size_t PreloadPumps = 0;
  size_t PreloadFlushes = 0;
  size_t PreloadAwaits = 0;

  struct WaitBreakdown {
    double StructureMs = 0.0;
    double ClassMs = 0.0;
    double TileMs = 0.0;
    double WorldWorkerMs = 0.0;
    double IdleMs = 0.0;
    size_t StructureCalls = 0;
    size_t ClassCalls = 0;
    size_t TileCalls = 0;
    size_t WorldWorkerCalls = 0;
    size_t IdleCalls = 0;
    size_t StructureSignals = 0;
    size_t ClassSignals = 0;
    size_t TileSignals = 0;
    size_t WorldWorkerSignals = 0;
    double TileNoOutstandingMs = 0.0;
    size_t TileNoOutstandingCalls = 0;
  } Waited;

  std::uint64_t StoreHits = 0;
  std::uint64_t StoreMisses = 0;
  std::uint64_t StoreWrites = 0;
  std::uint64_t ProviderStarts = 0;
  std::uint64_t RemoteStarts = 0;
  std::uint64_t ProviderRetries = 0;
  std::uint64_t SourceDeliveries = 0;
  std::uint64_t SourceFromStore = 0;
  std::uint64_t SourceBytes = 0;

  struct UsedSource {
    std::string Kind;
    std::string Id;
    std::string Revision;
    std::string Key;
    int Priority = 0;
    std::uint64_t TileDeliveries = 0;
  };

  std::vector<UsedSource> Sources;

  [[nodiscard]] constexpr double share() const noexcept {
    const double wants = static_cast<double>(GroundWanted) + static_cast<double>(VectorWanted);
    return wants == 0.0
               ? 1.0
               : (static_cast<double>(GroundArrived) + static_cast<double>(VectorArrived)) / wants;
  }
};

struct Roots {
  std::string Assets;
  std::string Shipped;
  std::string Cache;
  bool Offline = false;
  std::string AssetCache;
};

struct WeatherSnapshot;
class Engine;

class Capture {
public:
  ~Capture();

  Capture(Capture &&other) noexcept;

  Capture &operator=(Capture &&other) noexcept;

  void release() noexcept;
  Capture(const Capture &) = delete;
  Capture &operator=(const Capture &) = delete;

private:
  friend class Engine;

  explicit Capture(Engine &engine) noexcept : Engine_(&engine) {}

  Engine *Engine_ = nullptr;
};

class SwapChain {
public:
  [[nodiscard]] Extent extent() const;

  [[nodiscard]] bool presents() const;

private:
  friend class Engine;
  friend class Renderer;

  explicit SwapChain(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

class Renderer {
public:
  [[nodiscard]] Result beginFrame(SwapChain &into);

  [[nodiscard]] Result endFrame();

  [[nodiscard]] Result flushAndWait();

  [[nodiscard]] Result render(Extent frame);

  [[nodiscard]] Result resetMeasurements();

  [[nodiscard]] Result resetMeasurements(size_t gpuSamples);

  [[nodiscard]] Result gpuSamples(std::vector<Render::GpuFrameSample> &out) const;

  [[nodiscard]] bool camera(Camera &out) const;

  [[nodiscard]] std::string_view gpuName() const;

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
  Engine(Engine &&) = delete;
  Engine &operator=(Engine &&) = delete;
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  [[nodiscard]] Result setRenderTarget(SDL_Window *window);

  void setInputHost(Host *host);

  [[nodiscard]] Result registerGenerator(const Generators::Generator &generator);

  [[nodiscard]] Result registerProvider(const Data::Provider &provider);

  [[nodiscard]] Result setView(std::string_view view);

  [[nodiscard]] Result setCameraPose(const Camera &pose);

  [[nodiscard]] Holds<bool> handleEvent(const SDL_Event &event);

  [[nodiscard]] Result setRenderTarget(Extent offscreen);

  [[nodiscard]] Result setRoots(Roots roots);

  [[nodiscard]] Renderer renderer();

  [[nodiscard]] SwapChain swapChain();

  [[nodiscard]] Result inspect();

  [[nodiscard]] bool settled(WorldQuality required = WorldQuality::Playable) const;

  [[nodiscard]] Holds<std::shared_ptr<const WeatherSnapshot>> weather() const;

  [[nodiscard]] std::string unsettledReasons(WorldQuality required) const;

  [[nodiscard]] Holds<Capture> beginCapture();

  [[nodiscard]] Result preload(double patienceS);

  [[nodiscard]] Result preload(double patienceS, WorldQuality required);

  [[nodiscard]] Result preload(double patienceS, const std::function<void(const Loading &)> &tell);

  [[nodiscard]] Result prepareViewData(double durationS, double patienceS);

  [[nodiscard]] Loading loading() const;

  [[nodiscard]] double loadProgress() const;

  [[nodiscard]] Holds<double> sampleHeight(const LongitudeLatitudeHeight &at) const;

  [[nodiscard]] Holds<RouteInfo> routeInfo(std::string_view name) const;

  [[nodiscard]] Holds<RoutePose> sampleRoute(std::string_view name, double stationM) const;

  [[nodiscard]] Holds<RouteContact>
  sampleRouteContact(std::string_view name, double stationM, double lateralOffsetM) const;

  [[nodiscard]] Result prepareAudio(int sampleRateHz);

  [[nodiscard]] Result mix(std::span<float> stereo);

  [[nodiscard]] Result readScenario(std::string_view source, std::string_view overrides = {});

  [[nodiscard]] std::expected<std::string, std::string> writeScenario() const;

  [[nodiscard]] Result setGeometry(const Geometry &geometry);

  [[nodiscard]] Result declare(const Scenario::Document &scenario);

  [[nodiscard]] Result setSurfaces(std::span<const Scenario::Surface> surfaces);

  [[nodiscard]] const Scenario::Document &declaration() const;

  [[nodiscard]] EntityRegistry &entities();

  [[nodiscard]] const EntityRegistry &entities() const;

  [[nodiscard]] std::span<const std::string> unacted() const;

  [[nodiscard]] std::span<const DiagnosticSample> measures() const;

  void keepSamples(size_t steps);

  void stepTimesMs(std::vector<double> &out) const;

  void frameTimesMs(std::vector<double> &out) const;

  [[nodiscard]] Result assemble();

  [[nodiscard]] Result advance();

  [[nodiscard]] Result advance(double elapsedS);

  [[nodiscard]] double stepSeconds() const;

  [[nodiscard]] Result save(std::string_view path) const;

  [[nodiscard]] Result restore(std::string_view path);

  void logsTo(LogSink *sink);

  [[nodiscard]] bool standing() const;

private:
  friend class Capture;
  friend class Renderer;
  [[nodiscard]] Result advanceFrame(bool simulate);
  [[nodiscard]] Result preloadWithQuality(double patienceS,
                                          WorldQuality required,
                                          const std::function<void(const Loading &)> &tell);
  [[nodiscard]] Result render(Extent frame);
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);
  [[nodiscard]] Result beginFrame();
  [[nodiscard]] Result endFrame();
  [[nodiscard]] Result flushAndWait();
  [[nodiscard]] Extent canvas() const;

  [[nodiscard]] bool camera(Camera &out) const;
  [[nodiscard]] bool presenting() const;

  friend class SwapChain;
  friend class TestEngine;
  struct State;
  explicit Engine(std::unique_ptr<State> state);
  [[nodiscard]] bool
  readScenarioInto(std::string_view source, Scenario::Document &out, std::string_view overrides);
  void ships();
  std::unique_ptr<State> S_;
};

}

#endif
