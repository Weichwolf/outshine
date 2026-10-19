#include "BenchmarkReport.h"
#include "BenchmarkMotion.h"
#include "ClientBuild.h"
#include "GpuFrameSample.h"
#include "GpuFrameTiming.h"
#include "math/Quantile.h"
#include "math/Mat4.h"
#include "math/Vec3.h"
#include <Outshine.h>
#include <SDL3/SDL_cpuinfo.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Client {
namespace {
using ReportJson = nlohmann::json;

ReportJson VectorJson(const Vec3 &value) {
  return ReportJson::array({value[0], value[1], value[2]});
}

ReportJson CameraJson(const Camera &camera) {
  Mat4 model;
  ReportJson matrix = nullptr;
  if (camera.modelMatrix(model)) {
    matrix = ReportJson::array();
    for (size_t at = 0; at < 16; ++at) { matrix.push_back(model[at]); }
  }
  return {{"position_m", VectorJson(camera.PositionM)},
          {"world_from_camera", std::move(matrix)},
          {"looks_at", camera.LooksAt},
          {"look_at_m", VectorJson(camera.LookAtM)},
          {"up", VectorJson(camera.UpM)},
          {"perspective_fov_deg", camera.FovDeg},
          {"near_m", camera.NearM},
          {"far_m", camera.FarM}};
}

ReportJson Distribution(std::vector<double> values, double budgetMs) {
  if (values.empty()) { return nullptr; }
  std::ranges::sort(values);
  return {{"samples", values.size()},
          {"mean_ms",
           std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size())},
          {"p50_ms", *QuantileOf(values, kMiddleQuantile)},
          {"p95_ms", *QuantileOf(values, kBroadQuantile)},
          {"p99_ms", *QuantileOf(values, kWidestQuantile)},
          {"max_ms", values.back()},
          {"over_budget",
           std::ranges::count_if(values, [budgetMs](double ms) { return ms > budgetMs; })}};
}

ReportJson CpuTimes(const ScenarioBenchmarkReport &report, double budgetMs) {
  std::vector<double> frame, advance, render;
  frame.reserve(report.Frames.size());
  advance.reserve(report.Frames.size());
  render.reserve(report.Frames.size());
  auto samples = ReportJson::array();
  for (const auto &sample : report.Frames) {
    frame.push_back(sample.WallMs);
    advance.push_back(sample.AdvanceMs);
    render.push_back(sample.RenderMs);
    samples.push_back({{"frame_ms", sample.WallMs},
                       {"advance_ms", sample.AdvanceMs},
                       {"render_ms", sample.RenderMs},
                       {"world_refined", sample.Refined}});
  }
  return {{"time_basis", "host wall time excluding pacing; includes synchronous waits"},
          {"complete", !report.Frames.empty() && report.Frames.size() == report.Motion.Frames},
          {"frame", Distribution(std::move(frame), budgetMs)},
          {"advance", Distribution(std::move(advance), budgetMs)},
          {"render", Distribution(std::move(render), budgetMs)},
          {"samples", std::move(samples)}};
}

ReportJson GpuTimes(const ScenarioBenchmarkReport &report, double budgetMs) {
  const auto costs = GpuFrameTiming::Read(report.Measures);
  const bool complete = report.GpuSamples.size() == report.Motion.Frames &&
                        costs.Complete(static_cast<double>(report.Motion.Frames));
  std::vector<double> times;
  times.reserve(report.GpuSamples.size());
  auto samples = ReportJson::array();
  for (const auto &sample : report.GpuSamples) {
    times.push_back(sample.ElapsedMs);
    samples.push_back({{"device", sample.Device},
                       {"submission", sample.Submission},
                       {"elapsed_ms", sample.ElapsedMs}});
  }
  return {{"time_basis", "native submission GPU timestamps"},
          {"complete", complete},
          {"rendered_frames", costs.RenderedFrames},
          {"timed_frames", costs.TimedFrames},
          {"unavailable_frames", costs.UnavailableFrames},
          {"collection_cpu_ms", costs.QueryMs},
          {"frame", complete ? Distribution(std::move(times), budgetMs) : ReportJson(nullptr)},
          {"samples", std::move(samples)}};
}

