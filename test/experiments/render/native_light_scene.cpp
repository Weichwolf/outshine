#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "GpuSubmission.h"
#include "SubjectTypes.h"
#include "scene/Geometry.h"
#include "scene/Material.h"
#include "math/Units.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
constexpr uint32_t kWidth = 64, kHeight = 64;
constexpr size_t kValues = size_t{kWidth} * kHeight * 4;
constexpr double kMaximumRelative = .02, kRmsRelative = .005;
constexpr size_t kTailCount = kMaxSubjectLights + 1;
constexpr double kBoundaryOffsetM = .0000001, kNearM = .1, kFarM = 1000;
constexpr double kSunElevationDeg = 90;
constexpr float kTailCandela = 1000, kOverlapRangeM = 1000, kFiniteRangeM = 8;
constexpr float kColourStep = .25f, kBlue = .75f, kSunLux = .2f;
constexpr size_t kGlobalStride = 257, kDirectionalStride = 1024;
constexpr double kGridHalf = 15.5;
constexpr float kRoughness = .4f, kCoat = .8f, kCoatRoughness = .2f;
constexpr Vec4f kBaseColour{{.3f, .2f, .1f, 1}};
constexpr Vec3f kSheenColour{{.2f, .3f, .4f}}, kTransmission{{.4f, .5f, .6f}};
constexpr float kSheenRoughness = .6f, kAnisotropy = .8f;
enum class Population { Mixed, Tail, Overlap };
constexpr int64_t kPositionWidthM = 128;
constexpr double kReceiverX =
    static_cast<double>(kPositionWidthM - static_cast<int64_t>(kWgs84A) % kPositionWidthM);

bool Accepted(bool accepted, const std::string &error) {
  if (!accepted) { std::println(stderr, "{}", error); }
  return accepted;
}

struct Fixture {
  SceneRenderer Renderer{GpuSubmission{.Create = [](void *, Native::DeviceOptions options) {
    options.Validation = true;
    return Native::CreateDevice(options);
  }}};
  std::unique_ptr<Core::RuntimeScene> Scene;
  double CentreX = 0;

  bool Open(Material material, double centreX) {
    CentreX = centreX;
    Geometry geometry;
    if (material.Anisotropy > 0) {
      const auto normal = geometry.addImage(1, 1, std::array<uint8_t, 4>{128, 128, 255, 255});
      if (!normal) { return false; }
      material.NormalMap.Image = *normal;
    }
    const auto surface = geometry.addSurface("independent light receiver", material);
    if (!surface) { return false; }
    const auto part = geometry.addPart("receiver", *surface);
    if (!part) { return false; }
    const auto x = static_cast<float>(centreX);
    const std::array<float, 12> positions{x - 2, 0, -2, x + 2, 0, -2, x + 2, 0, 2, x - 2, 0, 2};
    if (!geometry.setPositions(*part, positions) ||
        !geometry.setNormals(*part, std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}) ||
        !geometry.setTexture(*part, std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1}) ||
        !geometry.setTangents(
            *part, std::array<float, 16>{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1}) ||
        !geometry.setTriangles(*part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2})) {
      return false;
    }
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry;
    declaration.SurfaceWidthPx = kWidth;
    declaration.SurfaceHeightPx = kHeight;
    declaration.Outputs = {"surface", "sceneLinear"};
    declaration.KeyLux = 1;
    declaration.KeyElevationDeg = kSunElevationDeg;
    std::string error;
    if (!Accepted(Core::RuntimeScene::Open(Renderer, declaration, nullptr, Scene, error), error)) {
      return false;
    }
    Renderer.CastsBelow(0);
    return Camera(false);
  }

  bool Camera(bool orthographic) {
    auto eye = Viewpoint::LookAt(
        {.EyeM = {{CentreX - kBoundaryOffsetM, 5, 4}}, .AimM = {{CentreX, 0, 0}}}, 0.0);
    if (!eye) { return false; }
    eye->YfovRad = 1;
    eye->ZNearM = kNearM;
    eye->ZFarM = kFarM;
    if (orthographic) {
      eye->Kind = CameraKind::Orthographic;
      eye->XMagM = eye->YMagM = 2;
      eye->ZNearM = kNearM;
      eye->ZFarM = kFarM;
    }
    Scene->Eye(*eye);
    std::string error;
    return Accepted(Scene->Draw(error), error);
  }

  std::expected<std::vector<float>, std::string> Capture(std::span<const SubjectLight> lights) {
    std::string error;
    if (!Renderer.SetSubjectLights(lights, error)) { return std::unexpected(error); }
    const auto drawn = Renderer.RenderFrame();
    if (!drawn) { return std::unexpected(drawn.error()); }
    Renderer.WaitForGpu();
    std::vector<float> pixels;
    if (Renderer.ReadSceneLinear(pixels) != ReadState::Ready || pixels.size() != kValues) {
      return std::unexpected("complete linear frame is unavailable");
    }
    return pixels;
  }
};

