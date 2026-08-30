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

#include "Generate.h"
#include "Geometry.h"
#include "Logging.h"

#include <Event.h>
#include <Scene.h>
#include <Scenario.h>

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

  [[nodiscard]] double share(void) const {
    const size_t wants = GroundWanted + VectorWanted;
    return wants == 0 ? 1.0 : (double)(GroundArrived + VectorArrived) / (double)wants;
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
  [[nodiscard]] Extent extent(void) const;
  [[nodiscard]] bool presents(void) const;

  void logsTo(LogSink *sink);

private:
  friend class Engine;

  explicit SwapChain(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

class Renderer {
public:
  [[nodiscard]] Result beginFrame(SwapChain &into);
  [[nodiscard]] Result endFrame(void);

  [[nodiscard]] Result flushAndWait(void);

  [[nodiscard]] Result render(Extent frame);
  [[nodiscard]] Result saveScreenshot(std::string_view path);
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);

  [[nodiscard]] int settleFrames(void) const;

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
  void offers(const Generates &maker);
  [[nodiscard]] Result setView(std::string_view view);
  [[nodiscard]] Result handleEvent(const SDL_Event &event);
  [[nodiscard]] Result drawsInto(Extent offscreen);
  void setRoots(Roots roots);
  [[nodiscard]] Renderer renderer(void);
  [[nodiscard]] SwapChain swapChain(void);
  [[nodiscard]] Result inspect(void);
  [[nodiscard]] bool settled(void) const;
  [[nodiscard]] Result preload(double patienceS);
  [[nodiscard]] Result preload(double patienceS, const std::function<void(const Loading &)> &tell);
  [[nodiscard]] Loading loading(void) const;
  [[nodiscard]] double loadProgress(void) const;

  [[nodiscard]] bool sampleHeight(double latitudeDeg, double longitudeDeg, double &heightM) const;
  [[nodiscard]] Result mix(std::span<float> stereo, int rate);

  [[nodiscard]] Result readScenario(std::string_view path);
  [[nodiscard]] Result setGeometry(const Geometry &geometry);
  [[nodiscard]] Result declare(const Scenario &scenario);
  [[nodiscard]] Result setSurfaces(const std::vector<Surface> &surfaces);

  [[nodiscard]] const Scenario &declaration(void) const;
  [[nodiscard]] Scene &scene(void);
  [[nodiscard]] const Scene &scene(void) const;
  [[nodiscard]] const std::vector<std::string> &unacted(void) const;
  [[nodiscard]] const std::vector<Measure> &measures(void) const;

  void keepSamples(size_t steps);
  void stepTimesMs(std::vector<double> &out) const;
  void frameTimesMs(std::vector<double> &out) const;

  [[nodiscard]] Result assemble();

  [[nodiscard]] Result advance();
  [[nodiscard]] Result advance(double elapsedS);
  [[nodiscard]] double stepSeconds(void) const;
  [[nodiscard]] Result run();

  [[nodiscard]] Result park();
  [[nodiscard]] Result resume(std::string_view name);
  [[nodiscard]] Result discard(std::string_view name);
  [[nodiscard]] Result save(std::string_view path) const;
  [[nodiscard]] Result restore(std::string_view path);
  [[nodiscard]] std::vector<std::string> parked(void) const;

  void logsTo(LogSink *sink);

  [[nodiscard]] bool standing(void) const;
  [[nodiscard]] const std::string &error(void) const;

private:
  friend class Renderer;
  [[nodiscard]] bool render(Extent frame);
  [[nodiscard]] bool saveScreenshot(std::string_view path);
  [[nodiscard]] bool readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] bool readPixels(Buffer which, std::vector<float> &out);
  [[nodiscard]] bool beginFrame(void);
  [[nodiscard]] bool endFrame(void);
  [[nodiscard]] bool flushAndWait(void);
  [[nodiscard]] Extent canvas(void) const;

  [[nodiscard]] bool camera(Camera &out) const;
  [[nodiscard]] bool presenting(void) const;

  friend class SwapChain;
  struct State;
  [[nodiscard]] bool readScenarioInto(std::string_view path, Scenario &out);
  [[nodiscard]] bool generated(const Scenario &scenario);
  void ships(void);
  std::unique_ptr<State> S_;
};

} // namespace outshine

#endif
