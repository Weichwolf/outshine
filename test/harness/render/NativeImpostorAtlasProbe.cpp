#include "Check.h"
#include "ImpostorAtlas.h"
#include "ImpostorSurface.h"
#include "content/GeometryAsset.h"
#include "math/Vec3.h"
#include <Outshine.h>
#include <scenario/Scenario.h>
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kInputLimit = size_t{16} * 1024 * 1024;
constexpr size_t kChannels = 4;
constexpr double kByteSteps = 255;
constexpr double kLightLux = 20000;
constexpr double kElevationDeg = 40;
constexpr std::array<double, 2> kBearings{135, 315};
constexpr double kEyeExtents = 3;
constexpr double kFarExtents = 5;
constexpr size_t kMostRepeats = 16;

std::optional<size_t> Number(std::string_view text) {
  size_t value = 0;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) { return std::nullopt; }
  return value;
}

std::optional<std::vector<uint8_t>> Read(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const auto count = input ? static_cast<std::streamoff>(input.tellg()) : -1;
  const bool bounded = count > 0 && std::cmp_less_equal(count, kInputLimit);
  CHECK(bounded, "the frozen diagnostic input exists within its byte limit");
  if (!bounded) { return std::nullopt; }
  std::vector<uint8_t> bytes(static_cast<size_t>(count));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  CHECK(input.good(), "the complete frozen input is readable");
  if (!input) { return std::nullopt; }
  return bytes;
}

void Save(std::span<const std::byte> bytes, const std::string &path) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(output.good(), "the exact diagnostic product or readback is written");
}

struct SceneChoice {
  size_t View;
  size_t Light;
};

Scenario::Document Scene(const Content::ImpostorAtlas &atlas, SceneChoice choice) {
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = {.WidthPx = atlas.Pixels(), .HeightPx = atlas.Pixels()};
  scene.Render.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  scene.Lit.Declared = true;
  scene.Lit.Key.Lux = kLightLux;
  scene.Lit.Key.BearingDeg = kBearings[choice.Light];
  scene.Lit.Key.ElevationDeg = kElevationDeg;
  Scenario::View camera;
  camera.Id = "atlas";
  camera.Person = "first";
  camera.Placement = Scenario::CameraPlacement::Local;
  const double extent = atlas.HalfExtentM();
  camera.Sees.PositionM =
      atlas.CentreM() + atlas.Views()[choice.View].TowardEye * (kEyeExtents * extent);
  camera.Sees.LooksAt = true;
  camera.Sees.LookAtM = atlas.CentreM();
  camera.Sees.setProjection(Camera::Ortho{
      .XMagM = extent, .YMagM = extent, .NearM = extent, .FarM = kFarExtents * extent});
  scene.Views.push_back(camera);
  return scene;
}

struct Frame {
  std::vector<float> Linear;
  std::vector<float> Depth;
  std::vector<float> Normal;
};

std::optional<Frame> CaptureFrame(const Geometry &card, const Scenario::Document &scene) {
  Engine engine;
  Frame frame;
  const bool captured = engine.setRenderTarget(scene.Render.Frame) && engine.declare(scene) &&
                        engine.setGeometry(card) && engine.assemble() && engine.advance() &&
                        engine.renderer().render({}) &&
                        engine.renderer().readPixels(Buffer::Linear, frame.Linear) &&
                        engine.renderer().readPixels(Buffer::Depth, frame.Depth) &&
                        engine.renderer().readPixels(Buffer::ShadingNormal, frame.Normal);
  CHECK(captured, "the frozen native card yields every requested attachment");
  if (!captured) { return std::nullopt; }
  const size_t pixels = static_cast<size_t>(scene.Render.Frame.WidthPx) *
                        static_cast<size_t>(scene.Render.Frame.HeightPx);
  const bool complete = frame.Depth.size() == pixels && frame.Normal.size() == pixels * kChannels &&
                        frame.Linear.size() == pixels * kChannels;
  CHECK(complete, "every raw attachment covers the complete declared raster");
  if (!complete) { return std::nullopt; }
  return frame;
}

