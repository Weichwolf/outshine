#include "Camera.h"
#include "Extent.h"
#include "GroupImpostorView.h"
#include "ImageCapture.h"
#include "ImpostorViewDemand.h"
#include "MetalProductRenderer.h"
#include "ReadTextFile.h"
#include "SurfaceTile.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using Json = nlohmann::json;
constexpr size_t kMaximumDirections = 64;
constexpr size_t kFrames = 120;
constexpr double kFovDeg = 55;
constexpr double kFrameSeconds = 1.0 / 30.0;
constexpr size_t kInputLimit = size_t{4} * 1024u * 1024u;

bool ParseNumber(std::string_view text, double &out) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), out);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(out);
}

struct Group {
  std::vector<GroupImpostorView> Views;
  Vec3 CentreM;
  double HalfExtentM = 0;
  std::string CanopyCell;
};

bool LoadGroup(const std::filesystem::path &folder, Group &out) {
  const auto text = ReadTextFile((folder / "group.json").string(), kInputLimit);
  if (!text) { return false; }
  const auto manifest = Json::parse(*text, nullptr, false);
  if (!manifest.contains("centre_m") || !manifest["centre_m"].is_array() ||
      manifest["centre_m"].size() != 3 || !manifest.contains("half_extent_m") ||
      !manifest["half_extent_m"].is_number()) {
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!manifest["centre_m"][axis].is_number()) { return false; }
    out.CentreM[axis] = manifest["centre_m"][axis].get<double>();
  }
  out.HalfExtentM = manifest["half_extent_m"].get<double>();
  if (!manifest.contains("card_views") || !manifest["card_views"].is_number_unsigned()) {
    return false;
  }
  const auto count = manifest["card_views"].get<size_t>();
  if (count < 2 || count > kMaximumDirections) { return false; }
  if (const auto cell = manifest.find("canopy_cell"); cell != manifest.end()) {
    if (!cell->is_string()) { return false; }
    out.CanopyCell = cell->get<std::string>();
  }
  for (size_t at = 0; at < count; ++at) {
    const auto bytes =
        ReadTextFile((folder / (std::to_string(at) + ".card")).string(), kInputLimit);
    if (!bytes) { return false; }
    auto card =
        DecodeGroupImpostorView({reinterpret_cast<const uint8_t *>(bytes->data()), bytes->size()});
    if (!card) { return false; }
    out.Views.push_back(std::move(*card));
  }
  return true;
}

struct FlightPath {
  double DistanceM = 0;
  double ElevationDeg = 0;
  double FinishDistanceM = 0;
};

MetalProductViewAt Flight(Vec3 centre, FlightPath path) {
  return [=](size_t frame) -> std::expected<MetalProductView, std::string> {
    constexpr double radians = std::numbers::pi / 180;
    constexpr double fromDeg = -35;
    constexpr double toDeg = 51;
    const double fraction = static_cast<double>(frame) / (kFrames - 1);
    const double azimuth = std::lerp(fromDeg, toDeg, fraction) * radians;
    const double elevation = path.ElevationDeg * radians;
    const auto distance = std::lerp(path.DistanceM, path.FinishDistanceM, fraction);
    const double horizontal = distance * std::cos(elevation);
    const Vec3 eye = centre + Vec3{{horizontal * std::sin(azimuth),
                                    distance * std::sin(elevation),
                                    horizontal * std::cos(azimuth)}};
    Camera camera;
    camera.PositionM = eye;
    camera.LooksAt = true;
    camera.LookAtM = centre;
    camera.setProjection(Camera::Perspective{.FovDeg = kFovDeg, .NearM = 1, .FarM = 240000});
    MetalProductView view{};
    view.EyeM = eye;
    constexpr double aspect = static_cast<double>(MetalProductFrameWidth) / MetalProductFrameHeight;
    if (!camera.clipMatrix(aspect, view.ClipFromWorld)) {
      return std::unexpected("invalid group control camera");
    }
    return view;
  };
}

