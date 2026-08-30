#ifndef OUTSHINE_GENERATE_H
#define OUTSHINE_GENERATE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Geometry.h"

namespace outshine {

struct Ask {
  double EastM = 0.0;
  double NorthM = 0.0;
  double ExtentM = 0.0;
  uint64_t Seed = 0;
};

class Generates {
public:
  virtual ~Generates() = default;
  Generates(const Generates &) = delete;
  Generates &operator=(const Generates &) = delete;

  [[nodiscard]] virtual std::string_view kind() const = 0;
  [[nodiscard]] virtual bool make(const Ask &ask, Geometry &into) const = 0;

protected:
  Generates() = default;
};

enum class Ships { Structures, kCount };

inline constexpr std::string_view kShipped[] = {"structures"};

static_assert(sizeof kShipped / sizeof kShipped[0] == (size_t)Ships::kCount,
              "every shipped generator the catalogue enumerates carries a name");

[[nodiscard]] constexpr std::string_view nameOf(Ships which) {
  return kShipped[(size_t)which];
}

[[nodiscard]] constexpr bool EveryShippedKindIsSpelled() {
  for (size_t at = 0; at < (size_t)Ships::kCount; ++at) {
    if (kShipped[at].empty()) { return false; }
    for (size_t over = at + 1; over < (size_t)Ships::kCount; ++over) {
      if (kShipped[at] == kShipped[over]) { return false; }
    }
  }
  return true;
}

static_assert(EveryShippedKindIsSpelled(),
              "a shipped kind is spelled once and is never empty -- a catalogue that carries a "
              "blank or a repeat resolves a declaration by whichever entry it reaches first");

[[nodiscard]] bool writeGlb(const Geometry &what, std::vector<uint8_t> &glb, std::string &error);

class Makers {
public:
  [[nodiscard]] bool offers(const Generates &maker);

  [[nodiscard]] const Generates *named(std::string_view kind) const;
  [[nodiscard]] size_t count() const;

  Makers();
  ~Makers();
  Makers(Makers &&) noexcept;
  Makers &operator=(Makers &&) noexcept;
  Makers(const Makers &) = delete;
  Makers &operator=(const Makers &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

} // namespace outshine

#endif
