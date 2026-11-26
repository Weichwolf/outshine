#include "BenchmarkReport.h"
#include "BenchmarkMotion.h"
#include "DeviceMemoryJson.h"
#include "AppBuild.h"
#include "GpuFrameSample.h"
#include "GpuFrameTiming.h"
#include "Quantile.h"
#include "Mat4.h"
#include "Vec3.h"
#include "Units.h"
#include <Engine.h>
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

namespace outshine::App {
namespace {
using ReportJson = nlohmann::json;

ReportJson MemoryJson(const ProcessMemorySnapshot &sample) {
  const auto bytes = [](std::optional<uint64_t> value) {
    return value ? ReportJson(*value) : ReportJson(nullptr);
  };
  return {{"resident_bytes", bytes(sample.ResidentBytes)},
          {"peak_resident_bytes", bytes(sample.PeakResidentBytes)},
          {"footprint_bytes", bytes(sample.FootprintBytes)},
          {"peak_footprint_bytes", bytes(sample.PeakFootprintBytes)},
          {"collection_ns", bytes(sample.CollectionNs)},
          {"peak_scope", "process_lifetime"},
          {"sample_phase", "after_measurement"}};
}

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
  std::vector<double> frame;
  std::vector<double> advance;
  std::vector<double> render;
  std::vector<double> work;
  std::vector<double> waits;
  frame.reserve(report.Frames.size());
  advance.reserve(report.Frames.size());
  render.reserve(report.Frames.size());
  work.reserve(report.Frames.size());
  waits.reserve(report.Frames.size());
  auto samples = ReportJson::array();
  for (const auto &sample : report.Frames) {
    frame.push_back(sample.WallMs);
    advance.push_back(sample.AdvanceMs);
    render.push_back(sample.RenderMs);
    work.push_back(sample.WallMs - sample.SyncWaitMs);
    waits.push_back(sample.SyncWaitMs);
    samples.push_back({{"frame_ms", sample.WallMs},
                       {"advance_ms", sample.AdvanceMs},
                       {"render_ms", sample.RenderMs},
                       {"cpu_work_ms", work.back()},
                       {"sync_wait_ms", sample.SyncWaitMs},
                       {"world_refined", sample.Refined}});
  }
  return {{"time_basis", "host wall time excluding pacing; includes synchronous waits"},
          {"work_time_basis", "frame wall time minus explicit fence and command-acquisition waits"},
          {"complete", !report.Frames.empty() && report.Frames.size() == report.Motion.Frames},
          {"frame", Distribution(std::move(frame), budgetMs)},
          {"advance", Distribution(std::move(advance), budgetMs)},
          {"render", Distribution(std::move(render), budgetMs)},
          {"work", Distribution(std::move(work), budgetMs)},
          {"sync_wait", Distribution(std::move(waits), budgetMs)},
          {"samples", std::move(samples)}};
}

ReportJson GpuTimes(const ScenarioBenchmarkReport &report, double budgetMs) {
  const auto &costs = report.GpuTiming;
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
  const auto wait = [](double milliseconds, size_t calls, size_t signals) {
    return ReportJson{{"ms", milliseconds}, {"calls", calls}, {"signals", signals}};
  };
  const auto &waited = loading.Waited;
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
          {"terrain_workers",
           {{"mesh_jobs", loading.TerrainMeshJobs},
            {"field_jobs", loading.TerrainFieldJobs},
            {"mesh_ms", loading.TerrainMeshWorkerMs},
            {"field_ms", loading.TerrainFieldWorkerMs}}},
          {"fetched_mib", loading.FetchedMB},
          {"provider_starts", loading.ProviderStarts},
          {"remote_starts", loading.RemoteStarts},
          {"source_deliveries", loading.SourceDeliveries},
          {"source_from_store", loading.SourceFromStore},
          {"source_bytes", loading.SourceBytes},
          {"asset_package_bytes_read", loading.AssetPackageBytesRead},
          {"preload_ms", loading.PreloadMs},
          {"preload_pump_ms", loading.PreloadPumpMs},
          {"preload_flush_ms", loading.PreloadFlushMs},
          {"preload_await_ms", loading.PreloadAwaitMs},
          {"preload_pumps", loading.PreloadPumps},
          {"preload_flushes", loading.PreloadFlushes},
          {"preload_awaits", loading.PreloadAwaits},
          {"waits",
           {{"structure", wait(waited.StructureMs, waited.StructureCalls, waited.StructureSignals)},
            {"classification", wait(waited.ClassMs, waited.ClassCalls, waited.ClassSignals)},
            {"tiles", wait(waited.TileMs, waited.TileCalls, waited.TileSignals)},
            {"world_worker",
             wait(waited.WorldWorkerMs, waited.WorldWorkerCalls, waited.WorldWorkerSignals)},
            {"idle", {{"ms", waited.IdleMs}, {"calls", waited.IdleCalls}}},
            {"tiles_no_outstanding",
             {{"ms", waited.TileNoOutstandingMs}, {"calls", waited.TileNoOutstandingCalls}}}}},
          {"sources", std::move(sources)}};
}

