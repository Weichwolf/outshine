#include "RenderScene.h"
#include "Check.h"
#include "GeometryRenderer.h"
#include "Gpu.h"
#include "SceneResources.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kPieces = 20, kVertices = 65536;

class Pressure final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  bool Acquire = false, SubmitBusy = false;
  Native::ErrorKind Kind = Native::ErrorKind::Busy;
  unsigned Rejected = 0, Waits = 0;

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    if (!Acquire) { return Backing().Begin(); }
    ++Rejected;
    return std::unexpected(Native::Error{.Kind = Kind, .Message = "owned restore pressure"});
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    if (!SubmitBusy) { return Backing().Submit(std::move(commands)); }
    ++Rejected;
    return std::unexpected(Native::Error{.Kind = Kind, .Message = "owned restore pressure"});
  }

  Native::Result<Native::CompletionStatus> Wait(Native::Completion completion,
                                                std::chrono::nanoseconds budget) override {
    ++Waits;
    return Backing().Wait(completion, budget);
  }
};

bool Configure(GeometryRenderer &draw, Pressure &device) {
  outshine::Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 32;
  if (!gpu.SceneColours.Add(Resource::SceneLinear) || !draw.Configure(gpu)) { return false; }
  const std::array<MaterialBinding, 1> materials{};
  std::string error;
  if (!draw.AppendMaterials(materials, error)) { return false; }
  constexpr std::array<uint32_t, 1> slots{0};
  draw.SetNativePieceSurfaces(slots);
  return true;
}

void Exercise(bool acquire, Native::ErrorKind kind) {
  const auto native = outshine::Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
  outshine::Test::Gpu::NativeOfflineDevice offline(*native);
  Pressure device(offline);
  RenderScene publishedStorage;
  GeometryRenderer published{publishedStorage};
  RenderScene candidateStorage;
  GeometryRenderer candidate{candidateStorage};
  CHECK(Configure(published, device) && Configure(candidate, device),
        "published and candidate draw native geometry");
  SceneResources original;
  SceneResources copied;
  const std::vector<StoredVertex> vertices(kVertices,
                                           StoredVertex::Of({{1, 2, 3}}, {}, {{0, 1, 0}}));
  const std::array<uint32_t, 3> indices{0, 1, 2};
  PieceMesh mesh;
  mesh.Verts = vertices;
  mesh.Indices = indices;
  std::array<PieceHandle, kPieces> handles;
  for (auto &handle : handles) {
    const auto placed = original.PlacePiece(published, mesh);
    CHECK(placed.has_value(), "published geometry is accepted before pressure");
    if (!placed) { return; }
    handle = *placed;
  }
  CHECK(published.Residency().SubmitPendingUploads().has_value(), "published uploads complete");
  copied.CopySourcesFrom(original, SceneResources::PieceSources::Copy);
  device.Acquire = acquire;
  device.SubmitBusy = !acquire;
  device.Kind = kind;
  size_t next = 0;
  const auto paused = copied.AdvancePieceRestore(candidate, next, kPieces);
  CHECK(device.Rejected == 1 && next > 0 && next < kPieces,
        "real staged upload pressure occurs after partial restoration");
  CHECK(candidate.PiecesStanding() == next && published.PiecesStanding() == kPieces,
        "accepted candidate pieces and the published world survive rejection");
  if (kind == Native::ErrorKind::Busy) {
    CHECK(paused && !*paused, "temporary pressure suspends rather than fails restoration");
    const size_t held = next;
    const auto still = copied.AdvancePieceRestore(candidate, next, kPieces);
    CHECK(still && !*still && next == held && candidate.PiecesStanding() == held,
          "persistent pressure retries the same missing piece without duplicating progress");
  } else {
    CHECK(!paused && paused.error() == "owned restore pressure", "driver failure remains fatal");
  }
  CHECK(device.Waits == 0, "incremental candidate restoration never waits for the GPU");
  device.Acquire = device.SubmitBusy = false;
  CHECK(copied.AdvancePieceRestore(candidate, next, kPieces).value_or(false) && next == kPieces,
        "restoration completes from the retained cursor after pressure ends");
  CHECK(candidate.PiecesStanding() == kPieces && candidate.PieceTriangles() == kPieces &&
            candidate.Residency().SubmitPendingUploads().has_value(),
        "every source piece is resident exactly once and its uploads remain accepted");
  CHECK(copied.AdvancePieceRestore(candidate, next, kPieces).value_or(false),
        "completed restoration remains idempotent");
  std::string error;
  for (const auto handle : handles) {
    CHECK(original.SetPieceInstances(published, handle, {}, error) &&
              copied.SetPieceInstances(candidate, handle, {}, error),
          "both resource tables preserve each original piece handle");
  }
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "native pressure fixture starts video");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) {
    for (const bool acquire : {false, true}) {
      for (const auto kind : {Native::ErrorKind::Busy, Native::ErrorKind::Driver}) {
        Exercise(acquire, kind);
      }
    }
  }
  SDL_Quit();
  return Report();
}
