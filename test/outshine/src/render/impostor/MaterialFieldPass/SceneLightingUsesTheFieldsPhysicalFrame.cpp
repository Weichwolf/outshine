#include "SceneRenderer.h"
#include "SceneMaterialFieldView.h"
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeImageReadback.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

std::vector<float> Pixels(const RadianceImage &image) {
  const auto bytes = ReadImage(*image.Device, *image.Texture, {.Width = 16, .Height = 16});
  std::vector<float> pixels(size_t{16} * 16 * 4);
  CHECK(SDL_ConvertPixels(16,
                          16,
                          SDL_PIXELFORMAT_RGBA64_FLOAT,
                          bytes.Rows.data(),
                          static_cast<int>(bytes.RowBytes),
                          SDL_PIXELFORMAT_RGBA128_FLOAT,
                          pixels.data(),
                          16 * 16),
        "native HDR pixels decode without row padding");
  return pixels;
}

struct Fixture {
  SceneRenderer Renderer{GpuSubmission{.Create = [](void *, Native::DeviceOptions options) {
    options.Validation = true;
    return Native::CreateDevice(options);
  }}};
  std::shared_ptr<const ResidentMaterialField> Source;
  std::array<SceneLight, 2> Lights;
  EnvironmentLighting Environment;
  MaterialFieldView Manual;
  SceneMaterialFieldView Scene;
  Viewpoint Observer;

  void Open() {
    const auto plan =
        ExecutionPlan::Compile({.Outputs = {Resource::SceneLinear, Resource::EnvironmentSpecular},
                                .Content = {Stage::OpaqueGeometry},
                                .Exposure = {},
                                .Display = {},
                                .Precision = {}});
    CHECK(plan, "the actual world plan includes its reflection products");
    if (!plan) { Failed(); }
    CHECK(Renderer.Init({.WidthPx = 16, .HeightPx = 16}, *plan), "the native scene opens");
    Require(Renderer.ConfigureMaterialFields());
    const auto input = MaterialFieldInput::Prepare(MaterialFieldQuad({.6f, .4f, .2f, 1}),
                                                   {.WidthPx = 16, .HeightPx = 16});
    CHECK(input, "the source holds physical lit geometry");
    if (!input) { Failed(); }
    Source = Require(RetryOffline([&] { return Renderer.PrepareMaterialField(*input); },
                                  std::chrono::seconds(5)));
    Scene.Capture.Kind = CameraKind::Orthographic;
    Scene.Capture.EyeM = {{0, 0, 2}};
    Scene.Capture.XMagM = Scene.Capture.YMagM = 1;
    Scene.Capture.ZNearM = .1;
    Scene.Capture.ZFarM = 10;
    Scene.Target = {.WidthPx = 16, .HeightPx = 16};
    Scene.Encoding = RadianceEncoding::Texels;
    Scene.SceneFromField.Column = {
        0, 1, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1048576, -2097152, 4194304, 1};
    Lights[0].Light.Direction = {{0, 0, -1}};
    Lights[0].Light.Intensity = 20;
    Lights[1].Light.Kind = LightKind::Point;
    Lights[1].Light.Intensity = 100;
    Lights[1].PositionM = {{1, 2, 4}};
    Environment.RadianceLinear = {{.2, .3, .4}};
    Environment.GroundLinear = {{.05, .1, .15}};
    Environment.SkyLux = 1200;
    Manual.Camera = Scene.Capture;
    Manual.Target = Scene.Target;
    Manual.Lights = Lights;
    Manual.Environment = Environment;
    Observer.EyeM = {{0, 0, 3}};
    Observer.YfovRad = 1;
    Observer.ZNearM = .1;
    Observer.ZFarM = 100;
    Bind();
  }