ReportJson TileContent(const BenchmarkTileCoverage &tile) {
  return {{"kind", "single_opaque_tile"},
          {"address", tile.Address},
          {"origin_lon_lat", tile.OriginLonLat},
          {"revision", tile.Revision},
          {"geometric_error_m", tile.GeometricErrorM},
          {"draws_per_frame", 1},
          {"overlay_triangles_per_frame", tile.OverlayTriangles},
          {"triangles_per_frame", tile.Triangles}};
}

ReportJson TileContent(std::span<const BenchmarkTileCoverage> tiles) {
  if (tiles.size() == 1) { return TileContent(tiles.front()); }
  auto records = ReportJson::array();
  size_t triangles = 0;
  size_t overlayTriangles = 0;
  for (const auto &tile : tiles) {
    records.push_back(TileContent(tile));
    triangles += tile.Triangles;
    overlayTriangles += tile.OverlayTriangles;
  }
  return {{"kind", "opaque_surface_cut"},
          {"tiles", std::move(records)},
          {"draws_per_frame", tiles.size()},
          {"overlay_triangles_per_frame", overlayTriangles},
          {"triangles_per_frame", triangles}};
}

ReportJson PanoramaContent(const BenchmarkPanoramaCoverage &bake) {
  return {{"kind", "far_colour_depth_cubemap"},
          {"face_px", bake.Side},
          {"anchor_eus_m", bake.AnchorM.Axis},
          {"revision", bake.Revision},
          {"axial_near_m", bake.AxialNearM},
          {"axial_overlap_end_m", bake.AxialOverlapEndM},
          {"maximum_range_m", bake.MaximumRangeM},
          {"maximum_eye_translation_m", bake.MaximumTranslationM},
          {"draws_per_frame", 1},
          {"triangles_per_frame", 1}};
}

ReportJson PanoramaContent(const BenchmarkPanoramaCoverage &bake,
                           const std::optional<BenchmarkPanoramaHandoff> &handoff) {
  auto content = PanoramaContent(bake);
  if (handoff) {
    content["handoff"] = {{"blend_window_frames", handoff->BlendWindowFrames},
                          {"next", PanoramaContent(handoff->Next)}};
  }
  return content;
}

ReportJson RenderedContent(const ScenarioBenchmarkReport &report) {
  if (!report.Tiles.empty() && report.Panorama) {
    auto near = TileContent(report.Tiles);
    const auto triangles = near["triangles_per_frame"].get<size_t>();
    const auto &groups = report.GroupCoverage;
    near["endpoint_triangles"] = groups.SurfaceEndpointTriangles;
    if (groups.CanopySites > 0) {
      near["canopy"] = {{"sites", groups.CanopySites},
                        {"mesh_groups", groups.CanopyMeshGroups},
                        {"coarse_mesh_groups", groups.CanopyCoarseGroups},
                        {"mesh_detail_handoffs", groups.CanopyDetailHandoffs},
                        {"instances", groups.CanopyInstances}};
    }
    ReportJson content{{"kind", "near_far_composition"},
                       {"near", std::move(near)},
                       {"far", PanoramaContent(*report.Panorama, report.PanoramaHandoff)},
                       {"peak_triangles_per_frame", 1 + groups.PeakSurfaceAndGroupTriangles},
                       {"draws_per_frame", 1 + report.Tiles.size() + groups.CanopyMeshGroups},
                       {"triangles_per_frame", 1 + triangles}};
    if (groups.Groups > 0) {
      content["kind"] = "near_mid_far_composition";
      content.erase("draws_per_frame");
      content.erase("triangles_per_frame");
      content["mid"] = {{"groups", groups.Groups},
                        {"resident_views", groups.ResidentViews},
                        {"available_views", groups.AvailableViews},
                        {"peak_triangles_per_frame", groups.PeakTriangles},
                        {"handoff_groups", groups.HandoffGroups},
                        {"blended_frames", groups.BlendedFrames},
                        {"endpoint_triangles", groups.GroupEndpointTriangles}};
    }
    if (groups.Groups > 0 || groups.CanopyMeshGroups > 0) {
      content.erase("draws_per_frame");
      content.erase("triangles_per_frame");
    }
    return content;
  }
  if (report.Panorama) { return PanoramaContent(*report.Panorama, report.PanoramaHandoff); }
  if (!report.Tiles.empty()) { return TileContent(report.Tiles); }
  return {{"kind", "scenario_world"}};
}