bool Trial(std::string_view name,
           const MetalProductInputs &products,
           const MetalProductViewAt &viewAt,
           const std::filesystem::path &output,
           double frameSeconds = 0) {
  const auto frames = RenderMetalProducts(
      products, kFrames, viewAt, true, {.Load = {}, .FrameSeconds = frameSeconds});
  if (!frames) {
    std::cerr << frames.error() << '\n';
    return false;
  }
  std::vector<double> gpu;
  std::vector<double> wall;
  size_t triangles = 0;
  size_t blended = 0;
  for (const auto &sample : frames->Samples) {
    if (!sample.GpuMs) { return false; }
    gpu.push_back(*sample.GpuMs);
    wall.push_back(sample.WallMs);
    triangles = std::max(triangles, sample.TileTriangles + sample.GroupTriangles);
    if (sample.BlendingGroups > 0) { ++blended; }
  }
  std::ranges::sort(gpu);
  std::ranges::sort(wall);
  const auto p99 = [](const std::vector<double> &values) {
    return values[static_cast<size_t>(std::ceil(0.99 * static_cast<double>(values.size()))) - 1];
  };
  const auto trianglesAt = [](const MetalProductFrameSample &sample) {
    return sample.TileTriangles + sample.GroupTriangles;
  };
  const Json report{{"name", name},
                    {"device", frames->Device},
                    {"samples", kFrames},
                    {"gpu_p99_ms", p99(gpu)},
                    {"wall_p99_ms", p99(wall)},
                    {"max_triangles", triangles},
                    {"blended_frames", blended},
                    {"endpoint_triangles",
                     {trianglesAt(frames->Samples.front()), trianglesAt(frames->Samples.back())}},
                    {"uploaded_bytes", frames->UploadedBytes},
                    {"resource_bytes", frames->ResourceBytes}};
  std::cout << report.dump() << '\n';
  const auto png = (output / (std::string(name) + ".png")).string();
  const Extent extent{.WidthPx = MetalProductFrameWidth, .HeightPx = MetalProductFrameHeight};
  return App::SaveImage(frames->Rgba, extent, png).has_value();
}

std::optional<std::vector<MetalSurfaceInstance>> SiteInstances(const char *path,
                                                               const Group &group) {
  const auto text = ReadTextFile(path, kInputLimit);
  if (!text) { return std::nullopt; }
  const auto site = Json::parse(*text, nullptr, false);
  if (group.CanopyCell.empty() || !site.contains("canopy_cell") ||
      site["canopy_cell"] != group.CanopyCell || !site.contains("roots_m") ||
      !site["roots_m"].is_array() || !site.contains("tree_scales") ||
      !site["tree_scales"].is_array() || site["roots_m"].size() != site["tree_scales"].size()) {
    return std::nullopt;
  }
  std::vector<MetalSurfaceInstance> result;
  for (size_t i = 0; i < site["roots_m"].size(); ++i) {
    const auto &point = site["roots_m"][i];
    const auto &scale = site["tree_scales"][i];
    if (!point.is_array() || point.size() != 3 || !scale.is_number() ||
        !std::ranges::all_of(point, &Json::is_number)) {
      return std::nullopt;
    }
    const MetalSurfaceInstance value{
        point[0].get<float>(), point[1].get<float>(), point[2].get<float>(), scale.get<float>()};
    if (!std::ranges::all_of(value, [](float v) { return std::isfinite(v); }) ||
        value.back() <= 0) {
      return std::nullopt;
    }
    result.push_back(value);
  }
  if (result.empty()) { return std::nullopt; }
  return result;
}

