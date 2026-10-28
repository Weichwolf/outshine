#include "test/harness/render/NativeGpuCheck.h"
#include <cstdint>
#include "SceneRenderer.h"
#include "MaterialFieldPass.h"
#include "test/harness/render/NativeMaterialField.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

bool Publish(SceneRenderer &renderer) {
  const auto plan =
      ExecutionPlan::Compile({.Outputs = {Resource::SceneLinear, Resource::SceneDepth},
                              .Content = {Stage::OpaqueGeometry},
                              .Exposure = {},
                              .Display = {},
                              .Precision = {}});
  CHECK(plan, "the main frame owns its existing opaque render plan");
  if (!plan) { return false; }
  std::string error;
  const bool began = renderer.BeginsWorldCandidate(error);
  CHECK(began, "the main geometry is prepared as a candidate");
  if (!began) { return false; }
  {
    const auto edit = renderer.EditWorldCandidate();
    const auto initialized = renderer.Init({.WidthPx = 32, .HeightPx = 32}, *plan);
    CHECK(initialized, "the real main render targets initialize");
    if (!initialized) { return false; }
    MaterialBinding material;
    material.Row.Unlit = material.Row.DoubleSided = true;
    material.Row.BaseColour = {{.125f, .25f, .75f, 1}};
    if (!renderer.SetGeometryMaterials(std::span(&material, 1), error)) {
      CHECK(false, error.c_str());
      return false;
    }
    constexpr std::array<uint32_t, 1> slots{0};
    renderer.SetNativePieceSurfaces(slots);
    const std::array vertices{StoredVertex::Of({{-.6f, -.6f, -2}}, {{0, 0}}, {{0, 0, 1}}),
                              StoredVertex::Of({{.6f, -.6f, -2}}, {{1, 0}}, {{0, 0, 1}}),
                              StoredVertex::Of({{0, .6f, -2}}, {{.5f, 1}}, {{0, 0, 1}})};
    constexpr std::array<uint32_t, 3> indices{0, 1, 2};
    const std::array<DagCluster, 1> clusters{DagCluster{.SelfCenter = {{0, 0, -2}},
                                                        .SelfRadius = 1,
                                                        .ParentCenter = {{0, 0, -2}},
                                                        .ParentRadius = 1,
                                                        .ParentErr = kDagRootErr,
                                                        .Count = 3}};
    const std::array<Mat4, 1> rows{};
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Clusters = clusters;
    piece.Instances = rows;
    CHECK(renderer.PlacePiece(piece, error) != kNoPiece,
          "the main frame owns actual resident geometry");
  }
  CHECK(renderer.PublishesWorldCandidate(error), "the complete main candidate is published");
  Viewpoint camera;
  camera.YfovRad = 1;
  camera.ZNearM = .1;
  camera.ZFarM = 100;
  const auto lens = Lens::From(camera, 32, 32);
  CHECK(lens, "the main frame has a valid camera");
  if (!lens) { return false; }
  renderer.SetCamera(camera, *lens);
  CHECK(renderer.RenderFrame(), "the actual main frame submits");
  renderer.WaitForGpu();
  return true;
}

void Exercise() {
  const Native::Device *device = nullptr;
  SceneRenderer renderer({.Context = static_cast<void *>(&device),
                          .Create = [](void *context, Native::DeviceOptions options) {
                            options.Validation = true;
                            auto made = Native::CreateDevice(options);
                            if (made) {
                              *static_cast<const Native::Device **>(context) = made->get();
                            }
                            return made;
                          }});
  const auto offline = renderer.OfflineUploads(std::chrono::seconds(5));
  if (!Publish(renderer)) { return; }
  std::vector<float> before;
  CHECK(renderer.ReadSceneLinear(before) == ReadState::Ready &&
            std::ranges::any_of(before, [](float value) { return value > 0; }),
        "the published main frame has actual visible source pixels");
  const auto camera = renderer.LastSubmittedCamera();
  const auto *plan = &renderer.Plan();
  Require(renderer.ConfigureMaterialFields());
  auto input = PreparedMaterialField({.8f, .2f, .1f, 1});
  auto source = Require(
      RetryOffline([&] { return renderer.PrepareMaterialField(input); }, std::chrono::seconds(5)));
  std::array<SceneLight, 1> lights{};
  lights[0].Light.Intensity = 120;
  MaterialFieldView view;
  view.Camera.Kind = CameraKind::Orthographic;
  view.Camera.EyeM = {{0, 0, 2}};
  view.Camera.XMagM = view.Camera.YMagM = 1;
  view.Camera.ZNearM = .1;
  view.Camera.ZFarM = 10;
  view.Lights = lights;
  view.Target = {.WidthPx = 16, .HeightPx = 16};
  auto images = Require(RetryOffline([&] { return renderer.RelightMaterialField(*source, view); },
                                     std::chrono::seconds(5)));
  CHECK(images.Filtered->Device == device && images.Unfiltered->Device == device,
        "the isolated pass uses the real main render device");
  source.reset();
  input.reset();
  images = {};
  renderer.WaitForGpu();
  std::vector<float> after;
  CHECK(renderer.ReadSceneLinear(after) == ReadState::Ready && after == before &&
            &renderer.Plan() == plan && !renderer.HasWorldCandidate() && renderer.Drew(),
        "isolated field work preserves exact published pixels, plan and candidate state");
  const auto unchanged = renderer.LastSubmittedCamera();
  CHECK(unchanged.Serial == camera.Serial && unchanged.Basis.EyeM == camera.Basis.EyeM &&
            unchanged.Basis.Forward == camera.Basis.Forward,
        "field relighting neither publishes nor advances main camera history");
  CHECK(renderer.RenderFrame(),
        "the next real main frame remains renderable after field retirement");
  renderer.WaitForGpu();
  std::vector<float> next;
  CHECK(renderer.ReadSceneLinear(next) == ReadState::Ready && next == before,
        "the following actual main frame retains its original geometry and material image");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native frames");
  Exercise();
  SDL_Quit();
  return Report();
}
