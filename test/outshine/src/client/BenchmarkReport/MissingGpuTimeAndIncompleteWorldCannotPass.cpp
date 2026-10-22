#include "BenchmarkReport.h"
#include "Check.h"
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine::Client;
  using namespace outshine::Test;
  ScenarioBenchmarkReport report;
  report.Motion.Frames = 2;
  report.Motion.RateHz = 60;
  report.Frame = {32, 32};
  report.Scenario = "quote\" and newline\n";
  report.Frames = {{.WallMs = 3, .AdvanceMs = 1, .RenderMs = 2, .Refined = true},
                   {.WallMs = 4, .AdvanceMs = 1, .RenderMs = 3, .Refined = true}};
  report.GpuSamples = {{.Device = 1, .Submission = 9, .ElapsedMs = 1},
                       {.Device = 1, .Submission = 12, .ElapsedMs = 2}};
  report.Measures = {{"capture.gpu.render_frames", 2, "frames"},
                     {"capture.gpu.frames", 2, "frames"},
                     {"capture.gpu.total_ms", 3, "ms"},
                     {"capture.gpu.max_ms", 2, "ms"},
                     {"capture.gpu.unavailable", 0, "frames"},
                     {"capture.gpu.query_ms", 0.5, "ms"}};
  const auto decode = [&report] {
    return nlohmann::json::parse(ScenarioBenchmarkJson(report), nullptr, false);
  };
  auto json = decode();
  CHECK(!json.is_discarded() && json["scenario"] == report.Scenario,
        "report serialization preserves escaped scenario identity");
  CHECK(json["measurement_complete"] == true && json["world_complete"] == true &&
            json["budget"]["met"] == true,
        "a complete measured window has an explicit independent budget result");
  CHECK(json["cpu_times"]["frame"]["p50_ms"] == 3 && json["cpu_times"]["frame"]["p99_ms"] == 4 &&
            json["gpu_times"]["frame"]["p50_ms"] == 1 && json["gpu_times"]["frame"]["p99_ms"] == 2,
        "CPU and GPU retain their own nearest-rank durations without adding asynchronous phases");
  CHECK(json["build"]["host_load_before"].is_null(),
        "unavailable host load is not fabricated as zero");
  report.GpuSamples.pop_back();
  report.Measures[1].Value = 1;
  report.Measures[4].Value = 1;
  json = decode();
  CHECK(json["measurement_complete"] == true && json["gpu_times"]["complete"] == false &&
            json["gpu_times"]["frame"].is_null() && json["budget"]["gpu_p99_met"].is_null() &&
            json["budget"]["met"] == false,
        "one missing GPU timestamp preserves raw evidence and blocks complete GPU percentiles");
  report.Frames.front().Refined = false;
  json = decode();
  CHECK(json["world_complete"] == false && json["unrefined_frames"] == 1 &&
            json["budget"]["met"] == false,
        "a partially refined moving world cannot pass the budget gate");
  report.Motion.Frames = 3;
  json = decode();
  CHECK(json["measurement_complete"] == false && json["cpu_times"]["complete"] == false,
        "a shorter observed window never masquerades as the requested frame count");
  return Report();
}
