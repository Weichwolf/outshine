#include "ImpostorBaker.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;

Render::ImpostorCapture MakeCapture() {
  Geometry mesh;
  Material material;
  material.DoubleSided = true;
  material.BaseColour = {{0.25f, 0.5f, 0.75f, 1}};
  material.Roughness = 0.6f;
  material.Metalness = 0.25f;
  const auto surface = *mesh.addSurface("plane", material);
  const auto part = *mesh.addPart("plane", surface);
  CHECK(mesh.setPositions(part, std::array<float, 12>{-2, 0, -1, 2, 0, -1, 2, 0, 1, -2, 0, 1}) &&
            mesh.setNormals(part, std::array<float, 12>{0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}) &&
            mesh.setTriangles(part, std::array<uint32_t, 6>{0, 2, 1, 0, 3, 2}),
        "anisotropic plane owns actual native geometry and a material");
  Render::ImpostorCapture capture;
  capture.LeastM = {{-2, 0, -1}};
  capture.MostM = {{2, 0, 1}};
  capture.Sources.push_back({.Mesh = std::move(mesh), .Instances = {Mat4{}}});
  return capture;
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  std::string error;
  {
    auto capture = MakeCapture();
    constexpr double diagonal = 1 / std::numbers::sqrt3;
    capture.ViewDirections = {{{0, 1, 0}}, {{0, -1, 0}}, {{diagonal, diagonal, diagonal}}};
    const auto directions = capture.ViewDirections;
    const auto atlas =
        Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 17, .Views = 3}, error);
    CHECK(atlas, error.c_str());
    if (atlas) {
      const double expectedExtent = 3 / std::numbers::sqrt2 * 17 / 15;
      CHECK(std::abs(atlas->HalfExtentM() - expectedExtent) < 1e-12,
            "shared square bounds enclose the actual projected box in every requested view");
      CHECK(atlas->Views().size() == directions.size(), "every recipe direction is captured");
      for (size_t view = 0; view < atlas->Views().size(); ++view) {
        CHECK(atlas->Views()[view].TowardEye == directions[view],
              "direction order survives capture");
        size_t covered = 0;
        for (size_t at = 0; at < atlas->Views()[view].Texels.size(); ++at) {
          const auto &texel = atlas->Views()[view].Texels[at];
          if (texel.Surface == 0) { continue; }
          ++covered;
          CHECK(std::abs(std::abs(texel.Normal[1]) - 1) < 0.001f,
                "captured normal belongs to the original horizontal plane");
          const auto &sample = atlas->Views()[view].Materials[at];
          CHECK(sample.BaseColour == Vec3f({{0.25f, 0.5f, 0.75f}}) &&
                    std::abs(sample.Roughness - 0.6f) < 0.001f && sample.Metalness == 0.25f,
                "spatial capture retains resolved material values");
        }
        CHECK(covered > 20 && covered < size_t{17} * 17,
              "plane and empty border both remain in the atlas");
      }
    }
  }
  {
    auto capture = MakeCapture();
    capture.ViewDirections = {{{0, 1, 0}}};
    CHECK(!Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 17, .Views = 2}, error),
          "catalogue count must agree with the declared atlas shape");
  }
  {
    auto capture = MakeCapture();
    capture.ViewDirections = {{{0, 2, 0}}};
    CHECK(!Render::ImpostorBaker::Bake(std::move(capture), {.Pixels = 17, .Views = 1}, error),
          "nonunit catalogue direction is rejected");
  }
  SDL_Quit();
  return Report();
}
