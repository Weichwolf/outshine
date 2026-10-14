#include "LeafOpticsTable.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>

namespace outshine::Generators {
namespace {

using Spectrum = LeafOpticsTable::Spectrum;
constexpr std::array<double, 3> kLinearSrgbLuminance{0.2126, 0.7152, 0.0722};
constexpr size_t kMostTableSide = 65;

std::array<double, 2> Luminance(const Spectrum &value) noexcept {
  return {value[0] * kLinearSrgbLuminance[0] + value[1] * kLinearSrgbLuminance[1] +
              value[2] * kLinearSrgbLuminance[2],
          value[3] * kLinearSrgbLuminance[0] + value[4] * kLinearSrgbLuminance[1] +
              value[5] * kLinearSrgbLuminance[2]};
}

bool ValidSpectrum(const Spectrum &value) noexcept {
  if (!std::ranges::all_of(value, [](double x) { return std::isfinite(x); })) { return false; }
  const auto light = Luminance(value);
  return light[0] >= 0 && light[1] >= 0 && light[0] + light[1] <= 1;
}

Spectrum BoundedSpectrum(Spectrum value) noexcept {
  const auto light = Luminance(value);
  double scale = 1;
  double maximum = 0;
  for (size_t sheet = 0; sheet < 2; ++sheet) {
    const auto channels = std::span(value).subspan(sheet * 3, 3);
    const auto range = std::ranges::minmax_element(channels);
    if (*range.min < 0) { scale = std::min(scale, light[sheet] / (light[sheet] - *range.min)); }
    maximum += *range.max;
  }
  const double total = light[0] + light[1];
  if (maximum > 1) { scale = std::min(scale, (1 - total) / (maximum - total)); }
  for (size_t channel = 0; channel < value.size(); ++channel) {
    const double grey = light[channel / 3];
    value[channel] = std::clamp(grey + scale * (value[channel] - grey), 0.0, 1.0);
  }
  return value;
}

}

std::expected<LeafOpticsTable, LeafOpticsError>
LeafOpticsTable::Create(size_t side, std::span<const Spectrum> values, Pigments limits) {
  if (side < 3 || side > kMostTableSide || values.size() != side * side ||
      !std::ranges::all_of(values, ValidSpectrum)) {
    return std::unexpected(LeafOpticsError::InvalidTable);
  }
  if (!std::ranges::all_of(limits, [](double x) { return std::isfinite(x) && x > 0; })) {
    return std::unexpected(LeafOpticsError::InvalidLimits);
  }
  return LeafOpticsTable(side, values, limits);
}

LeafOpticsTable::LeafOpticsTable(size_t side, std::span<const Spectrum> values, Pigments limits)
    : Side_(side),
      Limits_(limits),
      LogLimits_{std::log1p(limits[0]), std::log1p(limits[1])},
      Values_(values.begin(), values.end()) {}

std::expected<LeafOpticsTable::Spectrum, LeafOpticsError>
LeafOpticsTable::Sample(Pigments pigments) const noexcept {
  for (size_t at = 0; at < pigments.size(); ++at) {
    if (!std::isfinite(pigments[at]) || pigments[at] < 0 || pigments[at] > Limits_[at]) {
      return std::unexpected(LeafOpticsError::InvalidPigments);
    }
  }
  const double x = std::log1p(pigments[0]) / LogLimits_[0] * static_cast<double>(Side_ - 1);
  const double y = std::log1p(pigments[1]) / LogLimits_[1] * static_cast<double>(Side_ - 1);
  const auto column = std::min(static_cast<size_t>(std::floor(x)), Side_ - 2);
  const auto row = std::min(static_cast<size_t>(std::floor(y)), Side_ - 2);
  const double u = x - static_cast<double>(column);
  const double v = y - static_cast<double>(row);
  const std::array weights{(1 - u) * (1 - v), u * (1 - v), (1 - u) * v, u * v};
  const std::array indices{column * Side_ + row,
                           (column + 1) * Side_ + row,
                           column * Side_ + row + 1,
                           (column + 1) * Side_ + row + 1};
  Spectrum value{};
  for (size_t channel = 0; channel < value.size(); ++channel) {
    for (size_t corner = 0; corner < indices.size(); ++corner) {
      value[channel] += weights[corner] * Values_[indices[corner]][channel];
    }
  }
  return BoundedSpectrum(value);
}

}
