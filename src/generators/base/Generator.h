#ifndef OUTSHINE_GENERATION_GENERATE_H
#define OUTSHINE_GENERATION_GENERATE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include <optional>

#include "Earth.h"
#include "Geometry.h"
#include "LevelOfDetail.h"
#include "ProjectedErrorBudget.h"

namespace outshine::Generators {

class HeightSampler {
public:
  virtual ~HeightSampler() = default;

  HeightSampler(const HeightSampler &) = delete;

  HeightSampler &operator=(const HeightSampler &) = delete;

  [[nodiscard]] virtual std::optional<double>
  sampleHeightAslM(const LongitudeLatitudeHeight &at) const = 0;

protected:
  HeightSampler() = default;
};

struct Parameter {
  std::string_view Name;
  std::string_view Value;
};

struct Request {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  double ExtentM = 0.0;
  std::span<const Parameter> Parameters;
  uint64_t Seed = 0;
  const HeightSampler *Ground = nullptr;
  LevelOfDetail Coarseness = LevelOfDetail::Fine;
  ProjectedErrorBudget Projection;
};

class Generator {
public:
  using Product = std::expected<Geometry, std::string>;

  virtual ~Generator() = default;

  Generator(const Generator &) = delete;

  Generator &operator=(const Generator &) = delete;

  [[nodiscard]] virtual std::string_view kind() const = 0;

  [[nodiscard]] virtual Product make(const Request &asked) const = 0;

protected:
  Generator() = default;
};

enum class Shipped : uint8_t {

  Structures,
  kCount
};

inline constexpr std::array<std::string_view, static_cast<size_t>(Shipped::kCount)> kShipped = {
    "structures"};

[[nodiscard]] constexpr std::string_view nameOf(Shipped which) noexcept {
  const auto index = static_cast<size_t>(which);
  return index < kShipped.size() ? kShipped[index] : std::string_view{};
}

[[nodiscard]] constexpr bool EveryShippedKindIsSpelled() noexcept {
  for (size_t at = 0; at < static_cast<size_t>(Shipped::kCount); ++at) {
    if (kShipped[at].empty()) { return false; }
    for (size_t over = at + 1; over < static_cast<size_t>(Shipped::kCount); ++over) {
      if (kShipped[at] == kShipped[over]) { return false; }
    }
  }
  return true;
}

static_assert(EveryShippedKindIsSpelled(),
              "a shipped kind is spelled once and is never empty -- a catalogue that carries a "
              "blank or a repeat resolves a declaration by whichever entry it reaches first");

class Registry {
public:
  enum class RegistrationError { EmptyKind, DuplicateKind };

  [[nodiscard]] std::expected<void, RegistrationError>
  registerGenerator(const Generator &generator);

  [[nodiscard]] const Generator *named(std::string_view kind) const;

  [[nodiscard]] size_t count() const;

  Registry();

  ~Registry();

  Registry(Registry &&other) noexcept;

  Registry &operator=(Registry &&other) noexcept;

  Registry(const Registry &) = delete;

  Registry &operator=(const Registry &) = delete;

private:
  struct Kept;
  std::unique_ptr<Kept> Kept_;
};

}

#endif
