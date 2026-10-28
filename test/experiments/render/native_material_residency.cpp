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

Render::PieceHandle Place(Render::SceneRenderer &renderer,
                          const Render::MaterialRegistration &material,
                          std::span<const Mat4> instances = {}) {
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
  mesh.Instances = instances;
  const auto piece = renderer.PlacePiece(mesh);
  if (!piece) { Fail(piece.error()); }
  return *piece;
}

Json Cycle(Render::SceneRenderer &renderer, const Render::MaterialRegistration &material) {
  const auto piece = Place(renderer, material);
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
  renderer.ReleasePiece(piece);
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

std::array<float, 3> PinnedColour(Render::SceneRenderer &renderer, size_t column = 8) {
  std::vector<float> colour;
  if (renderer.ReadSceneLinear(colour) != Render::ReadState::Ready ||
      colour.size() != size_t{32} * 32 * 4) {
    Fail("pinned sibling colour unavailable");
  }
  const size_t at = (size_t{16} * 32 + column) * 4;
  const std::array<float, 3> pixel{colour[at], colour[at + 1], colour[at + 2]};
  for (const float value : pixel) {
    if (!std::isfinite(value) || value < 0) { Fail("pinned sibling has invalid colour"); }
  }
  if (pixel[0] + pixel[1] + pixel[2] <= 0) { Fail("pinned sibling is not visible"); }
  return pixel;
}

struct MaterialPlacement {
  size_t Variant = 0;
  double X = 0;
};

Render::PieceHandle AddPositioned(Render::SceneRenderer &renderer, MaterialPlacement placement) {
  const auto material = Register(renderer, placement.Variant);
  constexpr double scale = .35;
  const Mat4 model = {{scale, 0, 0, 0, 0, scale, 0, 0, 0, 0, 1, 0, placement.X, 0, 0, 1}};
  return Place(renderer, material, std::span(&model, 1));
}

Json Interleaved(Render::SceneRenderer &renderer) {
  const auto permanent = AddPositioned(renderer, {.Variant = 0, .X = -1});
  Draw(renderer);
  const auto expected = PinnedColour(renderer);
  Render::PieceHandle trailing;
  Json records = Json::array();
  for (size_t at = 0; at < 16; ++at) {
    const auto temporary = AddPositioned(renderer, {.Variant = at * 2 + 1, .X = 0});
    const auto next = AddPositioned(renderer, {.Variant = at * 2 + 2, .X = 1});
    if (trailing) { renderer.ReleasePiece(trailing); }
    trailing = next;
    Draw(renderer);
    if (PinnedColour(renderer) != expected) { Fail("interleaved admission changed live colours"); }
    const auto right = PinnedColour(renderer, 24);
    const float red = static_cast<float>(static_cast<uint8_t>(64 + (at * 2 + 2) * 8)) / 255.0f;
    const float linear = red <= .04045f ? red / 12.92f : std::pow((red + .055f) / 1.055f, 2.4f);
    const float expectedRed = linear * Material{}.BaseColour[0];
    constexpr float colourTolerance = .001f;
    if (std::abs(right[0] - expectedRed) > colourTolerance || right[1] != expected[1] ||
        right[2] != expected[2]) {
      Fail("the new pinned source has another material's actual colour");
    }
    renderer.ReleasePiece(temporary);
    Draw(renderer);
    if (renderer.PiecesStanding() != 2 || renderer.PieceSlots() > 4 ||
        PinnedColour(renderer) != expected || PinnedColour(renderer, 24) != right) {
      Fail("interior retirement lost a live sibling or leaked piece slots");
    }
    const auto deviceBytes = Test::MaterialDeviceAllocatedBytes();
    records.push_back(
        {{"cycle", at},
         {"cpp_heap_live_bytes", Heap::LiveBytes()},
         {"native_device_allocated_bytes", deviceBytes ? Json(*deviceBytes) : Json(nullptr)},
         {"piece_slots", renderer.PieceSlots()},
         {"live_pieces", renderer.PiecesStanding()},
         {"pinned_rgb", expected}});
  }
  renderer.ReleasePiece(trailing);
  renderer.ReleasePiece(permanent);
  Draw(renderer);
  if (renderer.PiecesStanding() != 0 || renderer.PieceSourceBytes() != 0) {
    Fail("interleaved sources survived their last consumers");
  }
  return records;
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { Fail("expected register|reuse|interleaved and a JSON output path"); }
  const std::string_view mode(argv[1]);
  if (mode != "register" && mode != "reuse" && mode != "interleaved") {
    Fail("unknown material lifetime mode");
  }
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
    for (size_t at = 0; at < 16 && mode != "interleaved"; ++at) {
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
    if (mode == "interleaved") { records = Interleaved(renderer); }
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