double NormalError(const Frame &frame, const Content::ImpostorAtlas::Texel &source, size_t pixel) {
  Vec3 actual{{frame.Normal[pixel * kChannels],
               frame.Normal[pixel * kChannels + 1],
               frame.Normal[pixel * kChannels + 2]}};
  Vec3 expected{{source.Normal[0], source.Normal[1], source.Normal[2]}};
  if (!Normalise(actual) || !Normalise(expected)) { return 2; }
  const auto delta = actual - expected;
  return std::sqrt(Dot(delta, delta));
}

void CheckFrame(const Content::ImpostorAtlas &atlas,
                size_t view,
                const Frame &frame,
                const std::string &stem) {
  const auto &source = atlas.Views()[view].Texels;
  size_t mismatch = 0;
  size_t worstPixel = 0;
  double worstError = 0;
  for (size_t pixel = 0; pixel < source.size(); ++pixel) {
    mismatch += static_cast<size_t>((frame.Depth[pixel] > 0) != (source[pixel].Surface > 0));
    if (source[pixel].Surface == 0 || frame.Depth[pixel] <= 0) { continue; }
    const double error = NormalError(frame, source[pixel], pixel);
    if (error > worstError) {
      worstError = error;
      worstPixel = pixel;
    }
  }
  std::println("{} coverage_mismatch={} normal_error={} worst_pixel=({},{})",
               stem,
               mismatch,
               worstError,
               worstPixel % static_cast<size_t>(atlas.Pixels()),
               worstPixel / static_cast<size_t>(atlas.Pixels()));
  CHECK(mismatch == 0, "native reconstruction preserves every frozen coverage sample");
  CHECK(worstError <= 4 * std::sqrt(3.0) / kByteSteps,
        "native normals retain the unchanged RGBA8 quantisation bound");
  CHECK(std::ranges::all_of(frame.Linear, [](float x) { return std::isfinite(x); }),
        "the complete frozen-card radiance is finite");
  Save(std::as_bytes(std::span(frame.Depth)), stem + ".depth.f32");
  Save(std::as_bytes(std::span(frame.Normal)), stem + ".normal.f32");
  Save(std::as_bytes(std::span(frame.Linear)), stem + ".linear.f32");
}

void Run(const Content::ImpostorAtlas &atlas,
         const Geometry &card,
         size_t view,
         const std::string &directory,
         size_t repeats) {
  for (size_t repeat = 0; repeat < repeats; ++repeat) {
    for (size_t light = 0; light < kBearings.size(); ++light) {
      const auto frame = CaptureFrame(card, Scene(atlas, {.View = view, .Light = light}));
      if (!frame) { continue; }
      const auto stem =
          directory + "/frame-" + std::to_string(repeat) + "-light-" + std::to_string(light);
      CheckFrame(atlas, view, *frame, stem);
    }
  }
}
}

int main(int argc, char *const *argv) {
  if (argc != 6) {
    std::println(stderr, "usage: probe atlas.crown provenance.txt output-directory view repeats");
    return 2;
  }
  const auto view = Number(argv[4]);
  const auto repeats = Number(argv[5]);
  if (!view || !repeats || *repeats == 0 || *repeats > kMostRepeats) { return 2; }
  const auto bytes = Read(argv[1]);
  const auto producer = Read(argv[2]);
  if (!bytes || !producer) { return Report(); }
  const std::string provenance(producer->begin(), producer->end());
  std::string error;
  const auto atlas = Content::ImpostorAtlas::Decode(*bytes, provenance, error);
  CHECK(atlas.has_value(), "the frozen atlas decodes against its explicit producer identity");
  if (!atlas) {
    std::println(stderr, "{}", error);
    return Report();
  }
  if (*view >= atlas->Views().size()) { return 2; }
  const auto card = Render::BuildImpostorSurface(*atlas, *view);
  CHECK(card.has_value(), "the frozen atlas builds its actual depth-reconstructed native card");
  if (!card) { return Report(); }
  std::error_code ioError;
  std::filesystem::create_directories(argv[3], ioError);
  CHECK(!ioError, "the explicit diagnostic output directory is available");
  if (ioError) { return Report(); }
  const auto encoded = EncodeGeometryAsset(*card, kInputLimit);
  CHECK(encoded.has_value(), "the exact native product encodes for the shader comparison");
  if (!encoded) { return Report(); }
  Save(std::as_bytes(std::span(*encoded)), std::string(argv[3]) + "/card.asset");
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  Run(*atlas, *card, *view, argv[3], *repeats);
  SDL_Quit();
  return Report();
}
