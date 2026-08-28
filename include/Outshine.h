#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include "Generate.h"
#include "Geometry.h"

#include <Event.h>
#include <Scene.h>
#include <Scenario.h>

namespace outshine {

struct Roots {
  std::string Assets;
  std::string Shipped;
  std::string Cache;
  bool Offline = false;
};

class Engine {
public:
  Engine();
  ~Engine();
  Engine(Engine &&) noexcept;
  Engine &operator=(Engine &&) noexcept;
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  [[nodiscard]] bool drawsInto(SDL_Window *presents);
  void offers(Host *host);
  void offers(const Generates &maker);
  [[nodiscard]] bool setView(std::string_view view);
  [[nodiscard]] bool handleEvent(const SDL_Event &event);
  [[nodiscard]] bool drawsInto(Extent offscreen);
  void setRoots(Roots roots);
  [[nodiscard]] bool render(Extent frame);
  [[nodiscard]] bool saveScreenshot(std::string_view path);
  [[nodiscard]] bool readPixels(std::vector<uint8_t> &rgba);
  [[nodiscard]] bool inspect(void);
  [[nodiscard]] bool mix(std::span<float> stereo, int rate);

  [[nodiscard]] bool readScenario(std::string_view path);
  [[nodiscard]] bool setGeometry(const Geometry &geometry);
  [[nodiscard]] bool declare(const Scenario &scenario);
  [[nodiscard]] bool setSurfaces(const std::vector<Surface> &surfaces);

  [[nodiscard]] const Scenario &declaration(void) const;
  [[nodiscard]] Store &scene(void);
  [[nodiscard]] const Store &scene(void) const;
  [[nodiscard]] const std::vector<std::string> &unacted(void) const;
  [[nodiscard]] const std::vector<Measure> &measures(void) const;

  void keepSamples(size_t steps);
  void stepTimesMs(std::vector<double> &out) const;
  void frameTimesMs(std::vector<double> &out) const;

  [[nodiscard]] bool assemble();


  [[nodiscard]] bool advance();
  [[nodiscard]] bool advance(double elapsedS);
  [[nodiscard]] double stepSeconds(void) const;
  [[nodiscard]] bool run();

  [[nodiscard]] bool park();
  [[nodiscard]] bool resume(std::string_view name);
  [[nodiscard]] bool discard(std::string_view name);
  [[nodiscard]] bool save(std::string_view path) const;
  [[nodiscard]] bool restore(std::string_view path);
  [[nodiscard]] std::vector<std::string> parked(void) const;

  [[nodiscard]] bool Standing(void) const;
  [[nodiscard]] const std::string &error(void) const;

private:
  struct State;
  [[nodiscard]] bool readScenarioInto(std::string_view path, Scenario &out);
  [[nodiscard]] bool generated(const Scenario &scenario);
  void ships(void);
  std::unique_ptr<State> S_;
};

}

#endif
