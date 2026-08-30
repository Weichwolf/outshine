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

// WHAT A WAIT LOOKS LIKE WHILE IT IS STILL WAITING. Cesium answers `ComputeLoadProgress()` and
// Unreal `GetAsyncLoadPercentage`, both a single share, and a single share cannot say WHY a load is
// slow. These are the quantities that can: how much of each kind arrived, what the wire actually
// delivered, and how long a fetch took on average. A client that blocks on `preload` cannot poll
// for them, so `preload` hands them over on every wake.
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

// WHICH PICTURE A FRAME IS READ FROM. `Colour` is the displayed one and comes back as bytes;
// every other name is scene-referred and comes back as float, because a linear value quantised to
// 8 bits is no longer the value that was computed.
//
// NEITHER ENGINE ANSWERS THIS ONE THE SAME WAY, so the choice is stated here. Filament reads a
// render target through one verb with a pixel FORMAT and offers no second picture; Unreal offers
// many under "buffer visualisation" and no single read. Taking Filament's ONE VERB with Unreal's
// NAMED PICTURES: a client asks the same question and says which picture it is about. The list is
// what a conformance harness has to see to state a claim about a frame -- a linear delta against
// an oracle, a depth probe, a shading normal -- and it is not a debug menu.
enum class Buffer { Colour, Linear, Depth, ShadingNormal, SurfaceIdentity, Velocity };

// FILAMENT'S SWAPCHAIN IS THE SURFACE A RENDERER DRAWS INTO, and a client names it because a
// frame is bracketed against one. Here `drawsInto` already makes it -- a window or an offscreen
// canvas -- and this is the handle that says WHICH, so `beginFrame` has something to be about.
// It carries no resources of its own: the engine owns the target and this is a name for it, the
// same shape `TransformManager` already has over a `Geometry`.
class SwapChain {
public:
  [[nodiscard]] Extent extent(void) const;
  [[nodiscard]] bool presents(void) const;

  // WHERE THE ENGINE'S COMMENTARY GOES. Handing in nothing turns it off, which is the default.
  void logsTo(LogSink *sink);

private:
  friend class Engine;

  explicit SwapChain(Engine &of) : Of_(&of) {}

  Engine *Of_ = nullptr;
};

class Renderer {
public:
  // A FRAME IS BRACKETED SO THAT MORE THAN ONE VIEW CAN STAND IN IT. Filament's `beginFrame`
  // answers FALSE when the frame should be dropped -- the device is not ready, or the pacer says
  // skip -- and a client that ignores that draws into nothing. `render` alone is the one-view
  // shorthand and brackets itself; between a `beginFrame` and an `endFrame` it draws without
  // presenting, which is what lets a client compose several.
  [[nodiscard]] Result beginFrame(SwapChain &into);
  [[nodiscard]] Result endFrame(void);

  // EVERYTHING THE DEVICE STILL OWES, FINISHED. Filament's `flushAndWait` is what a client calls
  // before reading a resource back or before tearing down, and it is the only honest way to time
  // GPU work from the CPU: without it a clock measures how fast commands were QUEUED.
  [[nodiscard]] Result flushAndWait(void);

  [[nodiscard]] Result render(Extent frame);
  [[nodiscard]] Result saveScreenshot(std::string_view path);
  [[nodiscard]] Result readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] Result readPixels(Buffer which, std::vector<float> &out);

  // HOW MANY FRAMES THIS PLAN NEEDS BEFORE IT IS SHOWING WHAT IT MEANS TO SHOW. A temporal resolve
  // gathers its samples over TIME -- the projection is jittered by a sub-pixel offset each frame
  // and the history is reprojected and blended -- so a still camera converges only because the
  // SAMPLE moves, and one frame of it is one sample. The plan already computes the number and
  // nothing has ever asked: `kTemporalSettleFrames` is 128, and a client taking a single picture
  // drew two.
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

  // WHERE THE ENGINE COMMENTARY GOES. Handing in nothing turns it off, which is the default: a
  // client can run without any of it, because the engine refuses through `error()` and reports
  // through `measures()`. A client that wants the running account had no way to receive it.
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

  // THE CAMERA THE FRAME IS AIMED WITH, which is not always the one declared: a view that states
  // no camera gets the engine's own framing of what stands, and a client had no way to ask what
  // that came out as. Filament's `View::getCamera()` answers the same question, and a conformance
  // runner that cannot ask it has to reimplement the framing rule and compare its own answer with
  // itself.
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
