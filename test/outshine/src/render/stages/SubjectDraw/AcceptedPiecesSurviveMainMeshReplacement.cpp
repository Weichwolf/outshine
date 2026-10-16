#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "Check.h"
#include "Gpu.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <string>

namespace {
using namespace outshine::Render::Native;

struct Faults final : outshine::Test::Gpu::NativeDeviceProxy {
  explicit Faults(Device &device) : NativeDeviceProxy(device) {}

  bool Reject = false;
  unsigned Rejected = 0;

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (Reject) {
      Reject = false;
      ++Rejected;
      return std::unexpected(Error{.Kind = ErrorKind::Driver,
                                   .Message = "injected accepted-upload submission failure"});
    }
    return Backing().Submit(std::move(commands));
  }
};

bool Poison(Device &device, Buffer &buffer, uint32_t bytes) {
  auto transfer = device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopySource, .Placement = Memory::Upload});
  if (!transfer) { return false; }
  auto mapped = device.MapWrite(**transfer, {.Bytes = bytes});
  if (!mapped) { return false; }
  std::memset(mapped->data(), 0, bytes);
  if (!device.Unmap(**transfer)) { return false; }
  auto commands = device.Begin();
  if (!commands || !(*commands)->Copy(**transfer, buffer, {.Bytes = bytes})) { return false; }
  return device.Submit(std::move(*commands)).has_value();
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto native = outshine::Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
    outshine::Test::Gpu::NativeOfflineDevice offline(*native);
    Faults device(offline);
    for (const bool replacement : {false, true}) {
      SubjectDraw draw;
      std::string error;
      outshine::Render::Gpu gpu;
      gpu.Device = &device;
      gpu.HdrFormat = Native::Format::Rgba16Float;
      gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
      gpu.Width = gpu.Height = 32;
      CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "scene attachment adds");
      CHECK(draw.Configure(gpu).has_value(), "native pipelines configure");
      const std::array<SubjectMaterial, 1> materials{};
      CHECK(draw.AppendMaterials(materials, error), "native material registers");
      constexpr std::array<uint32_t, 1> slots{0};
      draw.SetNativePieceSurfaces(slots);
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{2, 3, 4}}, {{0, 0}}, {{0, 1, 0}}),
          StoredVertex::Of({{5, 6, 7}}, {{1, 0}}, {{0, 1, 0}}),
          StoredVertex::Of({{8, 9, 10}}, {{0, 1}}, {{0, 1, 0}})};
      constexpr std::array<uint32_t, 3> indices{0, 1, 2};
      PieceMesh piece{};
      piece.Verts = vertices;
      piece.Indices = indices;
      CHECK(draw.PlacePiece(piece, error) != kNoPiece, "independent piece upload is accepted");
      const auto &resident = draw.Resident();
      auto *positions = resident.Buffer(SubjectResidency::Stream::Vertex).get();
      auto *triangles = resident.Buffer(SubjectResidency::Stream::Index).get();
      constexpr std::array<float, 9> expected{2, 3, 4, 5, 6, 7, 8, 9, 10};
      CHECK(Poison(device, *positions, sizeof(expected)) &&
                Poison(device, *triangles, sizeof(indices)),
            "destination buffers independently contain known wrong bytes");
      DrawList draws;
      DrawItem item{};
      item.IndexCount = 3;
      CHECK(draws.Add(item, error), "replacement triangle adds");
      draws.Compile();
      constexpr std::array<float, 9> emitted{};
      SubjectMesh mesh;
      if (replacement) {
        mesh.Verts.From = expected.data();
        mesh.Positions = expected;
        mesh.Emitted.From = emitted.data();
        mesh.VertexCount = 3;
        mesh.Indices = indices.data();
        mesh.IndexCount = 3;
        mesh.Draws = &draws;
      }
      const auto generation = draw.Generation();
      const auto failuresBefore = device.Rejected;
      device.Reject = true;
      const auto failed = draw.BeginMesh(mesh);
      CHECK(!failed && !failed.error().empty() && device.Rejected == failuresBefore + 1 &&
                !device.Reject,
            "failed accepted-upload submission rejects mesh replacement");
      CHECK(draw.Generation() == generation && draw.PiecesStanding() == 1,
            "submission failure preserves generation and piece ownership");
      const auto retry = draw.BeginMesh(mesh);
      CHECK(retry.has_value(), "main mesh changes on retry");
      if (retry) { CHECK(draw.FinishMesh(*retry, mesh, error), "main mesh replacement finishes"); }
      CHECK(draw.Owned().SubmitPendingUploads().has_value(),
            "accepted retry submits before inspection");
      positions = resident.Buffer(SubjectResidency::Stream::Vertex).get();
      triangles = resident.Buffer(SubjectResidency::Stream::Index).get();
      Readback vertexReadback;
      Readback indexReadback;
      CHECK(vertexReadback.FromBuffer(&device, positions, sizeof(expected)) == ReadState::Ready &&
                std::ranges::equal(
                    std::span(reinterpret_cast<const std::byte *>(vertexReadback.Rows()),
                              sizeof(expected)),
                    std::as_bytes(std::span(expected))),
            "main mesh clear preserves accepted independent vertex upload");
      CHECK(indexReadback.FromBuffer(&device, triangles, sizeof(indices)) == ReadState::Ready &&
                std::memcmp(indexReadback.Rows(), indices.data(), sizeof(indices)) == 0,
            "main mesh clear preserves accepted independent index upload");
      CHECK(draw.PiecesStanding() == 1, "independent piece remains owned");
    }
  }
  SDL_Quit();
  return Report();
}
