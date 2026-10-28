#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <print>
#include <ratio>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

struct SeparateAttachments {
  std::vector<float> Depth, Normal, Identity, Base, MetalRough;

  bool Read(Render::SceneRenderer &renderer) {
    return renderer.ReadDepth(Depth) == Render::ReadState::Ready &&
           renderer.ReadShadingNormal(Normal) == Render::ReadState::Ready &&
           renderer.ReadSurfaceIdentity(Identity) == Render::ReadState::Ready &&
           renderer.ReadSurfaceBase(Base) == Render::ReadState::Ready &&
           renderer.ReadSurfaceMetalRough(MetalRough) == Render::ReadState::Ready;
  }

  [[nodiscard]] Render::SurfacePixel Pixel(size_t at) const {
    return {.Normal = {{Normal[at * 4], Normal[at * 4 + 1], Normal[at * 4 + 2]}},
            .Depth = Depth[at],
            .Identity = Identity[at * 4],
            .Base = {{Base[at * 4], Base[at * 4 + 1], Base[at * 4 + 2]}},
            .Metalness = MetalRough[at * 2],
            .Roughness = MetalRough[at * 2 + 1]};
  }
};

void MeasureReads(Render::SceneRenderer &renderer,
                  Render::SurfaceReadback &surface,
                  SeparateAttachments &separate) {
  std::array<std::vector<double>, 2> milliseconds;
  double expected = 0;
  const auto measure = [&](bool batch) {
    const auto start = std::chrono::steady_clock::now();
    const bool ready =
        batch ? renderer.ReadSurface(surface) == Render::ReadState::Ready : separate.Read(renderer);
    double checksum = 0;
    if (ready) {
      for (size_t at = 0; at < surface.Pixels(); ++at) {
        const auto value = batch ? surface.Read(at) : separate.Pixel(at);
        checksum += value.Depth + value.Identity + value.Metalness + value.Roughness;
        for (size_t axis = 0; axis < 3; ++axis) {
          checksum += value.Normal[axis] + value.Base[axis];
        }
      }
    }
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    CHECK(ready && (expected == 0 || checksum == expected),
          "measured paths consume the same samples");
    expected = checksum;
    return elapsed;
  };
  for (size_t at = 0; at < 10; ++at) {
    for (size_t order = 0; order < 2; ++order) {
      const size_t kind = (at + order) % 2;
      const double elapsed = measure(kind != 0);
      if (at != 0) { milliseconds[kind].push_back(elapsed); }
    }
  }
  for (auto &samples : milliseconds) { std::ranges::sort(samples); }
  std::println("surface-readback pixels={} samples={} separateMedianMs={:.6f} batchMedianMs={:.6f} "
               "mappedBytes={}",
               surface.Pixels(),
               milliseconds[0].size(),
               milliseconds[0][4],
               milliseconds[1][4],
               surface.StorageBytes());
}

void CheckSurfaceReadback(bool measure) {
  Render::SceneRenderer renderer;
  Render::SurfaceReadback surface;
  CHECK(renderer.ReadSurface(surface) == Render::ReadState::Failed && surface.Pixels() == 0,
        "unprepared capture rejects without replacing previous CPU data");
  Geometry mesh;
  Material material;
  material.DoubleSided = true;
  material.BaseColour = {{0.25f, 0.5f, 0.75f, 1}};
  material.Metalness = 0.3f;
  material.Roughness = 0.7f;
  const auto slot = *mesh.addSurface("surface", material);
  const auto part = *mesh.addPart("triangle", slot);
  CHECK(mesh.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            mesh.setNormals(part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
            mesh.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
        "test geometry is complete");
  Core::Declaration declaration;
  declaration.InitialGeometry = &mesh;
  declaration.SurfaceWidthPx = measure ? 256 : 17;
  declaration.SurfaceHeightPx = measure ? 256 : 13;
  declaration.Stages = {"opaqueGeometry"};
  declaration.Outputs = {"sceneHdr",
                         "sceneDepth",
                         "sceneShadingNormal",
                         "sceneSurfaceIdentity",
                         "sceneSurfaceBase",
                         "sceneSurfaceMetalRough"};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), error.c_str());
  if (!scene) { return; }
  Render::Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = Render::CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  eye.ZNearM = 0.1;
  eye.ZFarM = 10;
  scene->Eye(eye);
  CHECK(scene->Draw(error), "odd-sized surface frame submits");
  CHECK(renderer.ReadSurface(surface) == Render::ReadState::Ready,
        "one batch observes the submitted frame without a separate GPU wait");

  SeparateAttachments separate;
  CHECK(separate.Read(renderer), "existing attachment readers remain available");
  const size_t pixels =
      static_cast<size_t>(declaration.SurfaceWidthPx) * declaration.SurfaceHeightPx;
  CHECK(surface.Pixels() == pixels &&
            std::ranges::any_of(separate.Depth, [](float value) { return value > 0; }) &&
            std::ranges::any_of(separate.Depth, [](float value) { return value == 0; }),
        "capture contains both a real surface and background");
  for (size_t at = 0; at < surface.Pixels(); ++at) {
    const auto sample = surface.Read(at);
    CHECK(sample.Depth == separate.Depth[at] && sample.Identity == separate.Identity[at * 4] &&
              sample.Metalness == separate.MetalRough[at * 2] &&
              sample.Roughness == separate.MetalRough[at * 2 + 1],
          "depth, identity, metalness and roughness match the separate path exactly");
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK(sample.Normal[axis] == separate.Normal[at * 4 + axis] &&
                sample.Base[axis] == separate.Base[at * 4 + axis],
            "every consumed normal and base component matches the separate path exactly");
    }
  }
  CHECK(renderer.ReadSurface(surface) == Render::ReadState::Ready && surface.Pixels() == pixels,
        "reusing a snapshot releases its previous mapping and storage");
  if (measure) { MeasureReads(renderer, surface, separate); }
}
}

int main(int argc, char *const *argv) {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  CheckSurfaceReadback(argc == 2 && std::strcmp(argv[1], "--measure") == 0);
  SDL_Quit();
  return Report();
}