std::vector<SubjectLight> Sources(double x, Population population, size_t count) {
  const bool tail = population == Population::Tail;
  const bool overlap = population == Population::Overlap;
  std::vector<SubjectLight> result(count);
  for (size_t id = 0; id < count; ++id) {
    auto &source = result[id];
    source.Light.Kind = id % 3 == 0 ? LightKind::Spot : LightKind::Point;
    source.Light.Direction = {{0, -1, 0}};
    source.Light.RangeM = overlap ? kOverlapRangeM : kFiniteRangeM;
    source.Light.Intensity = tail ? 0 : 2;
    source.Light.Colour = {{kColourStep + static_cast<float>(id % 3) * kColourStep, .5f, kBlue}};
    source.PositionEcefM = {{kWgs84A + x + (static_cast<double>(id % 32) - kGridHalf) * 4,
                             2 + static_cast<double>((id / 32) % 8) * 2,
                             (static_cast<double>(id >> 8) - kGridHalf) * 4}};
    if (id % kGlobalStride == 0) { source.Light.RangeM = 0; }
    if (id % kDirectionalStride == 1) {
      source.Light.Kind = LightKind::Directional;
      source.Light.Intensity = tail ? 0 : kSunLux;
    }
  }
  result.back().Light.Kind = LightKind::Point;
  result.back().Light.Intensity = kTailCandela;
  result.back().PositionEcefM = {{kWgs84A + x, 2, 0}};
  return result;
}

bool Save(const std::filesystem::path &path, std::span<const float> pixels) {
  std::ofstream stream(path, std::ios::binary);
  const auto bytes = std::as_bytes(pixels);
  stream.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return stream.good();
}

bool Compare(Fixture &fixture,
             const std::filesystem::path &output,
             const std::string &name,
             std::span<const SubjectLight> lights) {
  auto actual = fixture.Capture(lights);
  if (!actual) { return Accepted(false, actual.error()); }
  std::vector<double> reference(kValues);
  for (size_t first = 0; first < lights.size(); first += kMaxSubjectLights) {
    const auto group = lights.subspan(first, std::min(kMaxSubjectLights, lights.size() - first));
    const auto pixels = fixture.Capture(group);
    if (!pixels) { return Accepted(false, pixels.error()); }
    for (size_t at = 0; at < kValues; ++at) {
      if (at % 4 != 3) { reference[at] += (*pixels)[at]; }
    }
  }
  double maximum = 0;
  double squares = 0;
  double signal = 0;
  size_t errors = 0;
  std::vector<float> expected(kValues);
  for (size_t at = 0; at < kValues; ++at) {
    expected[at] = static_cast<float>(reference[at]);
    if (at % 4 == 3) { continue; }
    const double difference = (*actual)[at] - reference[at];
    const double relative = std::abs(difference) / std::max(1e-5, std::abs(reference[at]));
    maximum = std::max(maximum, relative);
    squares += difference * difference;
    signal += reference[at] * reference[at];
    errors += !std::isfinite(relative) || relative > kMaximumRelative ? 1 : 0;
  }
  const double rms = std::sqrt(squares / std::max(1e-10, signal));
  const bool saved = Save(output / (name + ".actual.f32"), *actual) &&
                     Save(output / (name + ".reference.f32"), expected);
  const bool passed = errors == 0 && rms <= kRmsRelative && signal > 1 && saved;
  std::println("{{\"case\":\"{}\",\"count\":{},\"max_relative\":{},\"rms_relative\":{},"
               "\"bad_rgb\":{},\"signal_square\":{},\"passed\":{}}}",
               name,
               lights.size(),
               maximum,
               rms,
               errors,
               signal,
               passed);
  std::fflush(stdout);
  return passed;
}

Material Surface(size_t family) {
  Material material;
  material.Roughness = kRoughness;
  material.BaseColour = kBaseColour;
  material.DoubleSided = true;
  if (family == 1) {
    material.Clearcoat = kCoat;
    material.ClearcoatRoughness = kCoatRoughness;
  }
  if (family == 2) {
    material.SheenColour = kSheenColour;
    material.SheenRoughness = kSheenRoughness;
  }
  if (family == 3) { material.Iridescence = 1; }
  if (family == 4) { material.Anisotropy = kAnisotropy; }
  if (family == 5) {
    material.Scattering = ScatteringModel::DiffuseThinSheet;
    material.DiffuseTransmission = kTransmission;
  }
  if (family == 6) {
    material.Alpha = AlphaMode::Blended;
    material.BaseColour[3] = .5f;
  }
  if (family == 7) { material.Transmission = .5f; }
  return material;
}
}

int main(int argc, char **argv) {
  if (argc != 2 || !std::filesystem::is_directory(argv[1])) { return 2; }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::println(stderr, "{}", SDL_GetError());
    return 2;
  }
  bool passed = true;
  for (size_t family = 0; family < 8; ++family) {
    Fixture fixture;
    if (!fixture.Open(Surface(family), kReceiverX)) {
      passed = false;
      break;
    }
    const auto prefix = "material-" + std::to_string(family);
    passed = Compare(fixture,
                     argv[1],
                     prefix + "-tail17",
                     Sources(kReceiverX, Population::Tail, kTailCount)) &&
             passed;
    if (!fixture.Camera(true)) {
      passed = false;
      break;
    }
    auto back = Sources(kReceiverX, Population::Tail, kTailCount);
    if (family == 5) { back.back().PositionEcefM[1] = -2; }
    passed = Compare(fixture, argv[1], prefix + "-ortho17", back) && passed;
    if (family == 0) {
      for (const size_t count : {size_t{1024}, size_t{8192}}) {
        passed = Compare(fixture,
                         argv[1],
                         "mixed-" + std::to_string(count),
                         Sources(kReceiverX, Population::Mixed, count)) &&
                 passed;
      }
      passed =
          Compare(
              fixture, argv[1], "overlap-8192", Sources(kReceiverX, Population::Overlap, 8192)) &&
          passed;
    }
  }
  SDL_Quit();
  return passed ? 0 : 1;
}
