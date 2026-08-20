#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <memory>
#include <string>

namespace outshine {

struct Extent {
  int WidthPx = 0;
  int HeightPx = 0;
};

struct Light {
  double Lux = 0.0;
  double ElevationDeg = 0.0;
  double BearingDeg = 0.0;
};

struct Scenario {
  Extent Frame;
  std::string Stands;
  std::string Variant;
  double Fps = 60.0;
  double Fill = 0.9;
  double OrbitDegPerFrame = 0.0;
  Light Key;
  double Environment[3] = {0.0, 0.0, 0.0};
};

class Engine {
public:
  Engine();
  ~Engine();
  Engine(Engine &&) noexcept;
  Engine &operator=(Engine &&) noexcept;
  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  void RenderTo(Extent frame);

  [[nodiscard]] bool Load(const std::string &path);
  [[nodiscard]] bool Declare(const Scenario &scenario);

  [[nodiscard]] bool Advance();
  [[nodiscard]] bool Run();

  [[nodiscard]] int At(void) const;
  [[nodiscard]] int Frames(void) const;
  [[nodiscard]] bool Standing(void) const;
  [[nodiscard]] const std::string &Error(void) const;

private:
  struct State;
  std::unique_ptr<State> S_;
};

} // namespace outshine

#endif
