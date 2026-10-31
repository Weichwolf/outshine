#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <print>
#include <ratio>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using Json = nlohmann::json;
using Clock = std::chrono::steady_clock;

[[noreturn]] void Fail(const std::string &error) {
  std::println(stderr, "{}", error);
  std::_Exit(1);
}

Geometry Base() {
  Geometry geometry;
  const auto surface = geometry.addSurface("wall", Material{});
  if (!surface) { Fail("base material unavailable"); }
  const auto part = geometry.addPart("triangle", *surface);
  if (!part || !geometry.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) ||
      !geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2})) {
    Fail("base geometry unavailable");
  }
  return geometry;
}

Json Measure(Render::SceneRenderer &renderer,
             std::span<Render::SceneResources::PieceRows> changes,
             bool accepted,
             std::span<const Mat4> alternate = {}) {
  Json times = Json::array();
  const auto initial = changes.front().Rows;
  for (int round = 0; round < 5; ++round) {
    if (!alternate.empty()) {
      for (auto &change : changes) { change.Rows = round % 2 == 0 ? alternate : initial; }
    }
    std::string error;
    const auto began = Clock::now();
    const bool result = renderer.SetPieceInstances(changes, error);
    const auto elapsed = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
    if (result != accepted) { Fail("unexpected instance update result: " + error); }
    if (!accepted && error != "an instance update names one piece more than once") {
      Fail("negative control did not reject the repeated handle");
    }
    times.push_back(elapsed);
  }
  return times;
}

Json Run() {
  const auto base = Base();
  Core::Declaration declaration;
  declaration.InitialGeometry = &base;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface"};
  Render::SceneRenderer renderer;
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) { Fail(error); }
  const std::array<StoredVertex, 3> vertices{
      {StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 1, 0}}),
       StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 1, 0}}),
       StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 1, 0}})}};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  Render::PieceMesh mesh{};
  mesh.Verts = vertices;
  mesh.Indices = indices;
  mesh.MaxInstances = 1;
  const std::array<Mat4, 1> rows{};
  std::array<Mat4, 1> moved{};
  moved[0].SetTranslation({{1, 0, 0}});
  std::vector<Render::SceneResources::PieceRows> changes;
  Json inputs = Json::array();
  for (size_t at = 0; at < 8192; ++at) {
    const auto piece = renderer.PlacePiece(mesh);
    if (!piece) { Fail(piece.error()); }
    changes.push_back({.Piece = *piece, .Rows = rows});
    inputs.push_back({piece->Slot, piece->Generation});
  }
  if (!scene->Draw(error)) { Fail(error); }
  renderer.WaitForGpu();
  Json report{{"device", renderer.GpuName()},
              {"scope", "Host instance-update calls only; setup/GPU/Frame excluded"},
              {"piece_slots", renderer.PieceSlots()},
              {"piece_slot_bytes", renderer.PieceSlotBytes()},
              {"handles", inputs},
              {"cases", Json::array()}};
  for (const size_t count : std::array<size_t, 5>{512, 1024, 2048, 4096, 8192}) {
    const auto selected = std::span(changes).first(count);
    Json record{{"pieces", count}, {"unchanged_ms", Measure(renderer, selected, true)}};
    std::vector<Render::SceneResources::PieceRows> repeated(selected.begin(), selected.end());
    repeated.push_back(repeated.front());
    record["duplicate_ms"] = Measure(renderer, repeated, false);
    record["moved_ms"] = Measure(renderer, selected, true, moved);
    for (auto &change : std::span(changes).first(count)) { change.Rows = rows; }
    if (!renderer.SetPieceInstances(std::span(changes).first(count), error)) { Fail(error); }
    report["cases"].push_back(std::move(record));
  }
  return report;
}
}

int main(int argc, char *const argv[]) {
  if (argc != 2) { Fail("expected new report path"); }
  std::error_code error;
  if (std::filesystem::exists(argv[1], error) || error) { Fail("report path unavailable"); }
  if (!SDL_Init(SDL_INIT_VIDEO)) { Fail(SDL_GetError()); }
  const auto report = Run();
  SDL_Quit();
  std::ofstream output(argv[1]);
  output << report.dump(2) << '\n';
  output.close();
  if (!output) { Fail("instance update report write failed"); }
}
