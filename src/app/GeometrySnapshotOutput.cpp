#include "GeometrySnapshotOutput.h"
#include "GeometrySnapshot.h"
#include "Engine.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <expected>
#include <fstream>
#include <ios>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace outshine::App {
namespace {
bool Write(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  output.close();
  return static_cast<bool>(output);
}
}

std::expected<void, std::string> WriteGeometrySnapshot(Renderer &renderer,
                                                       const std::filesystem::path &root,
                                                       std::string_view report) {
  std::error_code error;
  if (!std::filesystem::create_directory(root, error)) {
    return std::unexpected("geometry snapshot requires a fresh writable directory");
  }
  Render::GeometrySnapshot snapshot;
  if (const auto read = renderer.readGeometry(snapshot); !read) { return read; }
  nlohmann::json metadata{
      {"version", 1},
      {"scope", "main mesh batches; terrain, prisms and transmission are separate"},
      {"generation", snapshot.Generation},
      {"readback_bytes", snapshot.ReadbackBytes},
      {"selection_ready", snapshot.SelectionReady},
      {"eye_from_anchor_m", snapshot.EyeFromAnchorM.Axis},
      {"camera_axes",
       {snapshot.CameraAxes[0].Axis, snapshot.CameraAxes[1].Axis, snapshot.CameraAxes[2].Axis}},
      {"depth_clip_from_eye", snapshot.DepthClipFromEye},
      {"viewport_px", snapshot.ViewportPx},
      {"byte_order", std::endian::native == std::endian::little ? "little" : "big"},
      {"measurement_excludes_snapshot", true}};
  const auto save = [&](std::string_view name, const auto &values, uint32_t columns) {
    const auto bytes = std::as_bytes(std::span(values));
    metadata["streams"][name] = {{"file", std::string(name) + ".bin"},
                                 {"rows", values.size()},
                                 {"columns", columns},
                                 {"bytes", bytes.size()}};
    return Write(root / (std::string(name) + ".bin"), bytes);
  };
  if (!save("roots", snapshot.Roots, 12) || !save("templates", snapshot.Templates, 4) ||
      !save("cluster_spheres", snapshot.Spheres, 12) ||
      !save("placements", snapshot.Placements, 16) || !save("batches", snapshot.Batches, 12) ||
      !save("source_indices", snapshot.SourceIndices, 1) || !save("kept", snapshot.Kept, 1) ||
      !save("visible", snapshot.Visible, 1) || !save("arguments", snapshot.Arguments, 1) ||
      !save("depth", snapshot.Depth, 1) ||
      !Write(root / "benchmark.json", std::as_bytes(std::span(report)))) {
    return std::unexpected("geometry snapshot stream write failed");
  }
  metadata["batch_columns"] = {"material",
                               "layout",
                               "pipeline",
                               "motion",
                               "passes",
                               "instances",
                               "first_index",
                               "index_count",
                               "first_instance",
                               "first_cluster",
                               "cluster_count",
                               "surface_kind"};
  const auto text = metadata.dump(2) + '\n';
  if (!Write(root / "geometry.json", std::as_bytes(std::span(text)))) {
    return std::unexpected("geometry snapshot metadata write failed");
  }
  return {};
}
}
