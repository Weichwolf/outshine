#ifndef OUTSHINE_CLIENT_PLACECAMERA_H
#define OUTSHINE_CLIENT_PLACECAMERA_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
class Engine;
}

namespace outshine::Shots {

inline constexpr int kWidePx = 1280;
inline constexpr int kHighPx = 720;
inline constexpr double kFrameBudgetMs = 1000.0 / 60.0;

struct Place {
  const char *Name = "";
  double LatDeg = 0.0;
  double LonDeg = 0.0;
  double BearingDeg = 0.0;
};

struct Shot {
  std::string Digest;
  std::string Wrote;
  std::string Why;

  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0;
  std::size_t Frames = 0, OverBudget = 0, WorstAt = 0;

  double Triangles = 0.0;
  double BareTiles = 0.0;
  double VariationAlongRows = 0.0;

  double StandingMs = 0.0, LoadingMs = 0.0, StreamedS = 0.0;
  bool Preloaded = false;
  bool Kept = false;
};

[[nodiscard]] Shot Draw(class ::outshine::Engine &engine, std::string_view name, bool tells);

[[nodiscard]] std::span<const Place> Places();
[[nodiscard]] const Place *PlaceNamed(std::string_view name);
[[nodiscard]] double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high);

/// The statistic's own negative control: what a bare vertical gradient varies by along its rows.
/// A picture of nothing IS a vertical gradient, so this must come in far under the bar a real
/// frame is held to -- and if it ever does not, the bar separates nothing and every green under
/// it is worthless.
[[nodiscard]] double ControlVariation();
[[nodiscard]] Shot Take(const Place &place, bool tells);

}
#endif
