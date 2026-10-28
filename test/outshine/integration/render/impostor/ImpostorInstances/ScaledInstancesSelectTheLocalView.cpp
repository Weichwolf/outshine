#include "ImpostorInstances.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

Geometry Surface(bool top) {
  Geometry mesh;
  Material material;
  material.Unlit = true;
  material.DoubleSided = true;
  material.BaseColour = top ? Vec4f{{0, 1, 0, 1}} : Vec4f{{1, 0, 0, 1}};
  const auto slot = *mesh.addSurface("view", material);
  const auto part = *mesh.addPart("card", slot);
  const std::array<float, 12> points =
      top ? std::array<float,
                       12>{-0.005f, 0, -0.5f, -0.005f, 0, 0.5f, 0.005f, 0, 0.5f, 0.005f, 0, -0.5f}
          : std::array<float, 12>{0, -0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0.5f, 0.5f, 0, -0.5f, 0.5f};
  const std::array<float, 12> normals =
      top ? std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}
          : std::array<float, 12>{1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0};
  CHECK(mesh.setPositions(part, points) && mesh.setNormals(part, normals) &&
            mesh.setTexture(part, std::array<float, 8>{0, 0, 1, 0, 1, 1, 0, 1}) &&
            mesh.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
        "independent coloured view cards are complete");
  return mesh;
}

size_t Green(Render::SceneRenderer &renderer) {
  CHECK(renderer.RenderFrame(), "native instance frame submits");
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == Render::ReadState::Ready,
        "native scene colours read back");
  size_t green = 0;
  for (size_t at = 0; at + 3 < pixels.size(); at += 4) {
    if (pixels[at + 1] > 0.9f && pixels[at] < 0.1f) { ++green; }
  }
  return green;
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    const auto plan = Render::ExecutionPlan::Compile(
        {.Outputs = {Render::Resource::SceneLinear, Render::Resource::SceneDepth},
         .Content = {Render::Stage::OpaqueGeometry},
         .Exposure = {},
         .Display = {},
         .Precision = {}});
    CHECK(plan && renderer.Init({.WidthPx = 64, .HeightPx = 64}, *plan), "native framegraph opens");
    if (!plan) { return Report(); }
    ImpostorCards cards;
    cards.HalfExtentM = 1;
    cards.Views.push_back({.Direction = {{1, 0, 0}}, .Surface = Surface(false)});
    cards.Views.push_back({.Direction = {{0, 1, 0}}, .Surface = Surface(true)});
    std::string error;
    auto instances = Render::ImpostorInstances::Create(renderer, cards, 1, error);
    CHECK(instances, error.c_str());
    if (instances) {
      const Vec3 eye{{1, 10, 0}};
      auto camera = Render::Viewpoint::LookAt({.EyeM = eye, .AimM = {}}, Vec3{{0, 0, 1}});
      CHECK(camera, "camera looks predominantly along the world Y axis");
      if (camera) {
        camera->Kind = Render::CameraKind::Orthographic;
        camera->XMagM = camera->YMagM = 1;
        camera->ZNearM = 0.1;
        camera->ZFarM = 100;
        const auto lens = Render::Lens::From(*camera, 64, 64);
        CHECK(lens, "camera projection is physical");
        if (lens) {
          renderer.SetCamera(*camera, *lens);
          Mat4 model;
          model[0] = 100;
          CHECK(instances->Update(std::span(&model, 1), eye, error), error.c_str());
          CHECK(Green(renderer) > 500,
                "inverse local eye direction selects the top card despite a hundredfold X scale");
          model[0] = 0;
          CHECK(!instances->Update(std::span(&model, 1), eye, error),
                "singular model cannot replace the accepted views");
          CHECK(Green(renderer) > 500,
                "rejected transforms preserve the accepted native instances");
          model[0] = -100;
          CHECK(instances->Update(std::span(&model, 1), eye, error),
                "reflected model is invertible");
          CHECK(Green(renderer) > 500, "reflection preserves selection by the local eye direction");
          CHECK(instances->Update({}, eye, error) && Green(renderer) == 0,
                "an empty instance set clears every view");
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
