#include <Outshine.h>
#include "BuildingMaterials.h"
#include "StoredVertex.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

constexpr int kWidth = 1280, kHeight = 720;
constexpr size_t kVertices = 6;
constexpr double kKeyLux = 1000;
constexpr double kKeyElevationDeg = 30;
constexpr double kKeyBearingDeg = 180;
constexpr double kAmbientRadiance = 0.1;
constexpr double kCameraHeightM = 3.85;
constexpr double kCameraNearM = 0.1;
constexpr double kCameraFarM = 100;
constexpr float kWallRoughness = 0.9f;
constexpr Extent kFrame{.WidthPx = kWidth, .HeightPx = kHeight};

struct Fixture {
  std::array<Vec3, 6> Frame;
  std::array<StoredVertex, kVertices> Verts;
  std::array<float, kVertices * 2> Metres;
};

constexpr size_t kFixtureBytes =
    6 * sizeof(Vec3) + kVertices * (sizeof(StoredVertex) + 2 * sizeof(float));
static_assert(std::is_trivially_copyable_v<Fixture> && sizeof(Fixture) == kFixtureBytes);

bool Accepted(const Result &result) {
  CHECK(result.has_value(), result ? "public operation accepted" : result.error().c_str());
  return result.has_value();
}

Geometry Mesh(const Fixture &fixture, SurfacePattern pattern, bool mapped, double &centreX) {
  const auto &frame = fixture.Frame;
  Vec3 along = frame[5] - frame[4];
  if (!Normalise(along)) { return {}; }
  const Vec3 horizontal = frame[1] * along[0] + frame[3] * along[2];
  const Vec3 facing = Cross(horizontal, frame[2]);
  std::array<float, kVertices * 3> positions{};
  std::array<float, kVertices * 3> normals{};
  std::array<float, kVertices * 2> uv{};
  for (size_t at = 0; at < kVertices; ++at) {
    const auto &vertex = fixture.Verts[at];
    const Vec3 relative = Vec3{{vertex.pos[0], vertex.pos[1], vertex.pos[2]}} - frame[0];
    const Vec3 base = frame[1] * frame[4][0] + frame[2] * frame[4][1] + frame[3] * frame[4][2];
    const Vec3 point = relative - base;
    const Vec3f normal = vertex.norm();
    const Vec3 n{{normal[0], normal[1], normal[2]}};
    const std::array axes{horizontal, frame[2], facing};
    for (size_t axis = 0; axis < axes.size(); ++axis) {
      positions[at * 3 + axis] = static_cast<float>(Dot(point, axes[axis]));
      normals[at * 3 + axis] = static_cast<float>(Dot(n, axes[axis]));
    }
    centreX += positions[at * 3] / static_cast<double>(kVertices);
    uv[at * 2] = vertex.texture[0];
    uv[at * 2 + 1] = vertex.texture[1];
  }
  Geometry mesh;
  Material material;
  material.BaseColour = {{Generators::kBuildingWallColour[0],
                          Generators::kBuildingWallColour[1],
                          Generators::kBuildingWallColour[2],
                          1}};
  material.Pattern = pattern;
  material.Roughness = kWallRoughness;
  material.DoubleSided = true;
  if (mapped) {
    const auto image = mesh.addImage(1, 1, std::array<uint8_t, 4>{157, 110, 249, 255});
    CHECK(image, "authored normal texel uploads through the public asset API");
    if (!image) { return {}; }
    material.NormalMap.Image = *image;
  }
  const auto surface = mesh.addSurface("actual-native-wall", material);
  CHECK(surface, "actual wall material is valid");
  if (!surface) { return {}; }
  const auto part = mesh.addPart("cached-wall", *surface);
  CHECK(part, "actual wall part is valid");
  if (!part) { return {}; }
  CHECK(mesh.setPositions(*part, positions) && mesh.setNormals(*part, normals) &&
            mesh.setTexture(*part, uv) && mesh.setTexture(*part, fixture.Metres, 1) &&
            mesh.setTriangles(*part, std::array<uint32_t, 6>{0, 1, 2, 3, 4, 5}),
        "public geometry retains both actual native UV streams");
  if (mapped) {
    std::array<float, kVertices * 4> tangents{};
    for (size_t at = 0; at < kVertices; ++at) { tangents[at * 4] = tangents[at * 4 + 3] = 1; }
    CHECK(mesh.setTangents(*part, tangents), "authored wall tangent frame uploads");
  }
  return mesh;
}

