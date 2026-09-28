#include "Check.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <cassert>
#include <dlfcn.h>
#include <string>

namespace {
bool reject = false;
unsigned rejected = 0;

bool Poison(SDL_GPUDevice *device, SDL_GPUBuffer *buffer, uint32_t bytes) {
  const SDL_GPUTransferBufferCreateInfo info{.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
                                             .size = bytes};
  auto *transfer = SDL_CreateGPUTransferBuffer(device, &info);
  if (!transfer) { return false; }
  auto *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (!mapped) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  std::memset(mapped, 0, bytes);
  SDL_UnmapGPUTransferBuffer(device, transfer);
  auto *commands = SDL_AcquireGPUCommandBuffer(device);
  auto *copy = commands ? SDL_BeginGPUCopyPass(commands) : nullptr;
  if (!copy) {
    if (commands) { SDL_CancelGPUCommandBuffer(commands); }
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    return false;
  }
  const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer};
  const SDL_GPUBufferRegion target{.buffer = buffer, .size = bytes};
  SDL_UploadToGPUBuffer(copy, &source, &target, false);
  SDL_EndGPUCopyPass(copy);
  const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return submitted;
}
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (reject) {
    reject = false;
    ++rejected;
    assert(SDL_CancelGPUCommandBuffer(commands));
    SDL_SetError("injected accepted-upload submission failure");
    return false;
  }
  static const auto original = reinterpret_cast<decltype(&SDL_SubmitGPUCommandBuffer)>(
      dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBuffer"));
  assert(original != nullptr);
  return original(commands);
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    const OwnedDevice device(SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        false,
        nullptr));
    CHECK(device, "GPU device opens");
    if (!device) { return Report(); }
    for (const bool replacement : {false, true}) {
      SubjectDraw draw;
      std::string error;
      Gpu gpu;
      gpu.Device = device.Get();
      gpu.HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
      gpu.SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      gpu.Width = gpu.Height = 32;
      CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "scene attachment adds");
      CHECK(draw.Configure(gpu, error), "native pipelines configure");
      const std::array<SubjectMaterial, 1> materials{};
      CHECK(draw.AppendMaterials(materials, error), "native material registers");
      constexpr std::array<uint32_t, 1> slots{0};
      draw.SetNativePieceSurfaces(slots);
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{2, 3, 4}}, {{0, 0}}, {{0, 1, 0}}),
          StoredVertex::Of({{5, 6, 7}}, {{1, 0}}, {{0, 1, 0}}),
          StoredVertex::Of({{8, 9, 10}}, {{0, 1}}, {{0, 1, 0}})};
      constexpr std::array<uint32_t, 3> indices{0, 1, 2};
      CHECK(draw.PlacePiece(PieceMesh{.Verts = vertices, .Indices = indices}, error) != kNoPiece,
            "independent piece upload is accepted");
      const auto &resident = draw.Resident();
      auto *positions = resident.Buffer(SubjectResidency::Stream::Vertex).Get();
      auto *triangles = resident.Buffer(SubjectResidency::Stream::Index).Get();
      constexpr std::array<float, 9> expected{2, 3, 4, 5, 6, 7, 8, 9, 10};
      CHECK(Poison(device.Get(), positions, sizeof(expected)) &&
                Poison(device.Get(), triangles, sizeof(indices)),
            "destination buffers independently contain known wrong bytes");
      DrawList draws;
      CHECK(draws.Add(DrawItem{.IndexCount = 3}, error), "replacement triangle adds");
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
      const auto failuresBefore = rejected;
      reject = true;
      const auto failed = draw.BeginMesh(mesh);
      CHECK(!failed && !failed.error().empty() && rejected == failuresBefore + 1 && !reject,
            "failed accepted-upload submission rejects mesh replacement");
      CHECK(draw.Generation() == generation && draw.PiecesStanding() == 1,
            "submission failure preserves generation and piece ownership");
      const auto retry = draw.BeginMesh(mesh);
      CHECK(retry.has_value(), "main mesh changes on retry");
      if (retry) { CHECK(draw.FinishMesh(*retry, mesh, error), "main mesh replacement finishes"); }
      positions = resident.Buffer(SubjectResidency::Stream::Vertex).Get();
      triangles = resident.Buffer(SubjectResidency::Stream::Index).Get();
      Readback vertexReadback;
      Readback indexReadback;
      CHECK(vertexReadback.FromBuffer(device.Get(), positions, sizeof(expected)) ==
                    ReadState::Ready &&
                std::memcmp(vertexReadback.Rows(), expected.data(), sizeof(expected)) == 0,
            "main mesh clear preserves accepted independent vertex upload");
      CHECK(indexReadback.FromBuffer(device.Get(), triangles, sizeof(indices)) ==
                    ReadState::Ready &&
                std::memcmp(indexReadback.Rows(), indices.data(), sizeof(indices)) == 0,
            "main mesh clear preserves accepted independent index upload");
      CHECK(draw.PiecesStanding() == 1, "independent piece remains owned");
    }
  }
  SDL_Quit();
  return Report();
}
