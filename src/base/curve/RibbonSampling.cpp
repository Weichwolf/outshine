#include "RibbonSampling.h"
#include "Carriageway.h"
#include "math/RenderFrame.h"
#include "Vec2.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kMaxRibbonStations = 262144;

[[nodiscard]] std::expected<size_t, std::string_view>
StationCount(const Section &section, double fromM, double toM, double stepM) {
  if (!std::isfinite(section.HalfWidthM) || !(section.HalfWidthM > 0.0) ||
      !std::isfinite(section.ThicknessM) || !(section.ThicknessM > 0.0) ||
      !std::isfinite(section.ShoulderM) || section.ShoulderM < 0.0 ||
      !std::isfinite(section.HalfWidthM + section.ShoulderM)) {
    return std::unexpected("ribbon width and thickness must be finite and positive; shoulder must "
                           "be finite and nonnegative");
  }
  if (!std::isfinite(fromM) || !std::isfinite(toM) || !std::isfinite(stepM) || !(stepM > 0.0) ||
      !(toM > fromM)) {
    return std::unexpected(
        "ribbon interval must be finite and forward with a finite positive step");
  }
  const double intervals = (toM - fromM) / stepM;
  if (!std::isfinite(intervals) || intervals >= static_cast<double>(kMaxRibbonStations)) {
    return std::unexpected("ribbon station count exceeds its generation budget");
  }
  const auto whole = static_cast<size_t>(intervals);
  const bool onGrid = fromM + static_cast<double>(whole) * stepM == toM;
  const size_t stations = whole + (onGrid ? 1 : 2);
  if (stations > kMaxRibbonStations) {
    return std::unexpected("ribbon station count exceeds its generation budget");
  }
  return stations;
}

std::expected<std::vector<double>, std::string_view>
SamplingStations(const Section &section,
                 double fromM,
                 double toM,
                 double stepM,
                 std::span<const double> required) {
  const auto count = StationCount(section, fromM, toM, stepM);
  if (!count) { return std::unexpected(count.error()); }
  if (required.size() > kMaxRibbonStations) {
    return std::unexpected("required ribbon stations exceed their input budget");
  }
  if (!std::ranges::is_sorted(required) || std::ranges::any_of(required, [&](double at) {
        return !std::isfinite(at) || at < fromM || at > toM;
      })) {
    return std::unexpected("required ribbon stations must be finite, sorted and in the interval");
  }
  std::vector<double> uniform(*count);
  for (size_t at = 0; at < *count; ++at) {
    uniform[at] = at + 1 == *count ? toM : std::min(toM, fromM + static_cast<double>(at) * stepM);
  }
  std::vector<double> sampling(uniform.size() + required.size());
  std::ranges::merge(uniform, required, sampling.begin());
  const auto duplicates = std::ranges::unique(sampling);
  sampling.erase(duplicates.begin(), duplicates.end());
  if (sampling.size() > kMaxRibbonStations) {
    return std::unexpected("combined ribbon stations exceed their generation budget");
  }
  return sampling;
}

struct SectionVertex {
  Vec3 PositionM;
  Vec3 NormalM;
  double AcrossM = 0;
};

RibbonVertex Stored(SectionVertex sample) noexcept {
  RibbonVertex result;
  result.AcrossM = static_cast<float>(sample.AcrossM);
  for (size_t axis = 0; axis < 3; ++axis) {
    result.PositionM[axis] = static_cast<float>(sample.PositionM[axis]);
    result.NormalM[axis] = static_cast<float>(sample.NormalM[axis]);
  }
  return result;
}
}

