#include <Outshine.h>
#include "BuildingMaterials.h"
#include "StoredVertex.h"
#include "Check.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

constexpr int kWidth = 1280, kHeight = 720;
constexpr size_t kVertices = 6;

struct Fixture {
  std::array<Vec3, 6> Frame;
  std::array<StoredVertex, kVertices> Verts;
  std::array<float, kVertices * 2> Metres;
};

static_assert(std::is_trivially_copyable_v<Fixture> && sizeof(Fixture) == 336);

bool Accepted(const Result &result) {
  CHECK(result.has_value(), result ? "public operation accepted" : result.error().c_str());
  return result.has_value();
}

Geometry Mesh(const Fixture &fixture, SurfacePattern pattern, double &centreX) {
  const auto &frame = fixture.Frame;
  Vec3 along = frame[5] - frame[4];
  if (!Normalise(along)) { return {}; }
  const Vec3 horizontal = frame[1] * along[0] + frame[3] * along[2];
  const Vec3 facing = Cross(horizontal, frame[2]);
  std::array<float, kVertices * 3> positions{}, normals{};
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
  material.Roughness = 0.9f;
  material.DoubleSided = true;
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
  return mesh;
}

bool RenderWall(const Geometry &mesh, double centreX, const char *prefix) {
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {kWidth, kHeight};
  scenario.Render.Outputs = {"sceneLinear", "sceneDepth"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = 1000;
  scenario.Lit.Key.ElevationDeg = 30;
  scenario.Lit.Key.BearingDeg = 180;
  scenario.Lit.IndirectLight = {{0.1, 0.1, 0.1}};
  Scenario::View view;
  view.Id = "wall";
  view.Person = "first";
  view.Placement = Scenario::CameraPlacement::Local;
  view.Sees.PositionM = {{centreX, 3.85, 8}};
  view.Sees.LooksAt = true;
  view.Sees.LookAtM = {{centreX, 3.85, 0}};
  view.Sees.setProjection(Camera::Ortho{.XMagM = 2, .YMagM = 1.125, .NearM = 0.1, .FarM = 100});
  scenario.Views.push_back(view);
  Engine engine;
  if (!Accepted(engine.setRenderTarget({kWidth, kHeight})) || !Accepted(engine.declare(scenario)) ||
      !Accepted(engine.setGeometry(mesh)) || !Accepted(engine.assemble())) {
    return false;
  }
  for (int repeat = 0; repeat < 3; ++repeat) {
    if (!Accepted(engine.advance()) || !Accepted(engine.renderer().render({}))) { return false; }
  }
  std::vector<float> linear, depth;
  if (!Accepted(engine.renderer().readPixels(Buffer::Linear, linear)) ||
      !Accepted(engine.renderer().readPixels(Buffer::Depth, depth))) {
    return false;
  }
  CHECK(linear.size() == static_cast<size_t>(kWidth * kHeight * 4) &&
            depth.size() == static_cast<size_t>(kWidth * kHeight),
        "complete 1280x720 frame and depth read through the public API");
  CHECK(std::ranges::all_of(linear, [](float value) { return std::isfinite(value); }),
        "lit MR output is finite");
  CHECK(std::ranges::all_of(depth, [](float value) { return value > 0; }),
        "the actual cached face covers the whole camera crop");
  const std::string path(prefix);
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
  CHECK(input && input.tellg() == static_cast<std::streamoff>(sizeof(fixture)),
        "actual source/cache fixture has its exact native layout");
  input.seekg(0);
  input.read(reinterpret_cast<char *>(&fixture), sizeof(fixture));
  if (!input || !SDL_Init(SDL_INIT_VIDEO)) { return 1; }
  {
    double centreX = 0;
    const auto pattern =
        std::string_view(argv[3]) == "2" ? SurfacePattern::FacadeMetres : SurfacePattern::Facade;
    const auto mesh = Mesh(fixture, pattern, centreX);
    CHECK(mesh.wellFormed(), "the actual transformed source face is a valid native asset");
    CHECK(RenderWall(mesh, centreX, argv[2]), "public engine material capture completes");
  }
  SDL_Quit();
  return Report();
}
