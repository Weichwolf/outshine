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
}

namespace outshine::Shots {

inline constexpr int kWidePx = 1280;
inline constexpr int kHighPx = 720;
inline constexpr double kFrameBudgetMs = 1000.0 / 60.0;

struct Place {

  const char *Name = "";

  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;

  double HeightAslM = 0.0;

  double BearingDeg = 0.0;

  double PitchDeg = 0.0;

  double FovDeg = 0.0;

  const char *WhenUtc = "";
};

struct Shot {
  std::string Digest;
  std::string Wrote;
  std::string Why;

  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0;

  double AdvanceP99Ms = 0.0, RenderP99Ms = 0.0;

  double AdvanceWorstMs = 0.0, RenderWorstMs = 0.0;
  std::size_t Frames = 0, OverBudget = 0, WorstAt = 0;

  double PeakHeapMB = 0.0;
  double PeakCostMs = 0.0;

  double Triangles = 0.0;

  double BareTiles = 0.0;

  double VariationAlongRows = 0.0;

  double StandingMs = 0.0, LoadingMs = 0.0, StreamedS = 0.0;

  double SettledOver = 0.0, PosedAtS = 0.0;
  bool Preloaded = false;
  bool Kept = false;

  std::vector<::outshine::Measure> Measures;
};

[[nodiscard]] ::outshine::Scenario::Document ScenarioFor(const Place &place);

[[nodiscard]] Shot Draw(class ::outshine::Engine &engine,
                        std::string_view name,
                        bool tells,
                        std::string_view under = "places");

[[nodiscard]] std::span<const Place> Places();
[[nodiscard]] const Place *PlaceNamed(std::string_view name);
[[nodiscard]] double VariationAlongRows(std::span<const std::uint8_t> rgba, int wide, int high);

[[nodiscard]] double ControlVariation();

extern ::outshine::LogSink *Telling;

extern bool Audits;

[[nodiscard]] Shot Take(const Place &place, bool tells);

}
#endif
