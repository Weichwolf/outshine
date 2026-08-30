#ifndef OUTSHINE_CLIENT_PLACECAMERA_H
#define OUTSHINE_CLIENT_PLACECAMERA_H

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <Outshine.h>

namespace outshine {
class Engine;
class LogSink;
} // namespace outshine

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

  /// How many frames the plan wanted before the picture settled, and the instant the SHOT itself
  /// was taken at -- a still of a moving subject says nothing without the second of these.
  double SettledOver = 0.0, PosedAtS = 0.0;
  bool Preloaded = false;
  bool Kept = false;

  /// Every measure the engine published for the frame this shot was taken on.
  std::vector<::outshine::Measure> Measures;
};

/// The declaration a place stands on, on its own, so a reader can write it down and read
/// it back rather than take the camera's word for what it declared.
[[nodiscard]] ::outshine::Scenario ScenarioFor(const Place &place);

[[nodiscard]] Shot Draw(class ::outshine::Engine &engine,
                        std::string_view name,
                        bool tells,
                        std::string_view under = "places");

[[nodiscard]] std::span<const Place> Places();
[[nodiscard]] const Place *PlaceNamed(std::string_view name);
[[nodiscard]] double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high);

/// The statistic's own negative control: what a bare vertical gradient varies by along its rows.
/// A picture of nothing IS a vertical gradient, so this must come in far under the bar a real
/// frame is held to -- and if it ever does not, the bar separates nothing and every green under
/// it is worthless.
[[nodiscard]] double ControlVariation();
/// Where the engine says what it is doing while a place is taken. Null keeps it silent.
extern ::outshine::LogSink *Telling;

/// Whether the engine walks its own geometry and publishes what it finds -- coincident corners,
/// edges on one triangle, needles. Off by default: it costs 11.3 s of Shibuya's load and answers
/// questions that change when a GENERATOR changes, never between two frames.
extern bool Audits;

[[nodiscard]] Shot Take(const Place &place, bool tells);

} // namespace outshine::Shots
#endif
