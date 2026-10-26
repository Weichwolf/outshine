#ifndef OUTSHINE_GENERATORS_FLORA_SPECIESMIXTURE_H
#define OUTSHINE_GENERATORS_FLORA_SPECIESMIXTURE_H

#include <cstdint>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators {

class SpeciesMixture {
public:
  static constexpr size_t kMostSpecies = 64;

  [[nodiscard]] static std::expected<SpeciesMixture, std::string>
  Prepare(std::span<const std::string_view> names, std::span<const double> weights);

  [[nodiscard]] std::optional<uint32_t> Choose(double uniform) const noexcept;

private:
  struct Entry {
    double Upper;
    uint32_t Species;
  };

  std::vector<Entry> Cumulative_;
};

}
#endif
