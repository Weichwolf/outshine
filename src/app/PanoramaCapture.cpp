#include "PanoramaCapture.h"
#include "AzimuthElevation.h"
#include "BenchmarkInput.h"
#include "BenchmarkReport.h"
#include "CivilTime.h"
#include "Ephemeris.h"
#include "FrameBuffer.h"
#include "GpuFrameSample.h"
#include "GroundAtmosphere.h"
#include "Units.h"
#include "WriteFileAtomically.h"
#include <Engine.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace outshine::App {
namespace {
using Json = nlohmann::json;
constexpr size_t kColourChannels = 4;
constexpr int kMaximumSide = 2048;

struct CubeFace {
  std::string_view Name;
  Vec3 Forward;
  Vec3 Up;
};

constexpr std::array kFaces{CubeFace{.Name = "px", .Forward = {{1, 0, 0}}, .Up = {{0, 1, 0}}},
                            CubeFace{.Name = "nx", .Forward = {{-1, 0, 0}}, .Up = {{0, 1, 0}}},
                            CubeFace{.Name = "py", .Forward = {{0, 1, 0}}, .Up = {{0, 0, -1}}},
                            CubeFace{.Name = "ny", .Forward = {{0, -1, 0}}, .Up = {{0, 0, 1}}},
                            CubeFace{.Name = "pz", .Forward = {{0, 0, 1}}, .Up = {{0, 1, 0}}},
                            CubeFace{.Name = "nz", .Forward = {{0, 0, -1}}, .Up = {{0, 1, 0}}}};

Result CaptureFace(Engine &engine, const CubeFace &face, Camera pose) {
  pose.LooksAt = true;
  pose.LookAtM = pose.PositionM + face.Forward;
  pose.UpM = face.Up;
  if (const auto set = engine.setCameraPose(pose); !set) { return set; }
  if (const auto prepared = engine.advance(0.0); !prepared) { return prepared; }
  if (!engine.settled(WorldQuality::Refined)) {
    return std::unexpected("panorama view is incomplete: " +
                           engine.unsettledReasons(WorldQuality::Refined));
  }
  auto renderer = engine.renderer();
  if (const auto drawn = renderer.render({}); !drawn) { return drawn; }
  return renderer.flushAndWait();
}

Result
WriteAttachment(Renderer &renderer, Buffer kind, size_t count, const std::filesystem::path &path) {
  static_assert(std::endian::native == std::endian::little);
  static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
  std::vector<float> values;
  if (const auto read = renderer.readPixels(kind, values); !read) { return read; }
  if (values.size() != count ||
      !std::ranges::all_of(values, [](float value) { return std::isfinite(value); })) {
    return std::unexpected("panorama attachment size or values are invalid");
  }
  return WriteFileAtomically(path.string(), std::as_bytes(std::span(values)));
}

Result WriteFace(Renderer &renderer,
                 const CubeFace &face,
                 size_t pixels,
                 const std::filesystem::path &directory) {
  const std::string basename(face.Name);
  const auto colour = directory / (basename + "-linear.f32");
  const auto depth = directory / (basename + "-depth.f32");
  if (const auto written =
          WriteAttachment(renderer, Buffer::Linear, pixels * kColourChannels, colour);
      !written) {
    return written;
  }
  return WriteAttachment(renderer, Buffer::Depth, pixels, depth);
}

Json CaptureSources(const Loading &loading) {
  auto sources = Json::array();
  for (const auto &source : loading.Sources) {
    sources.push_back({{"kind", source.Kind},
                       {"id", source.Id},
                       {"revision", source.Revision},
                       {"key", source.Key},
                       {"tile_deliveries", source.TileDeliveries}});
  }
  return sources;
}

Json SourceDeclarations(const Scenario::Document &scenario) {
  auto sources = Json::array();
  for (const auto &provider : scenario.Providers) {
    sources.push_back({{"kind", provider.Kind},
                       {"dataset", provider.Dataset},
                       {"revision", provider.Revision},
                       {"schema", provider.Schema},
                       {"payload_sha256", provider.PayloadSha256}});
  }
  return sources;
}

std::optional<Json> CaptureLighting(const Scenario::Document &scenario) {
  const auto &time = scenario.Time;
  int64_t utc = 0;
  if (!time.Declared || time.Live || !ParseIsoUtc(time.Start.c_str(), utc)) { return std::nullopt; }
  const LongitudeLatitude origin{.LongitudeDeg = scenario.Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = scenario.Ground.Origin.LatitudeDeg};
  const auto sun = EarthSunPos(origin, static_cast<double>(utc));
  const auto direction =
      EastUpSouthDirection(sun.AzimuthDeg * kDeg2Rad, sun.ElevationDeg * kDeg2Rad);
  const auto air = Hazed(kEarthAir, scenario.Ground.Sky.Haze);
  const auto reach = GroundAtmosphere{}.Evaluate(air, direction[1]);
  constexpr double scale = kSolarIlluminanceLx / std::numbers::pi;
  const double horizon = std::max(0.0, direction[1]);
  Vec3 direct;
  Vec3 sky;
  Vec3 ground;
  for (size_t channel = 0; channel < direct.Axis.size(); ++channel) {
    direct[channel] = reach.SunTransmittance[channel] * scale * (horizon > 0 ? 1.0 : 0.0);
    const double diffuse = reach.SkyIrradiance[channel] * scale;
    const double ambient = scenario.Lit.IndirectLight[channel];
    sky[channel] = diffuse + ambient;
    ground[channel] = air.GroundAlbedo[channel] * (diffuse + direct[channel] * horizon) + ambient;
  }
  return Json{{"to_sun_eus", direction.Axis},
              {"direct", direct.Axis},
              {"sky", sky.Axis},
              {"ground", ground.Axis}};
}

}

