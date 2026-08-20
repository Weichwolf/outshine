#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <memory>
#include <string>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

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

  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] const std::vector<std::string> &Carried(void) const;

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