ReportJson BenchmarkTraffic(const ScenarioBenchmarkReport &report) {
  if (report.Panorama || !report.Tiles.empty()) {
    const bool indexed = report.ProductStoreReadBytes.has_value();
    const size_t panoramaBytes =
        (report.Panorama ? report.Panorama->StoredBytes : 0) +
        (report.PanoramaHandoff ? report.PanoramaHandoff->Next.StoredBytes : 0);
    uint64_t tileBytes = 0;
    for (const auto &tile : report.Tiles) { tileBytes += tile.StoredBytes; }
    if (report.SurfaceStream) { tileBytes += report.SurfaceStream->DecodedBytes; }
    const uint64_t bytes = report.ProductStoreReadBytes.value_or(tileBytes) +
                           (report.IndexedPanoramaRequestedBytes ? 0 : panoramaBytes);
    return {
        {"source_delivery_bytes", 0},
        {"asset_read_bytes", bytes},
        {"asset_read_scope",
         indexed ? "stored package bytes plus direct product payloads"
                 : "prepared product payloads"},
        {"tile_requested_bytes", report.TileRequestedBytes},
        {"group_requested_bytes", report.GroupCoverage.RequestedBytes},
        {"canopy_mesh_requested_bytes", report.GroupCoverage.CanopyRequestedBytes},
        {"panorama_requested_bytes", report.IndexedPanoramaRequestedBytes.value_or(panoramaBytes)},
        {"panorama_decoded_bytes", panoramaBytes},
        {"tile_decoded_bytes", report.Tiles.empty() ? ReportJson(nullptr) : ReportJson(tileBytes)},
        {"asset_read_ms", report.ProductReadMs},
        {"upload_bytes", report.ProductUploadBytes},
        {"upload_scope",
         report.SurfaceStream ? "initial and staged shared product resources"
                              : "initial shared product buffers and textures"},
        {"upload_ms", report.ProductUploadMs}};
  }
  const auto bytes = std::ranges::find_if(report.Measures, [](const auto &metric) {
    return metric.Name == "rebuild: bytes offered for upload";
  });
  return {{"source_delivery_bytes", report.Finished.SourceBytes},
          {"asset_read_bytes", report.Finished.AssetPackageBytesRead},
          {"asset_read_scope", "asset package reads tracked by the engine"},
          {"upload_bytes",
           bytes == report.Measures.end() ? ReportJson(nullptr) : ReportJson(bytes->Value)},
          {"upload_scope", "bytes offered in the last reported geometry rebuild"},
          {"upload_ms", nullptr}};
}

void ReportRenderPath(const ScenarioBenchmarkReport &report, ReportJson &json) {
  if (report.SurfaceStream) {
    const auto &stream = *report.SurfaceStream;
    json["surface_stream"] = {
        {"first_refined_frame",
         stream.FirstRefinedFrame ? ReportJson(*stream.FirstRefinedFrame) : ReportJson(nullptr)},
        {"initial_cut_final", stream.InitialCutFinal},
        {"initial_tiles", report.Tiles.size()},
        {"initial_canopy_sites", report.SurfaceStream->InitialCanopySites},
        {"refined_tiles", stream.Tiles},
        {"reused_tiles", stream.ReusedTiles},
        {"reused_groups", stream.ReusedGroups},
        {"refined_triangles", stream.Triangles},
        {"ready_ms", stream.ReadyMs},
        {"error", stream.Error}};
  }
  if (!report.Tiles.empty() || report.Panorama) {
    auto &timing = json["gpu_times"];
    timing["scope"] = report.Panorama ? "command buffer containing one far panorama pass"
                                      : "command buffer containing one opaque tile pass";
    std::string_view pass = report.Panorama ? "far_panorama" : "opaque_tile";
    if (!report.Tiles.empty() && report.Panorama) {
      timing["scope"] = "command buffer containing the depth-composited near and far pass";
      pass = "near_far_composition";
      if (report.GroupCoverage.Groups > 0) {
        timing["scope"] = "command buffer containing the depth-composited near, mid and far pass";
        pass = "near_mid_far_composition";
      }
    }
    json["pass_times"] = {{"scope", timing["scope"]}, {"passes", {{pass, timing["frame"]}}}};
  } else {
    json["gpu_times"]["scope"] = "frame submissions recorded by the world renderer";
  }
}

