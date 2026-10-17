#ifndef OUTSHINE_CLIENT_GPUFRAMETIMING_H
#define OUTSHINE_CLIENT_GPUFRAMETIMING_H

#include <cmath>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <diagnostics/DiagnosticSample.h>

namespace outshine::Client {

struct GpuFrameTiming {
  double RenderedFrames = 0.0;
  double TimedFrames = 0.0;
  double TotalMs = 0.0;
  double MaxMs = 0.0;
  double UnavailableFrames = 0.0;
  double QueryMs = 0.0;

  [[nodiscard]] bool Complete(double expectedFrames) const noexcept {
    return std::isfinite(expectedFrames) && expectedFrames > 0.0 &&
           RenderedFrames == expectedFrames && TimedFrames == expectedFrames &&
           UnavailableFrames == 0.0 && std::isfinite(TotalMs) && std::isfinite(MaxMs) &&
           MaxMs > 0.0 && TotalMs >= MaxMs && std::isfinite(QueryMs) && QueryMs >= 0.0;
  }

  [[nodiscard]] static GpuFrameTiming Read(std::span<const DiagnosticSample> samples,
                                           bool captured = true) {
    const auto value = [samples](std::string_view name) {
      for (const auto &sample : samples) {
        if (sample.Name == name) { return sample.Value; }
      }
      return std::numeric_limits<double>::quiet_NaN();
    };
    const std::string prefix = captured ? "capture.gpu." : "cost.gpu.";
    return {.RenderedFrames = value(captured ? "capture.gpu.render_frames" : "cost.render.frames"),
            .TimedFrames = value(prefix + "frames"),
            .TotalMs = value(prefix + "total_ms"),
            .MaxMs = value(prefix + "max_ms"),
            .UnavailableFrames = value(prefix + "unavailable"),
            .QueryMs = value(prefix + "query_ms")};
  }
};

}
#endif
