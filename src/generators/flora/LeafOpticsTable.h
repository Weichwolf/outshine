#ifndef OUTSHINE_GENERATORS_FLORA_LEAFOPTICSTABLE_H
#define OUTSHINE_GENERATORS_FLORA_LEAFOPTICSTABLE_H

#include <array>
#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators {

enum class LeafOpticsError { InvalidTable, InvalidLimits, InvalidPigments };

class LeafOpticsTable {
public:
  using Spectrum = std::array<double, 6>;
  using Pigments = std::array<double, 2>;
  static constexpr Pigments kPigmentLimitsUnsaid{80.0, 10.0};

  [[nodiscard]] static std::expected<LeafOpticsTable, LeafOpticsError>
  Create(size_t side, std::span<const Spectrum> values, Pigments limits = kPigmentLimitsUnsaid);

  [[nodiscard]] std::expected<Spectrum, LeafOpticsError> Sample(Pigments pigments) const noexcept;

  [[nodiscard]] size_t Side() const noexcept { return Side_; }

  [[nodiscard]] Pigments Limits() const noexcept { return Limits_; }

  [[nodiscard]] std::span<const Spectrum> Values() const noexcept { return Values_; }

private:
  LeafOpticsTable(size_t side, std::span<const Spectrum> values, Pigments limits);

  size_t Side_;
  Pigments Limits_;
  Pigments LogLimits_;
  std::vector<Spectrum> Values_;
};

}
#endif