  void Bind() {
    auto placed = Lights;
    for (auto &light : placed) {
      light.PositionM = Scene.SceneFromField.TransformPoint(light.PositionM);
      const auto direction = light.Light.Direction;
      light.Light.Direction = {{direction[2], direction[0], direction[1]}};
    }
    std::string error;
    CHECK(Renderer.SetGeometryLights(placed, error), "the main scene owns its real lights");
    auto environment = Environment;
    environment.UpUnit = {{0, 0, 1}};
    Renderer.SetGeometryEnvironment(environment);
    auto watching = Observer;
    watching.EyeM = Scene.SceneFromField.TransformPoint(Observer.EyeM);
    watching.Right = {{0, 1, 0}};
    watching.Up = {{0, 0, 1}};
    watching.Forward = {{-1, 0, 0}};
    const auto lens = Lens::From(watching, 16, 16);
    CHECK(lens, "the main camera has physical scene coordinates");
    if (!lens) { Failed(); }
    Renderer.SetCamera(watching, *lens);
    Manual.LightingCamera = Observer;
  }

  MaterialFieldImages Relight() {
    return Require(RetryOffline([&] { return Renderer.RelightSceneMaterialField(*Source, Scene); },
                                std::chrono::seconds(5)));
  }
};

void Compare(Fixture &fixture) {
  const auto expected = Require(RetryOffline(
      [&] { return fixture.Renderer.RelightMaterialField(*fixture.Source, fixture.Manual); },
      std::chrono::seconds(5)));
  const auto actual = fixture.Relight();
  CHECK(Pixels(*actual.Filtered) == Pixels(*expected.Filtered),
        "scene point/directional lights, environment and observer map to the physical field frame");
  CHECK(actual.LightingCamera.EyeM == fixture.Observer.EyeM &&
            actual.LightingCamera.Kind == fixture.Observer.Kind &&
            actual.Camera.EyeM == fixture.Scene.Capture.EyeM,
        "large scene translations leave the capture camera and lighting observer separate");
}

void Exercise() {
  Fixture fixture;
  const auto offline = fixture.Renderer.OfflineUploads(std::chrono::seconds(5));
  fixture.Open();
  Compare(fixture);
  fixture.Observer.Kind = CameraKind::Orthographic;
  fixture.Observer.XMagM = fixture.Observer.YMagM = 1;
  fixture.Bind();
  Compare(fixture);
  auto rejected = fixture.Scene;
  rejected.SceneFromField[0] = 2;
  CHECK(!fixture.Renderer.RelightSceneMaterialField(*fixture.Source, rejected),
        "scale and shear cannot change physical lighting units");
  rejected = fixture.Scene;
  rejected.SceneFromField[15] = 0;
  CHECK(!fixture.Renderer.RelightSceneMaterialField(*fixture.Source, rejected),
        "a projective or singular field frame records no relighting");
  rejected = fixture.Scene;
  rejected.SceneFromField[12] = std::numeric_limits<double>::infinity();
  CHECK(!fixture.Renderer.RelightSceneMaterialField(*fixture.Source, rejected),
        "a nonfinite scene field is rejected before GPU use");
  const auto withoutSky = Pixels(*fixture.Relight().Filtered);
  fixture.Renderer.SetMedium(kEarthAir);
  fixture.Renderer.SetSky(
      {.ToSun = {{0, .8f, .6f}}, .Up = {{0, 0, 1}}, .IlluminanceLux = 1200, .EyeHeightM = 200});
  const auto pending = fixture.Renderer.RelightSceneMaterialField(*fixture.Source, fixture.Scene);
  CHECK(!pending && pending.error().Kind == Native::ErrorKind::Busy,
        "unsubmitted scene reflections defer field work without blocking");
  const auto prepared = fixture.Renderer.PrepareWorldResources();
  CHECK(prepared, "the existing preparation path submits the real environment atlas");
  if (!prepared) { return; }
  const auto reflected = fixture.Relight();
  CHECK(Pixels(*reflected.Filtered) != withoutSky,
        "the actual scene reflection atlas contributes to field shading");
  fixture.Renderer.WaitForGpu();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native scene fields initialize video");
  Exercise();
  SDL_Quit();
  return Report();
}
