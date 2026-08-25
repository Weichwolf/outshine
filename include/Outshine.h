#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <string_view>
#include <memory>
#include <string>
#include <vector>

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

  void RenderTo(Extent frame);
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

  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] const std::vector<std::string> &Carried(void) const;

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
  struct State;
  [[nodiscard]] bool ReadInto(std::string_view path, Scenario &out);
  std::unique_ptr<State> S_;
};

}

#endif
