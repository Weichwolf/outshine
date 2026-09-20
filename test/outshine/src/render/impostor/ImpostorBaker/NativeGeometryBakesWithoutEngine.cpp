#include "ImpostorBaker.h"

#include "Check.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Geometry mesh;
    Material material;
    material.DoubleSided = true;
    const auto surface = mesh.addSurface("two-sided", material);
    CHECK(surface.has_value(), "native capture fixture owns one material");
    if (!surface) { return Report(); }
    const auto part = mesh.addPart("triangle", *surface);
    CHECK(part.has_value(), "native capture fixture owns one part");
    if (!part) { return Report(); }
    CHECK(mesh.setPositions(*part,
                            std::array<float, 9>{-0.5f, -0.5f, 0, 0.5f, -0.5f, 0, 0, 0.5f, 0}) &&
              mesh.setNormals(*part, std::array<float, 9>{0, 0, 1, 0, 0, 1, 0, 0, 1}) &&
              mesh.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "native capture fixture is complete");
    Render::ImpostorCapture capture;
    capture.LeastM = {{-0.5, -0.5, -0.1}};
    capture.MostM = {{0.5, 0.5, 0.1}};
    capture.Sources.push_back({.Mesh = std::move(mesh), .Instances = std::vector<Mat4>{Mat4{}}});
    std::string error;
    const auto atlas =
        Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 16, .Views = 2}, error);
    CHECK(atlas.has_value(), "native geometry bakes without engine or generator ownership");
    if (atlas) {
      CHECK(atlas->Views().size() == 2 && atlas->Surfaces().size() == 1,
            "one renderer captures every requested view and material");
      for (const auto &view : atlas->Views()) {
        CHECK(
            std::ranges::any_of(view.Texels, [](const auto &texel) { return texel.Surface == 1; }),
            "each opposing view captures the double-sided source");
      }
    }
  }
  SDL_Quit();
  return Report();
}