ReportJson LoadJson(const std::optional<std::array<double, 3>> &value) {
  return value ? ReportJson(*value) : ReportJson(nullptr);
}
}

std::string ScenarioBenchmarkJson(const ScenarioBenchmarkReport &report) {
  const double budgetMs = kMsPerS / report.Motion.RateHz;
  auto cpu = CpuTimes(report, budgetMs);
  auto gpu = GpuTimes(report, budgetMs);
  const bool measurementComplete =
      !report.Frames.empty() && report.Frames.size() == report.Motion.Frames &&
      gpu["rendered_frames"].get<double>() == static_cast<double>(report.Motion.Frames);
  const double completedMs = report.WindowMs + report.FlushMs;
  const bool completedWindow = measurementComplete && std::isfinite(report.WindowMs) &&
                               report.WindowMs > 0 && std::isfinite(report.FlushMs) &&
                               report.FlushMs >= 0 && std::isfinite(completedMs);
  const auto unrefined =
      std::ranges::count_if(report.Frames, [](const auto &frame) { return !frame.Refined; });
  const bool contentComplete =
      measurementComplete && unrefined == 0 && report.FinalWorldBlockers.empty();
  const bool worldComplete = contentComplete && report.Tiles.empty() && !report.Panorama;
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
  auto worldGaps = ReportJson::array();
  for (const auto &gap : report.WorldGaps) {
    worldGaps.push_back({{"first_frame", gap.FirstFrame}, {"blockers", gap.Blockers}});
  }
  std::string_view renderPath = report.Tiles.empty() ? "world" : "metal_tile";
  if (report.Panorama) { renderPath = "metal_panorama"; }
  if (report.Panorama && !report.Tiles.empty()) { renderPath = "metal_near_far"; }
  ReportJson json{
      {"benchmark_version", 2},
      {"mode", "scenario"},
      {"render_path", renderPath},
      {"rendered_content", RenderedContent(report)},
      {"content_complete", contentComplete},
      {"traffic", BenchmarkTraffic(report)},
      {"measurement_complete", measurementComplete},
      {"wall_throughput_fps",
       completedWindow
           ? ReportJson(static_cast<double>(report.Frames.size()) * kMsPerS / completedMs)
           : ReportJson(nullptr)},
      {"scenario", report.Scenario},
      {"declaration_sha256", report.DeclarationSha256},
      {"view", report.View},
      {"cpu", report.Cpu},
      {"gpu", report.Gpu},
      {"backend", backend},
      {"logical_cpu_count", SDL_GetNumLogicalCPUCores()},
      {"ram_bytes", static_cast<uint64_t>(std::max(0, SDL_GetSystemRAM())) * 1024u * 1024u},
      {"process_memory", MemoryJson(report.Memory)},
      {"gpu_memory",
       Diagnostics::Calibration::DeviceMemoryJson(report.GpuMemory, "after_measurement")},
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
        {"pacing", report.SurfaceStream ? "realtime" : "unpaced"},
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
        {"path_preparation", report.PathPreparationMs},
        {report.SurfaceStream ? "paced_window" : "unpaced_window", report.WindowMs},
        {"final_gpu_flush", report.FlushMs},
        {"final_inspection", report.InspectionMs},
        {"final_image", report.ImageMs},
        {"hardware_metadata", report.HardwareMs}}},
      {"world_complete", worldComplete},
      {"image_row_variation",
       report.ImageRowVariation ? ReportJson(*report.ImageRowVariation) : ReportJson(nullptr)},
      {"unrefined_frames", unrefined},
      {"world_gaps", std::move(worldGaps)},
      {"final_world_blockers", report.FinalWorldBlockers},
      {"budget",
       {{"cpu_p99_met", cpuPass},
        {"gpu_p99_met", gpuPass},
        {"met", worldComplete && cpuPass && gpuPass.is_boolean() && gpuPass.get<bool>()}}},
      {"cpu_times", std::move(cpu)},
      {"gpu_times", std::move(gpu)},
      {"metrics", std::move(metrics)}};
  ReportRenderPath(report, json);
  if (report.ProductSamples) { json["profile"]["raster_samples"] = *report.ProductSamples; }
  return json.dump(2, ' ', false, ReportJson::error_handler_t::replace) + '\n';
}

}
