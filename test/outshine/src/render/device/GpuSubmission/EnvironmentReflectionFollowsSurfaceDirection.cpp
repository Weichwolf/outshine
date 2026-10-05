#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "EnvironmentSpecularLayout.h"
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

Vec3f ReflectedColour(Vec3f up, bool upward, float roughness) {
  Geometry geometry;
  Material material;
  material.DoubleSided = true;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Metalness = 1;
  material.Roughness = roughness;
  const auto surface = geometry.addSurface("mirror", material);
  CHECK(surface, "the material has no diffuse or emissive source");
  if (!surface) { return {}; }
  const auto part = geometry.addPart("plane", *surface);
  CHECK(part, "the reflection surface is native geometry");
  if (!part) { return {}; }
  CHECK(geometry.setPositions(*part, std::array<float, 9>{-4, -4, -2, 4, -4, -2, 0, 4, -2}),
        "the surface covers the measured pixel");
  const float side = upward ? 1.0f : -1.0f;
  Vec3f normal{{side * up[0], side * up[1], 1}};
  CHECK(Normalise(normal), "the shading normal is finite and normalized");
  std::array<float, 9> normals{};
  for (size_t at = 0; at < 3; ++at) {
    for (size_t axis = 0; axis < 3; ++axis) { normals[at * 3 + axis] = normal[axis]; }
  }
  CHECK(geometry.setNormals(*part, normals), "the declared normal determines reflection direction");
  CHECK(geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "the plane is triangulated");
  SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "sceneLinear"};
  declaration.Antialiasing = "none";
  declaration.IndirectLight = {1, 1, 1};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "the first-frame reflection scene opens");
  if (!scene) { return {}; }
  Viewpoint eye;
  eye.YfovRad = 1;
  eye.ZNearM = 0.1;
  eye.ZFarM = 100;
  scene->Eye(eye);
  CHECK(scene->Advance(error),
        "the scenario binds its camera and native world origin before lighting");
  renderer.SetMedium(kEarthAir);
  renderer.SetSky({.ToSun = up, .Up = up, .IlluminanceLux = 1, .EyeHeightM = 0});
  SubjectEnvironment environment;
  environment.UpUnit = {{up[0], up[1], up[2]}};
  environment.GroundAlbedo = {{0, 0, 0}};
  environment.SkyLux = environment.CosSunZenith = 1;
  renderer.SetSubjectEnvironment(environment);
  CHECK(scene->Draw(error), "the first frame consumes the prepared reflection atlas");
  std::vector<float> pixels;
  CHECK(renderer.ReadSceneLinear(pixels) == ReadState::Ready, "linear radiance reaches readback");
  if (pixels.size() != 32 * 32 * 4) { return {}; }
  constexpr size_t at = (16 * 32 + 16) * 4;
  CHECK(renderer.Plan().Holds(Stage::EnvironmentSpecular),
        "the plan holds the shared reflection producer");
  std::vector<float> atlas;
  const auto atlasState = renderer.ReadEnvironmentSpecular(atlas);
  CHECK(atlasState == ReadState::Ready && !atlas.empty(), "submitted sky radiance is readable");
  CHECK(pixels[at + 3] == 1, "the measured pixel is covered by the declared surface");
  std::printf("reflection upward %d up %.1f %.1f %.1f: %.8g %.8g %.8g\n",
              upward,
              up[0],
              up[1],
              up[2],
              pixels[at],
              pixels[at + 1],
              pixels[at + 2]);
  return {{pixels[at], pixels[at + 1], pixels[at + 2]}};
}
}

int main() {
  CHECK(SDL_SetHint(SDL_HINT_ASSERT, "abort"), "assertions fail without a dialog");
  CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL video initializes");
  const auto sky = ReflectedColour({{0, 1, 0}}, true, 0);
  const auto ground = ReflectedColour({{0, 1, 0}}, false, 0);
  const auto rotated = ReflectedColour({{1, 0, 0}}, true, 0);
  for (size_t channel = 0; channel < 3; ++channel) {
    CHECK(std::isfinite(sky[channel]) && sky[channel] > 0.0001f,
          "a white metal reflects physical sky without diffuse, emission or direct light");
    CHECK(std::abs(ground[channel]) < sky[channel] * 0.01f,
          "a downward reflection sees black ground rather than hemispheric average sky");
    CHECK_NEAR(rotated[channel],
               sky[channel],
               std::max(0.00001f, sky[channel] * 0.02f),
               "world frame",
               "explicit world-up rotates reflection and source together");
  }
  SDL_Quit();
  return Report();
}
