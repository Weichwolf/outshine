#ifndef OUTSHINE_OUTSHINE_H
#define OUTSHINE_OUTSHINE_H

#include <string_view>
#include <memory>
#include <string>
#include <vector>

#include <outshine/Scenario.h>
#include <outshine/Store.h>

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

  [[nodiscard]] bool Read(std::string_view path);
  [[nodiscard]] bool Load(std::string_view path);
  [[nodiscard]] bool Declare(const Scenario &scenario);

  [[nodiscard]] const Scenario &Declared(void) const;
  [[nodiscard]] const std::vector<std::string> &Carried(void) const;

  [[nodiscard]] bool Assemble();
  [[nodiscard]] Store &Scene(void);
  [[nodiscard]] const Store &Scene(void) const;

  [[nodiscard]] bool Advance();
  [[nodiscard]] bool Run();

  [[nodiscard]] bool Park();
  [[nodiscard]] bool Resume(std::string_view name);
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

} // namespace outshine

#endif
