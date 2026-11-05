#include "RoadDisplaySampler.h"
#include "math/RenderFrame.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <numeric>
#include <optional>
#include <string_view>
#include <vector>

namespace outshine::Generators {
namespace {
using Ring = std::array<Vec3, kRibbonAcross>;
constexpr std::string_view kInvalid =
    "road display requires a finite valid curve, section and budget";
constexpr std::string_view kWork = "road display exceeds its admitted pose or station work";
constexpr std::string_view kSource = "road display source model exceeds its position budget";

struct Profile {
  std::vector<double> Seams;
  std::vector<double> Second;
  Vec3 Origin;
  size_t Queries = 0;
};

double Norm(const Vec3 &v) {
  return std::hypot(v[0], v[1], v[2]);
}

Ring Positions(const Placed &pose, Section section, const Vec3 &origin) {
  const std::array across{-section.HalfWidthM - section.ShoulderM,
                          -section.HalfWidthM,
                          section.HalfWidthM,
                          section.HalfWidthM + section.ShoulderM};
  Ring result;
  for (size_t at = 0; at < across.size(); ++at) {
    result[at] = {
        {pose.EastM - std::sin(pose.HeadingRad) * across[at] - origin[0],
         pose.HeightM - across[at] * std::tan(pose.BankRad) - origin[1],
         RenderFrame::ZOfNorth(pose.NorthM + std::cos(pose.HeadingRad) * across[at]) - origin[2]}};
  }
  return result;
}

std::array<double, 4> Controls(double a, double b, double da, double db, double length) {
  return {a, a + length * da / 3, b - length * db / 3, b};
}

double First(const std::array<double, 4> &v, double length) {
  return 3 * std::max({std::abs(v[1] - v[0]), std::abs(v[2] - v[1]), std::abs(v[3] - v[2])}) /
         length;
}

double Second(const std::array<double, 4> &v, double length) {
  return 6 * std::max(std::abs(v[2] - 2 * v[1] + v[0]), std::abs(v[3] - 2 * v[2] + v[1])) /
         (length * length);
}

std::optional<double> SurfaceSecond(const std::array<Placed, 2> &ends,
                                    const Placed &middle,
                                    double length,
                                    double width,
                                    double curvature) {
  const auto height =
      Controls(ends[0].HeightM, ends[1].HeightM, ends[0].Slope, ends[1].Slope, length);
  const auto bank = Controls(
      ends[0].BankRad, ends[1].BankRad, ends[0].BankRatePerM, ends[1].BankRatePerM, length);
  const double bankMax =
      std::max({std::abs(bank[0]), std::abs(bank[1]), std::abs(bank[2]), std::abs(bank[3])});
  if (bankMax >= std::acos(-1.0) / 2 || width * curvature >= 1) { return std::nullopt; }
  const double rate = First(bank, length);
  const double cosine = std::cos(bankMax);
  const double q2 =
      (Second(bank, length) + 2 * std::tan(bankMax) * rate * rate) / (cosine * cosine);
  const double planar =
      std::hypot(width * middle.CurvatureRatePerM, curvature * (1 + width * curvature));
  const double result = std::hypot(planar, Second(height, length) + width * q2);
  return std::isfinite(result) ? std::optional(result) : std::nullopt;
}

std::expected<Profile, std::string_view> Prepare(const ReferenceLine &line, Section section) {
  Profile result;
  result.Seams = line.Seams();
  Placed a;
  if (result.Seams.size() < 2 || !line.At(0, a)) { return std::unexpected(kInvalid); }
  result.Queries = 1;
  result.Origin = {{a.EastM, a.HeightM, RenderFrame::ZOfNorth(a.NorthM)}};
  result.Second.reserve(result.Seams.size() - 1);
  for (size_t at = 0; at + 1 < result.Seams.size(); ++at) {
    const double start = result.Seams[at];
    const double end = result.Seams[at + 1];
    Placed b;
    Placed middle;
    const auto curvature = line.MaxAbsCurvaturePerM(start, end);
    if (!curvature || !line.At(end, b) || !line.At(std::midpoint(start, end), middle)) {
      return std::unexpected(kInvalid);
    }
    result.Queries += 2;
    const auto second = SurfaceSecond(
        {a, b}, middle, end - start, section.HalfWidthM + section.ShoulderM, *curvature);
    if (!second) { return std::unexpected(kSource); }
    result.Second.push_back(*second);
    a = b;
  }
  return result;
}

struct Interval {
  double First = 0;
  double Last = 0;
};

std::expected<double, std::string_view> Envelope(const ReferenceLine &line,
                                                 Section section,
                                                 Profile &profile,
                                                 Interval interval,
                                                 size_t queriesMost) {
  const auto first = std::ranges::upper_bound(profile.Seams, interval.First);
  const auto last = std::ranges::lower_bound(profile.Seams, interval.Last);
  const size_t required = static_cast<size_t>(last - first) + 2;
  if (required > queriesMost - profile.Queries) { return std::unexpected(kWork); }
  const double length = interval.Last - interval.First;
  Placed pose;
  Ring start;
  Ring end;
  if (!line.At(interval.First, pose)) { return std::unexpected(kInvalid); }
  start = Positions(pose, section, profile.Origin);
  if (!line.At(interval.Last, pose)) { return std::unexpected(kInvalid); }
  end = Positions(pose, section, profile.Origin);
  profile.Queries += 2;
  double maximum = 0;
  double residual = 0;
  double previousAt = interval.First;
  size_t span = static_cast<size_t>(first - profile.Seams.begin() - 1);
  for (auto next = first;; ++next) {
    const bool final = next == last;
    const double station = final ? interval.Last : *next;
    Ring points = end;
    if (!final) {
      if (!line.At(station, pose)) { return std::unexpected(kInvalid); }
      points = Positions(pose, section, profile.Origin);
      ++profile.Queries;
    }
    double current = 0;
    const double t = (station - interval.First) / length;
    for (size_t at = 0; at < kRibbonAcross; ++at) {
      current = std::max(current, Norm(points[at] - (start[at] + (end[at] - start[at]) * t)));
    }
    const double part = station - previousAt;
    maximum =
        std::max(maximum, std::max(residual, current) + part * part * profile.Second[span] / 8);
    if (final) { break; }
    residual = current;
    previousAt = station;
    ++span;
  }
  double twist = 0;
  double rounding = 0;
  for (size_t at = 0; at < kRibbonAcross; ++at) {
    for (const Ring *ring : {&start, &end}) {
      Vec3 difference;
      for (size_t axis = 0; axis < 3; ++axis) {
        difference[axis] = (*ring)[at][axis] - static_cast<float>((*ring)[at][axis]);
      }
      rounding = std::max(rounding, Norm(difference));
    }
    if (at + 1 < kRibbonAcross) {
      twist = std::max(twist, Norm((end[at + 1] - end[at]) - (start[at + 1] - start[at])) / 4);
    }
  }
  const double result = maximum + twist + rounding;
  if (!std::isfinite(result)) { return std::unexpected(kSource); }
  return result;
}
}

std::expected<RoadDisplayPlan, std::string_view> SampleRoadDisplay(const ReferenceLine &line,
                                                                   Section section,
                                                                   double errorM,
                                                                   RoadDisplayLimits limits) {
  if (!std::isfinite(errorM) || errorM <= 0 || !std::isfinite(section.HalfWidthM) ||
      section.HalfWidthM <= 0 || !std::isfinite(section.ShoulderM) || section.ShoulderM < 0 ||
      limits.StationsMost < 2 || limits.PoseQueriesMost < 4) {
    return std::unexpected(kInvalid);
  }
  const size_t upper = line.SegmentCount() + line.RiseKnotCount() + line.BankKnotCount() + 1;
  if (upper > limits.PoseQueriesMost / 2) { return std::unexpected(kWork); }
  const auto truncation = line.PlanarTruncationBoundM(0, line.LengthM());
  if (!truncation || !std::isfinite(*truncation) || 2 * *truncation >= errorM) {
    return std::unexpected(kSource);
  }
  auto profile = Prepare(line, section);
  if (!profile) { return std::unexpected(profile.error()); }
  std::vector<Interval> pending{{.First = 0, .Last = line.LengthM()}};
  RoadDisplayPlan result{.StationsM = {0}, .SourceTruncationM = *truncation};
  while (!pending.empty()) {
    const Interval interval = pending.back();
    pending.pop_back();
    const auto bound = Envelope(line, section, *profile, interval, limits.PoseQueriesMost);
    if (!bound) { return std::unexpected(bound.error()); }
    const double total = *bound + 2 * *truncation;
    if (total <= errorM) {
      result.StationsM.push_back(interval.Last);
      result.PositionBoundM = std::max(result.PositionBoundM, total);
      continue;
    }
    if (pending.size() + result.StationsM.size() + 2 > limits.StationsMost) {
      return std::unexpected(kWork);
    }
    const double middle = std::midpoint(interval.First, interval.Last);
    if (middle <= interval.First || middle >= interval.Last) { return std::unexpected(kSource); }
    pending.push_back({.First = middle, .Last = interval.Last});
    pending.push_back({.First = interval.First, .Last = middle});
  }
  result.PoseQueries = profile->Queries;
  return result;
}
}
