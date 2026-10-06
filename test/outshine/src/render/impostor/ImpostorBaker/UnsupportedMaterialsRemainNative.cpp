#include "ImpostorBaker.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  for (int variant = 0; variant < 7; ++variant) {
    Geometry mesh;
    Material material;
    material.DoubleSided = true;
    switch (variant) {
      case 0: material.Transmission = 0.5f; break;
      case 1: material.Alpha = AlphaMode::Blended; break;
      case 2: material.Emission = {{0.1f, 0, 0}}; break;
      case 3: material.SpecularFactor = 0.4f; break;
      case 4: material.Unlit = true; break;
      case 5:
        material.OcclusionMap.Image = *mesh.addImage(1, 1, std::array<uint8_t, 4>{64, 64, 64, 255});
        break;
      case 6:
        material.BaseColourMap.Image =
            *mesh.addImage(1, 1, std::array<uint8_t, 4>{128, 128, 128, 255});
        material.BaseColourMap.Set = UvSet::Uv1;
        break;
    }
    const auto surface = *mesh.addSurface("unsupported", material);
    const auto part = *mesh.addPart("triangle", surface);
    CHECK(mesh.setPositions(part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
              mesh.setNormals(part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
              mesh.setTriangles(part, std::array<uint32_t, 3>{0, 1, 2}),
          "native fallback fixture is valid");
    if (variant == 6) {
      CHECK(mesh.setTexture(part, std::array<float, 6>{0, 0, 1, 0, 0.5f, 1}, 1),
            "second-coordinate fixture is valid native geometry");
    }
    Render::ImpostorCapture capture;
    capture.LeastM = {{-1, -1, -0.1}};
    capture.MostM = {{1, 1, 0.1}};
    capture.Sources.push_back({.Mesh = std::move(mesh), .Instances = {Mat4{}}});
    std::string error;
    CHECK(!Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 8, .Views = 1}, error) &&
              error.find("retain native geometry") != std::string::npos,
          "unsupported light response or coverage is rejected explicitly for native fallback");
  }
  SDL_Quit();
  return Report();
}