std::expected<RibbonSamplingPlan, std::string>
PrepareRibbonSampling(const ReferenceLine &along,
                      const Section &section,
                      double fromM,
                      double toM,
                      double stepM,
                      std::span<const double> requiredStations) {
  auto sampling = SamplingStations(section, fromM, toM, stepM, requiredStations);
  if (!sampling) { return std::unexpected(std::string(sampling.error())); }
  const auto curvature = along.MaxAbsCurvaturePerM(fromM, toM);
  if (!curvature) { return std::unexpected("ribbon requires a finite reference-line interval"); }
  if (*curvature > 0.0 && section.HalfWidthM + section.ShoulderM >= 1.0 / *curvature) {
    return std::unexpected(
        "ribbon width reaches the local curvature radius and folds its horizontal offset");
  }
  Placed first;
  if (!along.At(fromM, first)) {
    return std::unexpected("the reference line places nothing at " + std::to_string(fromM) + " m");
  }
  const Astride surface = StandAt(first, {.AlongM = fromM, .AcrossM = 0.0, .HalfWidthM = 0.0});
  return RibbonSamplingPlan{
      .OriginM = {{first.EastM, surface.HeightM, RenderFrame::ZOfNorth(first.NorthM)}},
      .StationsM = std::move(*sampling)};
}

RibbonSectionSamples SampleRibbonSection(const Placed &on,
                                         const Section &section,
                                         const Vec3 &originM,
                                         RibbonForm form) noexcept {
  const std::array<double, kRibbonAcross> acrossAt = {{-(section.HalfWidthM + section.ShoulderM),
                                                       -section.HalfWidthM,
                                                       section.HalfWidthM,
                                                       section.HalfWidthM + section.ShoulderM}};
  const Vec2 left = {{-std::sin(on.HeadingRad), std::cos(on.HeadingRad)}};
  std::array<Astride, kRibbonAcross> stood{};
  RibbonSectionSamples result;
  result.Count = form == RibbonForm::Surface ? kRibbonAcross : kRibbonAcross * 2 + 4;
  for (size_t which = 0; which < kRibbonAcross; ++which) {
    stood[which] = StandAt(on, {.AcrossM = acrossAt[which], .HalfWidthM = 0.0});
    const double eastM = on.EastM + left[0] * acrossAt[which];
    const double northM = on.NorthM + left[1] * acrossAt[which];
    result.Vertices[which] = Stored({.PositionM = {{eastM - originM[0],
                                                    stood[which].HeightM - originM[1],
                                                    RenderFrame::ZOfNorth(northM) - originM[2]}},
                                     .NormalM = {{stood[which].NormalM[0],
                                                  stood[which].NormalM[1],
                                                  RenderFrame::ZOfNorth(stood[which].NormalM[2])}},
                                     .AcrossM = acrossAt[which]});
  }
  if (form == RibbonForm::Surface) { return result; }
  for (size_t which = 0; which < kRibbonAcross; ++which) {
    const Astride &surface = stood[which];
    const double eastM = on.EastM + left[0] * acrossAt[which];
    const double northM = on.NorthM + left[1] * acrossAt[which];
    result.Vertices[kRibbonAcross + which] = Stored(
        {.PositionM = {{eastM - surface.NormalM[0] * section.ThicknessM - originM[0],
                        surface.HeightM - surface.NormalM[1] * section.ThicknessM - originM[1],
                        RenderFrame::ZOfNorth(northM - surface.NormalM[2] * section.ThicknessM) -
                            originM[2]}},
         .NormalM = {{-surface.NormalM[0], -surface.NormalM[1], surface.NormalM[2]}},
         .AcrossM = acrossAt[which]});
  }
  for (size_t side = 0; side < 2; ++side) {
    const size_t which = side == 0 ? 0 : kRibbonAcross - 1;
    const Astride &surface = stood[which];
    const double outward = which == 0 ? -1.0 : 1.0;
    const double eastM = on.EastM + left[0] * acrossAt[which];
    const double northM = on.NorthM + left[1] * acrossAt[which];
    for (size_t below = 0; below < 2; ++below) {
      const double sink = below != 0 ? section.ThicknessM : 0.0;
      result.Vertices[kRibbonAcross * 2 + side * 2 + below] = Stored(
          {.PositionM = {{eastM - surface.NormalM[0] * sink - originM[0],
                          surface.HeightM - surface.NormalM[1] * sink - originM[1],
                          RenderFrame::ZOfNorth(northM - surface.NormalM[2] * sink) - originM[2]}},
           .NormalM = {{left[0] * outward, 0.0, RenderFrame::ZOfNorth(left[1] * outward)}},
           .AcrossM = acrossAt[which]});
    }
  }
  return result;
}
}