ReportJson LoadingJson(const Loading &loading) {
  auto sources = ReportJson::array();
  for (const auto &source : loading.Sources) {
    sources.push_back({{"kind", source.Kind},
                       {"id", source.Id},
                       {"revision", source.Revision},
                       {"key", source.Key},
                       {"priority", source.Priority},
                       {"tile_deliveries", source.TileDeliveries}});
  }
  return {{"ground_wanted", loading.GroundWanted},
          {"ground_arrived", loading.GroundArrived},
          {"vector_wanted", loading.VectorWanted},
          {"vector_arrived", loading.VectorArrived},
          {"outstanding", loading.Outstanding},
          {"fetched_mib", loading.FetchedMB},
          {"provider_starts", loading.ProviderStarts},
          {"remote_starts", loading.RemoteStarts},
          {"source_deliveries", loading.SourceDeliveries},
          {"source_from_store", loading.SourceFromStore},
          {"source_bytes", loading.SourceBytes},
          {"preload_ms", loading.PreloadMs},
          {"preload_pump_ms", loading.PreloadPumpMs},
          {"preload_flush_ms", loading.PreloadFlushMs},
          {"preload_await_ms", loading.PreloadAwaitMs},
          {"sources", std::move(sources)}};
}

ReportJson LoadJson(const std::optional<std::array<double, 3>> &value) {
  return value ? ReportJson(*value) : ReportJson(nullptr);
}
}

std::string ScenarioBenchmarkJson(const ScenarioBenchmarkReport &report) {
  const double budgetMs = 1000.0 / report.Motion.RateHz;
  auto cpu = CpuTimes(report, budgetMs);
  auto gpu = GpuTimes(report, budgetMs);
  const bool measurementComplete =
      !report.Frames.empty() && report.Frames.size() == report.Motion.Frames &&
      gpu["rendered_frames"].get<double>() == static_cast<double>(report.Motion.Frames);
  const auto unrefined =
      std::ranges::count_if(report.Frames, [](const auto &frame) { return !frame.Refined; });
  const bool cpuPass = measurementComplete && !cpu["frame"].is_null() &&
                       cpu["frame"]["p99_ms"].get<double>() <= budgetMs;
  ReportJson gpuPass = gpu["complete"].get<bool>()
                           ? ReportJson(gpu["frame"]["p99_ms"].get<double>() <= budgetMs)
                           : ReportJson(nullptr);
  auto metrics = ReportJson::array();
  for (const auto &metric : report.Measures) {
    metrics.push_back({{"name", metric.Name}, {"value", metric.Value}, {"unit", metric.Unit}});
  }
#ifdef __APPLE__
  constexpr std::string_view backend = "Metal";
#else
  constexpr std::string_view backend = "Vulkan";
#endif
  ReportJson json{
      {"benchmark_version", 1},
      {"mode", "scenario"},
      {"measurement_complete", measurementComplete},
      {"scenario", report.Scenario},
      {"declaration_sha256", report.DeclarationSha256},
      {"view", report.View},
      {"cpu", report.Cpu},
      {"gpu", report.Gpu},
      {"backend", backend},
      {"logical_cpu_count", SDL_GetNumLogicalCPUCores()},
      {"ram_bytes", static_cast<uint64_t>(std::max(0, SDL_GetSystemRAM())) * 1024u * 1024u},
      {"build",
       {{"commit", kBuildCommit},
        {"flags", kBuildFlags},
        {"compiler", __VERSION__},
        {"host_load_before", LoadJson(report.LoadBefore)},
        {"host_load_after", LoadJson(report.LoadAfter)}}},
      {"profile",
       {{"width_px", report.Frame.WidthPx},
        {"height_px", report.Frame.HeightPx},
        {"fps", report.Motion.RateHz},
        {"budget_ms", budgetMs}}},
      {"motion",
       {{"frames", report.Motion.Frames},
        {"time_step_s", 1.0 / report.Motion.RateHz},
        {"linear_velocity_mps", VectorJson(report.Motion.LinearVelocityMps)},
        {"angular_velocity_deg_s", VectorJson(report.Motion.AngularVelocityDegS)},
        {"start", CameraJson(report.Start)},
        {"finish", CameraJson(report.Finish)}}},
      {"loading",
       {{"prepared", LoadingJson(report.Prepared)}, {"finished", LoadingJson(report.Finished)}}},
      {"phases_ms",
       {{"setup", report.SetupMs},
        {"preload", report.PreloadMs},
        {"paced_window", report.WindowMs},
        {"final_gpu_flush", report.FlushMs},
        {"final_image", report.ImageMs},
        {"hardware_metadata", report.HardwareMs}}},
      {"world_complete", measurementComplete && unrefined == 0},
      {"unrefined_frames", unrefined},
      {"budget",
       {{"cpu_p99_met", cpuPass},
        {"gpu_p99_met", gpuPass},
        {"met",
         measurementComplete && unrefined == 0 && cpuPass && gpuPass.is_boolean() &&
             gpuPass.get<bool>()}}},
      {"cpu_times", std::move(cpu)},
      {"gpu_times", std::move(gpu)},
      {"metrics", std::move(metrics)}};
  return json.dump(2, ' ', false, ReportJson::error_handler_t::replace) + '\n';
}

}
