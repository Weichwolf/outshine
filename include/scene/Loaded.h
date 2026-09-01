#ifndef OUTSHINE_LOADED_H
#define OUTSHINE_LOADED_H

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Geometry.h"
#include "scenario/Scenario.h"

namespace outshine {

class Loaded {
public:
  Loaded();
  ~Loaded();
  Loaded(Loaded &&) noexcept;
  Loaded &operator=(Loaded &&) noexcept;
  Loaded(const Loaded &) = delete;
  Loaded &operator=(const Loaded &) = delete;

  [[nodiscard]] bool reads(std::string_view path);
  [[nodiscard]] bool wears(std::string_view variant);
  [[nodiscard]] const std::string &error() const;

  [[nodiscard]] const Geometry &geometry() const;

  [[nodiscard]] bool plays(std::span<const int> animations);
  [[nodiscard]] int animations() const;
  [[nodiscard]] double durationS() const;
  [[nodiscard]] bool poses(double seconds);

  [[nodiscard]] bool carriesCamera() const;
  [[nodiscard]] const Camera &camera() const;
  [[nodiscard]] int cameras() const;
  [[nodiscard]] bool camera(int index, Camera &out) const;

  [[nodiscard]] bool frames(double fill, Camera &out) const;
  [[nodiscard]] bool frames(Camera &out) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

} // namespace outshine

#endif
