#include "src/client/GpuFrameTiming.h"
#include "Check.h"
#include <array>
#include <limits>

int main() {
  using namespace outshine::Test;
  using outshine::Client::GpuFrameTiming;
  const std::array samples{outshine::DiagnosticSample{"cost.render.frames", 90.0, "frames"},
                           outshine::DiagnosticSample{"cost.gpu.frames", 90.0, "frames"},
                           outshine::DiagnosticSample{"cost.gpu.total_ms", 1800.0, "ms"},
                           outshine::DiagnosticSample{"cost.gpu.max_ms", 40.0, "ms"},
                           outshine::DiagnosticSample{"cost.gpu.unavailable", 0.0, "frames"},
                           outshine::DiagnosticSample{"cost.gpu.query_ms", 0.2, "ms"},
                           outshine::DiagnosticSample{"capture.gpu.render_frames", 30.0, "frames"},
                           outshine::DiagnosticSample{"capture.gpu.frames", 30.0, "frames"},
                           outshine::DiagnosticSample{"capture.gpu.total_ms", 600.0, "ms"},
                           outshine::DiagnosticSample{"capture.gpu.max_ms", 32.0, "ms"},
                           outshine::DiagnosticSample{"capture.gpu.unavailable", 0.0, "frames"},
                           outshine::DiagnosticSample{"capture.gpu.query_ms", 0.01, "ms"}};
  const auto capture = GpuFrameTiming::Read(samples);
  CHECK(capture.Complete(30) && capture.MaxMs == 32.0 && capture.TotalMs == 600.0 &&
            capture.QueryMs == 0.01,
        "capture timestamps exclude preload and subsequent screenshot work");
  const auto lifetime = GpuFrameTiming::Read(samples, false);
  CHECK(lifetime.Complete(90) && !lifetime.Complete(30) && lifetime.MaxMs == 40.0,
        "lifetime counts cannot replace a complete measured turn");
  CHECK(!GpuFrameTiming::Read({}).Complete(30) && !GpuFrameTiming{}.Complete(30),
        "absent GPU timestamps remain unproven");
  for (std::size_t omitted = 6; omitted < samples.size(); ++omitted) {
    auto incomplete = samples;
    incomplete[omitted].Name = "unrelated";
    CHECK(!GpuFrameTiming::Read(incomplete).Complete(30),
          "every capture measurement is required even when lifetime data is complete");
  }
  for (const auto member : {&GpuFrameTiming::RenderedFrames,
                            &GpuFrameTiming::TimedFrames,
                            &GpuFrameTiming::TotalMs,
                            &GpuFrameTiming::MaxMs,
                            &GpuFrameTiming::UnavailableFrames,
                            &GpuFrameTiming::QueryMs}) {
    for (double bad : {-1.0,
                       std::numeric_limits<double>::infinity(),
                       std::numeric_limits<double>::quiet_NaN()}) {
      auto invalid = capture;
      invalid.*member = bad;
      CHECK(!invalid.Complete(30), "invalid measurements cannot certify a GPU budget");
    }
  }
  auto partial = capture;
  partial.TimedFrames = 29;
  partial.UnavailableFrames = 1;
  CHECK(!partial.Complete(30), "a single missing frame fails complete GPU coverage");
  CHECK(!capture.Complete(60) && !capture.Complete(0),
        "another sampling profile and an empty capture cannot pass");
  return Report();
}
