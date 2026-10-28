#include "Heap.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "NativeMaterialMemory.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using Json = nlohmann::json;

[[noreturn]] void Fail(const std::string &reason) {
  std::println(stderr, "{}", reason);
  std::_Exit(1);
}

Geometry MaterialSource(size_t variant) {
  constexpr size_t side = 256;
  std::vector<uint8_t> pixels(side * side * 4, 255);
  for (size_t at = 0; at < pixels.size(); at += 4) {
    pixels[at] = static_cast<uint8_t>(64 + variant * 8);
    pixels[at + 1] = 96;
    pixels[at + 2] = 128;
  }
  Geometry source;
  const auto image = source.addImage(256, 256, pixels);
  if (!image) { Fail("material image rejected"); }
  Material material;
  material.Unlit = true;
  material.BaseColourMap.Image = *image;
  if (!source.addSurface("resident-image", material)) { Fail("material source rejected"); }
  return source;
}

Render::MaterialRegistration Register(Render::SceneRenderer &renderer, size_t variant) {
  auto result = renderer.RegisterPieceMaterials(MaterialSource(variant));
  if (!result) { Fail(result.error()); }
  return *result;
}

void Draw(Render::SceneRenderer &renderer) {
  if (auto result = renderer.RenderFrame(); !result) { Fail(result.error()); }
  renderer.WaitForGpu();
}

Json Cycle(Render::SceneRenderer &renderer, const Render::MaterialRegistration &material) {
  const std::array<StoredVertex, 4> vertices{StoredVertex::Of({{-1, -1, 0}}, {{0, 1}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, -1, 0}}, {{1, 1}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, 1, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{-1, 1, 0}}, {{0, 0}}, {{0, 0, 1}})};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};
  Render::PieceMesh mesh;
  mesh.Verts = vertices;
  mesh.Indices = indices;
  mesh.Textured = true;
  mesh.Surface = Render::PieceSurface::Registered(material);
  const auto piece = renderer.PlacePiece(mesh);
  if (!piece) { Fail(piece.error()); }
  Draw(renderer);
  std::vector<float> colour;
  if (renderer.ReadSceneLinear(colour) != Render::ReadState::Ready || colour.empty()) {
    Fail("piece colour unavailable");
  }
  double radiance = 0;
  for (size_t at = 0; at < colour.size(); ++at) {
    if (!std::isfinite(colour[at])) { Fail("nonfinite piece colour"); }
    if (at % 4 != 3) { radiance += colour[at]; }
  }
  if (!(radiance > 0)) { Fail("piece was not visible"); }
  renderer.ReleasePiece(*piece);
  Draw(renderer);
  if (renderer.PiecesStanding() != 0 || renderer.PieceSourceBytes() != 0) {
    Fail("released geometry remains live");
  }
  return {{"registered_index", material.First},
          {"cpp_heap_live_bytes", Heap::LiveBytes()},
          {"piece_source_bytes", renderer.PieceSourceBytes()},
          {"piece_slots", renderer.PieceSlots()},
          {"uploaded_bytes", renderer.TakeUploadBytes()},
          {"visible_rgb_sum", radiance}};
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { Fail("expected register|reuse and a JSON output path"); }
  const std::string_view mode(argv[1]);
  if (mode != "register" && mode != "reuse") { Fail("unknown material lifetime mode"); }
  Heap::EnableProcessInstrumentation();
  if (!SDL_Init(SDL_INIT_VIDEO)) { Fail(SDL_GetError()); }
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"sceneLinear"};
    declaration.Stages = {"opaqueGeometry"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) { Fail(error); }
    Render::Viewpoint eye;
    eye.Kind = Render::CameraKind::Orthographic;
    eye.EyeM = {{0, 0, 4}};
    eye.XMagM = eye.YMagM = 2;
    eye.ZNearM = .1;
    eye.ZFarM = 10;
    scene->Eye(eye);
    if (!scene->Draw(error)) { Fail(error); }
    renderer.WaitForGpu();
    const auto shared = mode == "reuse" ? Register(renderer, 0) : Render::MaterialRegistration{};
    Json records = Json::array();
    for (size_t at = 0; at < 16; ++at) {
      Json record;
      {
        const auto material = mode == "reuse" ? shared : Register(renderer, at);
        record = Cycle(renderer, material);
      }
      Draw(renderer);
      record["cpp_heap_live_bytes"] = Heap::LiveBytes();
      const auto deviceBytes = Test::MaterialDeviceAllocatedBytes();
      record["native_device_allocated_bytes"] = deviceBytes ? Json(*deviceBytes) : Json(nullptr);
      records.push_back(std::move(record));
    }
    const Json result{
        {"mode", mode},
        {"gpu", renderer.GpuName()},
        {"cycles", records},
        {"scope",
         "completed native draw/release cycles; instrumented C++ heap, not RSS; optional Metal "
         "currentAllocatedSize for the default device includes buffers and textures, excludes "
         "unreported driver storage and is not additive to process RSS on UMA; no frame-time "
         "claim"}};
    std::ofstream output(argv[2]);
    output << result.dump(2) << '\n';
    if (!output) { Fail("result write failed"); }
  }
  SDL_Quit();
  return 0;
}
