#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL.h>

#include <Scenario.h>

namespace outshine {

struct Argument {
  enum class Kind : uint8_t { Number, Text };
  Kind Is = Kind::Number;
  double Number = 0.0;
  std::string_view Text;
};

class Host {
public:
  virtual ~Host() = default;
  [[nodiscard]] virtual bool Calls(std::string_view name, std::span<const Argument> args) = 0;
};

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
  [[nodiscard]] bool Scrolls(double byPx);
  [[nodiscard]] std::vector<std::string> Views(void) const;
  [[nodiscard]] bool Handles(const SDL_Event &event);
  [[nodiscard]] bool DrawsInto(Extent offscreen);
  void Under(Roots roots);
  [[nodiscard]] bool Drove(void) const;
  [[nodiscard]] double ReachedM(void) const;
  [[nodiscard]] double RouteM(void) const;
  [[nodiscard]] bool Compose(void);
  [[nodiscard]] size_t GroundTiles(void) const;
  [[nodiscard]] bool Capture(std::string_view path);
  [[nodiscard]] const Roots &Under(void) const;

  [[nodiscard]] bool Read(std::string_view path);
  [[nodiscard]] bool Load(std::string_view path);
  [[nodiscard]] bool Declare(const Scenario &scenario);
  [[nodiscard]] bool Shows(const std::vector<Surface> &surfaces);

  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] const std::vector<std::string> &Carried(void) const;
  [[nodiscard]] const std::vector<std::string> &Measured(void) const;

  [[nodiscard]] bool Assemble();

  [[nodiscard]] bool Advance();
  [[nodiscard]] bool Run();

  [[nodiscard]] bool Park();
  [[nodiscard]] bool Resume(std::string_view name);
  [[nodiscard]] bool Discard(std::string_view name);
  [[nodiscard]] bool Save(std::string_view path) const;
  [[nodiscard]] bool Restore(std::string_view path);
  [[nodiscard]] std::vector<std::string> Parked(void) const;

  [[nodiscard]] int At(void) const;
  [[nodiscard]] int Frames(void) const;
  [[nodiscard]] bool Standing(void) const;
  [[nodiscard]] const std::string &Error(void) const;

private:
  [[nodiscard]] bool Rides(void);

  struct State;
  [[nodiscard]] bool ReadInto(std::string_view path, Scenario &out);
  std::unique_ptr<State> S_;
};

}

#endif
