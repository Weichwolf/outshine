#include "MeshInput.h"
#include "Check.h"
#include "DrawList.h"
#include "ExecutionPlan.h"
#include "Lens.h"
#include "Mat4.h"
#include "SceneRenderer.h"
#include "SubjectTypes.h"
#include "Viewing.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kSide = 32;
constexpr size_t kCentre = (size_t{kSide / 2} * kSide + kSide / 2) * 4;

bool InstallMesh(SceneRenderer &renderer) {
  std::array<MaterialBinding, 2> materials;
  materials[0].Row.Unlit = true;
  materials[0].Row.BaseColour = {{1, 0, 0, 1}};
  materials[1].Row.Transmission = 1;
  std::string error;
  CHECK(renderer.SetGeometryMaterials(materials, error), "opaque and glass materials upload");
  DrawList draws;
  for (uint32_t slot = 0; slot < materials.size(); ++slot) {
    DrawItem item;
    item.Order.MaterialSlot = slot;
    item.Order.Surface = StateOf(materials[slot].Row);
    item.SourceFirstIndex = slot * 3;
    item.IndexCount = 3;
    CHECK(draws.Add(item, error), "both surface kinds enter the shared native mesh");
  }
  draws.Compile();
  constexpr std::array<float, 18> positions{
      -1, -1, 0, 1, -1, 0, 0, 1, 0, 9, -1, 0, 11, -1, 0, 10, 1, 0};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
  constexpr std::array<float, 18> emitted{1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  MeshInput mesh;
  mesh.Verts.From = positions.data();
  mesh.Positions = positions;
  mesh.Emitted.From = emitted.data();
  mesh.VertexCount = 6;
  mesh.Indices = indices.data();
  mesh.IndexCount = 6;
  mesh.Draws = &draws;
  const auto installed = renderer.SetGeometryMesh(mesh, error) &&
                         renderer.GeometryPlacementRows(1, error) &&
                         renderer.SetGeometryPlacements(Mat4{}.data(), 1, error);
  CHECK(installed, error.empty() ? "one native placement reaches both passes" : error.c_str());
  return installed;
}

bool Draw(SceneRenderer &renderer, std::vector<float> &pixels) {
  const auto drawn = renderer.RenderFrame();
  CHECK(drawn, drawn ? "the shared scene submits" : drawn.error().c_str());
  if (!drawn) { return false; }
  const auto read = renderer.ReadSceneLinear(pixels);
  CHECK(read == ReadState::Ready && pixels.size() == size_t{kSide} * kSide * 4,
        "the actual submitted frame is readable");
  return read == ReadState::Ready && pixels.size() == size_t{kSide} * kSide * 4;
}

void Exercise() {
  PlanSpec spec;
  spec.Outputs = {Resource::Surface, Resource::SceneLinear, Resource::SceneComposited};
  spec.Content = {Stage::OpaqueGeometry, Stage::TransmissiveGeometry};
  const auto plan = ExecutionPlan::Compile(spec);
  CHECK(plan && (*plan)->Holds(Stage::TransmissiveGeometry),
        "the test really executes both opaque and transmission passes");
  if (!plan) { return; }
  SceneRenderer renderer;
  CHECK(renderer.Init({.WidthPx = kSide, .HeightPx = kSide}, *plan),
        "the mixed native plan initializes");
  if (!renderer.DeviceUsable() || !InstallMesh(renderer)) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, 4}};
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = eye.YMagM = 2;
  constexpr double kNearM = .1;
  constexpr double kFarM = 100;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  const auto lens = Lens::From(eye, kSide, kSide);
  CHECK(lens, "an independently specified frontal camera exists");
  if (!lens) { return; }
  renderer.SetCamera(eye, *lens);
  std::vector<float> pixels;
  if (!Draw(renderer, pixels)) { return; }
  CHECK(pixels[kCentre] == 1 && pixels[kCentre + 1] == 0,
        "the opaque red triangle is visible before the shared placement changes");
  const auto before = renderer.RecordedCrossings();
  Mat4 moved;
  moved[12] = 4;
  renderer.MoveGeometryPlacement(0, moved);
  if (!Draw(renderer, pixels)) { return; }
  const auto copies = renderer.RecordedCrossings() - before;
  std::println("shared-placement recorded-copies={}", copies);
  CHECK(copies == 1, "both passes consume one recorded placement transfer");
  CHECK(pixels[kCentre] == 0 && pixels[kCentre + 1] == 0,
        "the placement update really moves the visible triangle out of the image");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
