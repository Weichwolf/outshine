#include "RuntimeScene.h"
#include "WorldCandidate.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  for (const bool sky : {false, true}) {
    for (const double farM : {0.0, 1000.0}) {
      auto eye = *Render::Viewpoint::LookAt({.EyeM = {{0, 250, 0}}, .AimM = {{-50, 262, 86}}},
                                            Vec3{{0, 1, 0}});
      eye.YfovRad = 1;
      eye.ZNearM = 0.05;
      eye.ZFarM = farM;
      const Vec3 centreM = eye.EyeM + eye.Forward * 100.0;
      const std::array corners{centreM - eye.Right * 20.0 - eye.Up * 20.0,
                               centreM + eye.Right * 20.0 - eye.Up * 20.0,
                               centreM + eye.Right * 20.0 + eye.Up * 20.0,
                               centreM - eye.Right * 20.0 + eye.Up * 20.0};
      std::array<float, 12> positions{};
      std::array<float, 12> normals{};
      for (size_t corner = 0; corner < corners.size(); ++corner) {
        for (size_t axis = 0; axis < 3; ++axis) {
          positions[corner * 3 + axis] = static_cast<float>(corners[corner][axis]);
          normals[corner * 3 + axis] = static_cast<float>(-eye.Forward[axis]);
        }
      }
      Geometry geometry;
      Material material;
      material.BaseColour = {{1, 0, 0, 1}};
      const int part =
          geometry.addPart("red wall", geometry.addSurface("red", material).value()).value();
      CHECK(geometry.setPositions(part, positions) && geometry.setNormals(part, normals) &&
                geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3}),
            "wall geometry is valid");
      Render::SceneRenderer renderer;
      Core::Declaration declaration;
      declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
      declaration.Outputs = {"sceneLinear", "sceneDepth"};
      declaration.DrawsSky = sky;
      declaration.KeyLux = 10;
      declaration.KeyElevationDeg = 45;
      declaration.Haze = 0;
      std::unique_ptr<Core::RuntimeScene> scene;
      std::string error;
      if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) {
        Unprepared(error.c_str());
        return Report();
      }
      Core::WorldCandidate candidate(renderer);
      CHECK(candidate.Prepare(*scene, nullptr).has_value() &&
                candidate.SetGeometry(std::move(geometry), 0, material, error) &&
                candidate.Publish(scene).has_value(),
            "geometry publishes into the initially empty world");
      scene->Eye(eye);
      CHECK(scene->Advance(error) && scene->Draw(error), "perspective wall renders");
      std::vector<float> depth;
      std::vector<float> colour;
      CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready && depth.size() == 32u * 32u,
            "depth is readable");
      CHECK(renderer.ReadSceneLinear(colour) == Render::ReadState::Ready &&
                colour.size() == 32u * 32u * 4u,
            "linear colour is readable");
      if (depth.size() == 32u * 32u && colour.size() == depth.size() * 4u) {
        const size_t centre = 16u * 32u + 16u;
        std::printf("sky %d far %.0f depth %.9g rgb %.9g %.9g %.9g\n",
                    sky,
                    farM,
                    depth[centre],
                    colour[centre * 4u],
                    colour[centre * 4u + 1u],
                    colour[centre * 4u + 2u]);
        const double expectedDepth =
            farM == 0 ? 0.05 / 100 : (0.05 / 100 - 0.05 / farM) / (1 - 0.05 / farM);
        CHECK_NEAR(depth[centre],
                   expectedDepth,
                   1e-7,
                   "reverse depth",
                   "the wall retains physical depth with and without sky");
        CHECK(colour[centre * 4u] > colour[centre * 4u + 1u] + 0.5f,
              "the nearby red wall remains visible through the atmosphere");
      }
    }
  }
  SDL_Quit();
  return Report();
}
