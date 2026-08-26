#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include <Event.h>
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

  [[nodiscard]] bool DrawsInto(SDL_Window *presents);
  void Offers(Host *host);
  [[nodiscard]] bool Takes(std::string_view view);
  [[nodiscard]] bool Handles(const SDL_Event &event);
  [[nodiscard]] bool DrawsInto(Extent offscreen);
  void Under(Roots roots);
  [[nodiscard]] bool RenderTo(Extent frame);
  [[nodiscard]] bool Capture(std::string_view path);
  [[nodiscard]] bool Pixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] bool Read(std::string_view path);
  [[nodiscard]] bool Declare(const Scenario &scenario);
  [[nodiscard]] bool Shows(const std::vector<Surface> &surfaces);

  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] const std::vector<std::string> &Carried(void) const;
  [[nodiscard]] const std::vector<Measure> &Numbers(void) const;

  [[nodiscard]] bool Assemble();

  [[nodiscard]] double Along(void) const;
  [[nodiscard]] double Whole(void) const;

  [[nodiscard]] bool Advance();
  [[nodiscard]] bool Advance(double elapsedS);
  [[nodiscard]] double StepS(void) const;
  [[nodiscard]] bool Run();

  [[nodiscard]] bool Park();
  [[nodiscard]] bool Resume(std::string_view name);
  [[nodiscard]] bool Discard(std::string_view name);
  [[nodiscard]] bool Save(std::string_view path) const;
  [[nodiscard]] bool Restore(std::string_view path);
  [[nodiscard]] std::vector<std::string> Parked(void) const;

  [[nodiscard]] bool Standing(void) const;
  [[nodiscard]] const std::string &Error(void) const;

private:
  struct State;
  [[nodiscard]] bool ReadInto(std::string_view path, Scenario &out);
  std::unique_ptr<State> S_;
};

}

#endif