bool RenderWall(const Geometry &mesh, double centreX, const char *prefix) {
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = kFrame;
  scenario.Render.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = kKeyLux;
  scenario.Lit.Key.ElevationDeg = kKeyElevationDeg;
  scenario.Lit.Key.BearingDeg = kKeyBearingDeg;
  scenario.Lit.IndirectLight = {{kAmbientRadiance, kAmbientRadiance, kAmbientRadiance}};
  Scenario::View view;
  view.Id = "wall";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{centreX, kCameraHeightM, 8}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{centreX, kCameraHeightM, 0}};
  view.Sees.setProjection(Camera::Ortho{
      .XMagM = 2, .YMagM = 2.0 * kHeight / kWidth, .NearM = kCameraNearM, .FarM = kCameraFarM});
  scenario.Views.push_back(view);
  Engine engine;
  if (!Accepted(engine.setRenderTarget(kFrame)) || !Accepted(engine.declare(scenario)) ||
      !Accepted(engine.setGeometry(mesh)) || !Accepted(engine.assemble())) {
    return false;
  }
  for (int repeat = 0; repeat < 3; ++repeat) {
    if (!Accepted(engine.advance()) || !Accepted(engine.renderer().render({}))) { return false; }
  }
  std::vector<float> linear;
  std::vector<float> depth;
  std::vector<float> normals;
  if (!Accepted(engine.renderer().readPixels(Buffer::Linear, linear)) ||
      !Accepted(engine.renderer().readPixels(Buffer::Depth, depth)) ||
      !Accepted(engine.renderer().readPixels(Buffer::ShadingNormal, normals))) {
    return false;
  }
  CHECK(linear.size() == static_cast<size_t>(kWidth * kHeight * 4) &&
            depth.size() == static_cast<size_t>(kWidth * kHeight),
        "complete 1280x720 frame and depth read through the public API");
  CHECK(std::ranges::all_of(linear, [](float value) { return std::isfinite(value); }),
        "lit MR output is finite");
  CHECK(std::ranges::all_of(depth, [](float value) { return value > 0; }),
        "the actual cached face covers the whole camera crop");
  CHECK(normals.size() == linear.size() &&
            std::ranges::all_of(normals, [](float value) { return std::isfinite(value); }),
        "the public shading-normal attachment is complete and finite");
  const std::string path(prefix);
  std::ofstream normalOutput(path + ".normal.f32", std::ios::binary);
  normalOutput.write(reinterpret_cast<const char *>(normals.data()),
                     static_cast<std::streamsize>(normals.size() * sizeof(float)));
  normalOutput.close();
  CHECK(normalOutput, "actual shading normals are saved without display encoding");
  std::ofstream output(path + ".f32", std::ios::binary);
  output.write(reinterpret_cast<const char *>(linear.data()),
               static_cast<std::streamsize>(linear.size() * sizeof(float)));
  output.close();
  CHECK(output, "linear radiance is saved separately from display encoding");
  return Accepted(engine.renderer().saveScreenshot(path + ".png"));
}
}

int main(int argc, char **argv) {
  if (argc != 4 || (std::string_view(argv[3]) != "1" && std::string_view(argv[3]) != "2")) {
    return 2;
  }
  Fixture fixture{};
  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
  const std::streamoff bytes = input.tellg();
  CHECK(input && std::cmp_equal(bytes, sizeof(fixture)),
        "actual source/cache fixture has its exact native layout");
  input.seekg(0);
  input.read(reinterpret_cast<char *>(&fixture), sizeof(fixture));
  if (!input || !SDL_Init(SDL_INIT_VIDEO)) { return 1; }
  {
    double centreX = 0;
    const auto pattern = (std::string_view(argv[3]) == "2" || std::string_view(argv[3]) == "3")
                             ? SurfacePattern::FacadeMetres
                             : SurfacePattern::Facade;
    const auto mesh = Mesh(fixture, pattern, std::string_view(argv[3]) == "3", centreX);
    CHECK(mesh.wellFormed(), "the actual transformed source face is a valid native asset");
    CHECK(RenderWall(mesh, centreX, argv[2]), "public engine material capture completes");
  }
  SDL_Quit();
  return Report();
}
