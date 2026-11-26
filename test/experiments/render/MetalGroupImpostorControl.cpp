#include "Camera.h"
#include "Extent.h"
#include "GroupImpostorView.h"
#include "ImageCapture.h"
#include "MetalProductRenderer.h"
#include "ReadTextFile.h"
#include "SurfaceTile.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <numbers>
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
constexpr size_t kDirections = 13;
constexpr size_t kFrames = 120;
constexpr size_t kInputLimit = size_t{4} * 1024u * 1024u;

bool ParseNumber(std::string_view text, double &out) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), out);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::isfinite(out);
}

struct Group {
  std::vector<GroupImpostorView> Views;
  Vec3 CentreM;
  double HalfExtentM = 0;
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
  for (size_t at = 0; at < kDirections; ++at) {
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

bool RemoveOverlay(SurfaceTile &surface) {
  const size_t grid = static_cast<size_t>(surface.GridSide) * surface.GridSide;
  if (grid == 0 || grid > surface.Vertices.size() || surface.GroundIndices == 0) { return false; }
  const auto colourFirst = surface.Vertices.size() - surface.Colours.size();
  surface.Vertices.resize(grid);
  surface.Indices.resize(surface.GroundIndices);
  surface.Colours.resize(colourFirst < grid ? grid - colourFirst : 0);
  return true;
}

struct FlightPath {
  double DistanceM = 0;
  double ElevationDeg = 0;
};

MetalProductViewAt Flight(Vec3 centre, FlightPath path) {
  return [=](size_t frame) -> std::expected<MetalProductView, std::string> {
    constexpr double radians = std::numbers::pi / 180;
    constexpr double fromDeg = -35;
    constexpr double toDeg = 51;
    const double fraction = static_cast<double>(frame) / (kFrames - 1);
    const double azimuth = std::lerp(fromDeg, toDeg, fraction) * radians;
    const double elevation = path.ElevationDeg * radians;
    const double horizontal = path.DistanceM * std::cos(elevation);
    const Vec3 eye = centre + Vec3{{horizontal * std::sin(azimuth),
                                    path.DistanceM * std::sin(elevation),
                                    horizontal * std::cos(azimuth)}};
    Camera camera;
    camera.PositionM = eye;
    camera.LooksAt = true;
    camera.LookAtM = centre;
    camera.setProjection(Camera::Perspective{.FovDeg = 55, .NearM = 1, .FarM = 240000});
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
           const std::filesystem::path &output) {
  const auto frames = RenderMetalProducts(products, kFrames, viewAt, true);
  if (!frames) {
    std::cerr << frames.error() << '\n';
    return false;
  }
  std::vector<double> gpu;
  std::vector<double> wall;
  size_t triangles = 0;
  for (const auto &sample : frames->Samples) {
    if (!sample.GpuMs) { return false; }
    gpu.push_back(*sample.GpuMs);
    wall.push_back(sample.WallMs);
    triangles = std::max(triangles, sample.GroupTriangles);
  }
  std::ranges::sort(gpu);
  std::ranges::sort(wall);
  const auto p99 = [](const std::vector<double> &values) {
    return values[static_cast<size_t>(std::ceil(0.99 * static_cast<double>(values.size()))) - 1];
  };
  const size_t tileTriangles = products.Tiles.front().Product.Indices.size() / 3;
  const Json report{{"name", name},
                    {"device", frames->Device},
                    {"samples", kFrames},
                    {"gpu_p99_ms", p99(gpu)},
                    {"wall_p99_ms", p99(wall)},
                    {"max_triangles", triangles + tileTriangles},
                    {"uploaded_bytes", frames->UploadedBytes},
                    {"resource_bytes", frames->ResourceBytes}};
  std::cout << report.dump() << '\n';
  const auto png = (output / (std::string(name) + ".png")).string();
  const Extent extent{.WidthPx = MetalProductFrameWidth, .HeightPx = MetalProductFrameHeight};
  return App::SaveImage(frames->Rgba, extent, png).has_value();
}
}

int main(int argc, const char *const *argv) {
  if (argc != 6) {
    std::cerr
        << "metal_groups <capture> <surface> <output directory> <distance m> <elevation deg>\n";
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
  auto ground = *surface;
  if (!RemoveOverlay(ground)) { return 1; }
  const auto view = Flight(group.CentreM, path);
  const MetalSurface full{.Product = *surface, .WorldFromTile = {}};
  const MetalSurface base{.Product = ground, .WorldFromTile = {}};
  const MetalImpostorGroup proxy{.Views = group.Views,
                                 .WorldFromTile = {},
                                 .CentreM = group.CentreM,
                                 .HalfExtentM = group.HalfExtentM};
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