int Forest(std::span<const char *const> arguments,
           const SurfaceTile &ground,
           const Group &group,
           FlightPath path) {
  constexpr double radians = std::numbers::pi / 180;
  const bool validDistance = ParseNumber(arguments[8], path.FinishDistanceM) &&
                             path.FinishDistanceM > group.HalfExtentM &&
                             path.FinishDistanceM < 240000;
  auto tree = ReadSurfaceTile(arguments[6]);
  const auto instances = SiteInstances(arguments[7], group);
  if (!validDistance || !tree || !instances || tree->GroundIndices >= tree->Indices.size()) {
    return 2;
  }
  tree->Indices.erase(tree->Indices.begin(), tree->Indices.begin() + tree->GroundIndices);
  tree->GroundIndices = 0;
  const auto view = Flight(group.CentreM, path);
  const double focal = MetalProductFrameHeight / (2.0 * std::tan(kFovDeg * radians / 2));
  const auto ramp = ImpostorProjectionRamp(group.HalfExtentM, group.Views.front().Side, focal);
  std::cout << Json{{"ramp_m", {ramp.BeginM, ramp.EndM}}, {"instances", instances->size()}}.dump()
            << '\n';
  std::array tiles{
      MetalSurface{.Product = ground, .WorldFromTile = {}, .HandoffGroup = {}, .Instances = {}},
      MetalSurface{
          .Product = *tree, .WorldFromTile = {}, .HandoffGroup = {}, .Instances = *instances}};
  MetalImpostorGroup proxy{.Views = group.Views,
                           .WorldFromTile = {},
                           .CentreM = group.CentreM,
                           .HalfExtentM = group.HalfExtentM,
                           .Handoff = {},
                           .FractionalCoverage = true};
  const std::filesystem::path output(arguments[3]);
  if (!Trial("geometry",
             {.Tiles = tiles, .Panoramas = {}, .Groups = {}},
             view,
             output,
             kFrameSeconds) ||
      !Trial("group",
             {.Tiles = std::span(tiles).first(1), .Panoramas = {}, .Groups = std::span(&proxy, 1)},
             view,
             output,
             kFrameSeconds)) {
    return 1;
  }
  proxy.Handoff = ramp;
  tiles.back().HandoffGroup = 0;
  return Trial("handoff",
               {.Tiles = tiles, .Panoramas = {}, .Groups = std::span(&proxy, 1)},
               view,
               output,
               kFrameSeconds)
             ? 0
             : 1;
}

}

int main(int argc, const char *const *argv) {
  if (argc != 6 && argc != 9) {
    std::cerr << "metal_groups <capture> <surface> <output directory> <distance m> <elevation deg> "
                 "[prototype surface, layout, finish distance m]\n";
    return 2;
  }
  const auto surface = ReadSurfaceTile(argv[2]);
  Group group;
  if (!surface || !LoadGroup(argv[1], group)) { return 1; }
  FlightPath path;
  if (!ParseNumber(argv[4], path.DistanceM) || !ParseNumber(argv[5], path.ElevationDeg) ||
      path.DistanceM <= group.HalfExtentM || path.DistanceM >= 240000 || path.ElevationDeg <= -89 ||
      path.ElevationDeg >= 89) {
    return 2;
  }
  path.FinishDistanceM = path.DistanceM;
  if (argc == 9) {
    return Forest(std::span(argv, static_cast<size_t>(argc)), *surface, group, path);
  }
  auto ground = *surface;
  if (!RetainSurfaceGround(ground)) { return 1; }
  const auto view = Flight(group.CentreM, path);
  const MetalSurface full{
      .Product = *surface, .WorldFromTile = {}, .HandoffGroup = {}, .Instances = {}};
  const MetalSurface base{
      .Product = ground, .WorldFromTile = {}, .HandoffGroup = {}, .Instances = {}};
  const MetalImpostorGroup proxy{.Views = group.Views,
                                 .WorldFromTile = {},
                                 .CentreM = group.CentreM,
                                 .HalfExtentM = group.HalfExtentM,
                                 .Handoff = {}};
  return Trial("geometry",
               {.Tiles = std::span(&full, 1), .Panoramas = {}, .Groups = {}},
               view,
               argv[3]) &&
                 Trial("group",
                       {.Tiles = std::span(&base, 1),
                        .Panoramas = {},
                        .Groups = std::span(&proxy, 1)},
                       view,
                       argv[3])
             ? 0
             : 1;
}