std::expected<void, std::string> CheckPanoramaSource(const Scenario::Document &scenario,
                                                     const ScenarioBenchmarkReport &report) {
  constexpr double kCubeFov = 90;
  const int side = report.Frame.WidthPx;
  const auto view = std::ranges::find(scenario.Views, report.View, &Scenario::View::Id);
  if (side < 2 || side > kMaximumSide || side != report.Frame.HeightPx ||
      !scenario.Ground.Declared || scenario.Render.Antialiasing != "none" ||
      view == scenario.Views.end() || view->Sees.Orthographic || view->Sees.FovDeg != kCubeFov ||
      !std::isfinite(view->Sees.FarM) || view->Sees.NearM <= 0 ||
      view->Sees.FarM <= view->Sees.NearM) {
    return std::unexpected("panorama capture requires a square render target, a declared world, "
                           "90-degree perspective, finite clipping planes and no temporal AA");
  }
  return {};
}

std::expected<std::string, std::string>
CapturePanorama(Engine &engine, const ScenarioBenchmarkReport &report, std::string_view output) {
  const int side = report.Frame.WidthPx;
  const auto &pose = report.Start;
  const auto &world = engine.declaration().Ground;
  const std::filesystem::path directory(output);
  std::error_code error;
  if (!std::filesystem::create_directory(directory, error)) {
    return std::unexpected("panorama capture requires a fresh writable output directory");
  }
  auto renderer = engine.renderer();
  if (const auto reset = renderer.resetMeasurements(kFaces.size()); !reset) {
    return std::unexpected(reset.error());
  }
  const auto began = BenchmarkClock::now();
  const auto pixels = static_cast<size_t>(side) * static_cast<size_t>(side);
  auto faces = Json::array();
  for (const auto &face : kFaces) {
    const auto capturedAt = BenchmarkClock::now();
    if (const auto captured = CaptureFace(engine, face, pose); !captured) {
      return std::unexpected(std::string(face.Name) + ": " + captured.error());
    }
    const double renderMs = BenchmarkElapsedMs(capturedAt);
    const auto saveAt = BenchmarkClock::now();
    if (const auto written = WriteFace(renderer, face, pixels, directory); !written) {
      return std::unexpected(std::string(face.Name) + ": " + written.error());
    }
    faces.push_back({{"name", face.Name},
                     {"forward_eus", face.Forward.Axis},
                     {"up_eus", face.Up.Axis},
                     {"render_and_wait_ms", renderMs},
                     {"attachment_read_and_write_ms", BenchmarkElapsedMs(saveAt)}});
  }
  std::vector<Render::GpuFrameSample> samples;
  if (const auto read = renderer.gpuSamples(samples); !read) {
    return std::unexpected(read.error());
  }
  auto gpu = Json::array();
  for (const auto &sample : samples) { gpu.push_back(sample.ElapsedMs); }
  auto light = CaptureLighting(engine.declaration());
  Json manifest{{"capture_version", 1},
                {"declaration_sha256", report.DeclarationSha256},
                {"origin_lon_lat", {world.Origin.LongitudeDeg, world.Origin.LatitudeDeg}},
                {"eye_eus_m", pose.PositionM.Axis},
                {"face_px", side},
                {"near_m", pose.NearM},
                {"far_m", pose.FarM},
                {"colour", "scene_linear_rgba_f32_le"},
                {"depth", "reversed_perspective_f32_le"},
                {"faces", std::move(faces)},
                {"source_declarations", SourceDeclarations(engine.declaration())},
                {"sources", CaptureSources(engine.loading())},
                {"device", renderer.gpuName()},
                {"setup_ms", report.SetupMs},
                {"preload_ms", report.PreloadMs},
                {"capture_ms", BenchmarkElapsedMs(began)},
                {"gpu_submission_ms", std::move(gpu)}};
  if (light) { manifest["surface_lighting"] = std::move(*light); }
  auto text = manifest.dump(2) + '\n';
  const auto saved =
      WriteFileAtomically((directory / "capture.json").string(), std::as_bytes(std::span(text)));
  if (!saved) { return std::unexpected(saved.error()); }
  return text;
}
}
