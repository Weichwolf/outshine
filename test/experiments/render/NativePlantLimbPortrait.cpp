#include "Compiled.h"
#include "Image.h"
#include "PlantSurface.h"
#include "SceneRenderer.h"
#include "StoredVertex.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numbers>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
constexpr uint32_t kWidth = 960;
constexpr uint32_t kHeight = 1080;
constexpr uint64_t kMostVertices = 1000000;
constexpr uint64_t kMostTriangles = 1000000;
constexpr uint64_t kMostStations = 1025;
constexpr float kBarkRed = 0.12f;
constexpr float kBarkGreen = 0.075f;
constexpr float kBarkBlue = 0.035f;
constexpr float kBarkRoughness = 0.85f;
constexpr float kSunLux = 20000;
constexpr float kSunAxis = 0.70710677f;
constexpr double kSkyLux = 5000;
constexpr double kGroundAlbedo = 0.2;
constexpr double kCameraFov = 0.8;
constexpr double kNearPlaneM = 0.1;
constexpr double kFarPlaneM = 100;
constexpr float kExposure = 2.5f / (1.2f * kSunLux);
constexpr Vec3 kNearEye{{1.5, 7, 3}};
constexpr Vec3 kNearAim{{0.1, 7, 0}};
constexpr Vec3 kWholeEye{{0, 9, 28}};
constexpr Vec3 kWholeAim{{0, 9, 0}};

template <typename Value> bool Read(std::ifstream &input, std::span<Value> values) {
  const auto bytes = std::as_writable_bytes(values);
  input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  return input.good();
}

bool Load(std::ifstream &input, Generators::PlantSurface &surface) {
  while (input.peek() != std::char_traits<char>::eof()) {
    std::array<uint64_t, 3> counts{};
    if (!Read(input, std::span<uint64_t>(counts)) || counts[0] < 2 || counts[0] > kMostStations ||
        counts[1] > kMostVertices || counts[2] > kMostTriangles) {
      return false;
    }
    const size_t offset = surface.Positions.size() / 3;
    const size_t triangleOffset = surface.Triangles.size();
    if (offset + counts[1] > kMostVertices || triangleOffset / 3 + counts[2] > kMostTriangles) {
      return false;
    }
    input.seekg(static_cast<std::streamoff>(counts[0] * 12 * sizeof(double)), std::ios::cur);
    surface.Positions.resize((offset + counts[1]) * 3);
    surface.Normals.resize(surface.Positions.size());
    surface.Triangles.resize(triangleOffset + counts[2] * 3);
    if (!Read(input, std::span(surface.Positions).subspan(offset * 3)) ||
        !Read(input, std::span(surface.Normals).subspan(offset * 3)) ||
        !Read(input, std::span(surface.Triangles).subspan(triangleOffset))) {
      return false;
    }
    for (auto &index : std::span(surface.Triangles).subspan(triangleOffset)) {
      if (index >= counts[1]) { return false; }
      index += static_cast<uint32_t>(offset);
    }
  }
  return !surface.Positions.empty() && Generators::PlantSurfaceIsOriented(surface);
}

bool Place(SceneRenderer &renderer, const Generators::PlantSurface &surface) {
  Material material;
  material.BaseColour = {kBarkRed, kBarkGreen, kBarkBlue, 1};
  material.Roughness = kBarkRoughness;
  Geometry source;
  if (!source.addSurface("wood", material)) { return false; }
  const auto slot = renderer.RegisterPieceMaterials(std::move(source));
  if (!slot) { return false; }
  std::vector<StoredVertex> vertices(surface.Positions.size() / 3);
  for (size_t at = 0; at < vertices.size(); ++at) {
    vertices[at] = StoredVertex::Of(
        {{surface.Positions[at * 3], surface.Positions[at * 3 + 1], surface.Positions[at * 3 + 2]}},
        {},
        {{surface.Normals[at * 3], surface.Normals[at * 3 + 1], surface.Normals[at * 3 + 2]}});
  }
  Mat4 placement;
  placement.At(1, 1) = placement.At(2, 2) = 0;
  placement.At(1, 2) = 1;
  placement.At(2, 1) = -1;
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = surface.Triangles;
  piece.Instances = std::span(&placement, 1);
  piece.MaxInstances = 1;
  piece.Surface = PieceSurface::Registered(*slot);
  return renderer.PlacePiece(piece).has_value();
}

bool RenderPortrait(SceneRenderer &renderer, const char *destination, bool near) {
  SubjectLight light;
  light.Light.Intensity = kSunLux;
  light.Light.Direction = {{-0.5f, -kSunAxis, 0.5f}};
  std::string error;
  if (!renderer.SetSubjectLights(std::span(&light, 1), error)) { return false; }
  const double sky = kSkyLux / std::numbers::pi;
  const double ground = kGroundAlbedo * (kSunLux * std::sqrt(0.5) + kSkyLux) / std::numbers::pi;
  renderer.SetSubjectEnvironment({.RadianceLinear = {{sky, sky, sky}},
                                  .GroundLinear = {{ground, ground, ground}},
                                  .UpUnit = {{0, 1, 0}},
                                  .GroundAlbedo = {},
                                  .SkyLux = 0,
                                  .CosSunZenith = 0});
  auto camera = Viewpoint::LookAt(
      {.EyeM = near ? kNearEye : kWholeEye, .AimM = near ? kNearAim : kWholeAim}, 0);
  if (!camera) { return false; }
  camera->YfovRad = kCameraFov;
  camera->ZNearM = kNearPlaneM;
  camera->ZFarM = kFarPlaneM;
  const auto lens = Lens::From(*camera, kWidth, kHeight);
  if (!lens) { return false; }
  renderer.SetCamera(*camera, *lens);
  const auto frame = renderer.RenderFrame();
  if (!frame) { return false; }
  renderer.WaitForGpu();
  std::vector<uint8_t> rgba;
  std::vector<uint8_t> png;
  if (renderer.ReadPixels(rgba) != ReadState::Ready ||
      !Core::EncodePng(rgba.data(), kWidth, kHeight, png)) {
    return false;
  }
  std::ofstream output(destination, std::ios::binary);
  output.write(reinterpret_cast<const char *>(png.data()),
               static_cast<std::streamsize>(png.size()));
  return output.good();
}
}

int main(int argc, const char *const *argv) {
  if (argc != 4 || !SDL_Init(SDL_INIT_VIDEO)) { return 1; }
  std::ifstream input(argv[1], std::ios::binary);
  Generators::PlantSurface surface;
  if (!input || !Load(input, surface)) { return 2; }
  PlanSpec spec;
  spec.Outputs = {Resource::FrameTex};
  spec.Content = {Stage::Subjects};
  spec.Exposure = Declared<float>(kExposure);
  const auto compiled = Compiled::Compile(spec);
  if (!compiled) { return 3; }
  SceneRenderer renderer;
  if (!renderer.Init({.WidthPx = kWidth, .HeightPx = kHeight}, *compiled) ||
      !Place(renderer, surface)) {
    return 4;
  }
  if (!RenderPortrait(renderer, argv[2], false) || !RenderPortrait(renderer, argv[3], true)) {
    return 5;
  }
  std::cout << "vertices=" << surface.Positions.size() / 3
            << " triangles=" << surface.Triangles.size() / 3 << " instances=1 world=0\n";
  return 0;
}
