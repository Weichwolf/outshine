#ifndef OUTSHINE_APP_CAPTURE_STILLCAPTURE_H
#define OUTSHINE_APP_CAPTURE_STILLCAPTURE_H

#include "FrameTiming.h"
#include "ScenarioArguments.h"
#include "Extent.h"
#include "DiagnosticSample.h"
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {
class Engine;
}

namespace outshine::App {

inline constexpr Extent kDefaultCaptureFrame{.WidthPx = 1280, .HeightPx = 720};

struct StillCaptureResult {
  App::FrameTiming Timing;
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

  double SetupMs = 0.0, LoadingMs = 0.0, StreamedS = 0.0;

  double MeasurementMs = 0.0, TurnDegrees = 0.0;

  double SettledOver = 0.0, PosedAtS = 0.0;
  bool Preloaded = false;
  bool Kept = false;

  std::vector<::outshine::DiagnosticSample> Measures;
};

[[nodiscard]] StillCaptureResult CaptureStill(Engine &engine,
                                              std::string_view name,
                                              bool tells,
                                              std::string_view outputDirectory = "captures",
                                              double preloadSeconds = kDefaultPreloadSeconds);

}
#endif
