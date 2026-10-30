#include "BenchmarkReport.h"
#include "Check.h"
#include <nlohmann/json.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>

int main() {
  using namespace outshine::App;
  using namespace outshine::Test;
  ScenarioBenchmarkReport report;
  report.Motion.Frames = 2;
  report.Motion.RateHz = 60;
  report.Frame = {.WidthPx = 32, .HeightPx = 32};
  report.InspectionMs = 1'000'000;
  report.Scenario = "quote\" and newline\n";
  report.Frames = {{.WallMs = 3, .AdvanceMs = 1, .RenderMs = 2, .Refined = true},
                   {.WallMs = 4, .AdvanceMs = 1, .RenderMs = 3, .Refined = true}};
  report.GpuSamples = {{.Device = 1, .Submission = 9, .ElapsedMs = 1},
                       {.Device = 1, .Submission = 12, .ElapsedMs = 2}};
  report.Measures = {{.Name = "capture.gpu.render_frames", .Value = 2, .Unit = "frames"},
                     {.Name = "capture.gpu.frames", .Value = 2, .Unit = "frames"},
                     {.Name = "capture.gpu.total_ms", .Value = 3, .Unit = "ms"},
                     {.Name = "capture.gpu.max_ms", .Value = 2, .Unit = "ms"},
                     {.Name = "capture.gpu.unavailable", .Value = 0, .Unit = "frames"},
                     {.Name = "capture.gpu.query_ms", .Value = 0.5, .Unit = "ms"}};
  const auto decode = [&report] {
    return nlohmann::json::parse(ScenarioBenchmarkJson(report), nullptr, false);
  };
  auto json = decode();
  CHECK(!json.is_discarded() && json["scenario"] == report.Scenario,
        "report serialization preserves escaped scenario identity");
  CHECK(json["measurement_complete"] == true && json["world_complete"] == true &&
            json["budget"]["met"] == true,
        "a complete measured window has an explicit independent budget result");
  CHECK(json["phases_ms"]["final_inspection"] == report.InspectionMs,
        "post-window inspection is explicit and never charged to the frame budget");
  report.FinalWorldBlockers = "required geometry publication is pending";
  json = decode();
  CHECK(json["measurement_complete"] == true && json["world_complete"] == false &&
            json["budget"]["met"] == false &&
            json["final_world_blockers"] == report.FinalWorldBlockers,
        "terminal world blockers reject acceptance without discarding the completed measurement");
  report.FinalWorldBlockers.clear();
  json = decode();
  CHECK(json["cpu_times"]["frame"]["p50_ms"] == 3 && json["cpu_times"]["frame"]["p99_ms"] == 4 &&
            json["gpu_times"]["frame"]["p50_ms"] == 1 && json["gpu_times"]["frame"]["p99_ms"] == 2,
        "CPU and GPU retain their own nearest-rank durations without adding asynchronous phases");
  CHECK(json["build"]["host_load_before"].is_null(),
        "unavailable host load is not fabricated as zero");
  CHECK(json["profile"]["pacing"] == "unpaced" && json["phases_ms"].contains("unpaced_window") &&
            json["wall_throughput_fps"].is_null(),
        "maximum-throughput execution is explicit and missing durations do not imply throughput");
  report.WindowMs = 20;
  report.FlushMs = 5;
  json = decode();
  CHECK(json["profile"]["pacing"] == "unpaced" && json["phases_ms"]["unpaced_window"] == 20 &&
            !json["phases_ms"].contains("paced_window") && json["wall_throughput_fps"] == 80 &&
            json["profile"]["fps"] == 60 && json["motion"]["frames"] == 2,
        "unpaced throughput includes final GPU completion and preserves simulation timing");
  for (const double invalid :
       {-1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    report.WindowMs = invalid;
    CHECK(decode()["wall_throughput_fps"].is_null(),
          "invalid measurement duration never fabricates throughput");
    report.WindowMs = 20;
    report.FlushMs = invalid;
    CHECK(decode()["wall_throughput_fps"].is_null(),
          "invalid GPU completion duration never fabricates throughput");
    report.FlushMs = 5;
  }
  report.WindowMs = 0;
  CHECK(decode()["wall_throughput_fps"].is_null(),
        "a flush duration alone cannot stand in for the missing measured window");
  report.WindowMs = std::numeric_limits<double>::max();
  report.FlushMs = std::numeric_limits<double>::max();
  CHECK(decode()["wall_throughput_fps"].is_null(),
        "duration overflow never implies zero-cost completed work");
  report.WindowMs = 20;
  report.FlushMs = 5;
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
  CHECK(json["wall_throughput_fps"].is_null(),
        "an incomplete measurement never publishes completed-window throughput");
  return Report();
}
