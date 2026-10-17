#ifndef OUTSHINE_GENERATORS_FLORA_PLANTRANDOM_H
#define OUTSHINE_GENERATORS_FLORA_PLANTRANDOM_H

#include <array>
#include <cstdint>
#include <span>

namespace outshine::Generators {
class PlantRandom {
public:
  explicit PlantRandom(uint64_t seed) noexcept;
  PlantRandom(uint64_t seed, uint64_t axis) noexcept;
  [[nodiscard]] uint64_t Raw() noexcept;
  [[nodiscard]] double Uniform(double minimum, double maximum) noexcept;

private:
  void Seed(std::span<const uint64_t> entropy) noexcept;
  void Advance() noexcept;
  std::array<uint64_t, 2> State_{}, Increment_{};
};
}
#endif
