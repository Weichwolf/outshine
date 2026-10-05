#include "Check.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace {
struct Emission {
  mutable uint32_t Calls = 0;
  std::array<float, 9> Values{0.25f, 0.5f, 0.75f, 0.25f, 0.5f, 0.75f, 0.25f, 0.5f, 0.75f};
};

void WriteEmission(const void *source, float *into, uint32_t floats) {
  const auto &emission = *static_cast<const Emission *>(source);
  ++emission.Calls;
  std::ranges::copy_n(emission.Values.begin(), floats, into);
}
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
    SubjectDraw draw;
    Gpu gpu;
    gpu.Device = device.Get();
    gpu.HdrFormat = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    gpu.SurfaceFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    gpu.Width = gpu.Height = 32;
    CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "scene attachment adds");
    std::string error;
    CHECK(draw.Configure(gpu, error), "native pipelines configure");
    const std::array<SubjectMaterial, 1> materials{};
    CHECK(draw.AppendMaterials(materials, error), "material registers");
    constexpr std::array<float, 9> positions{-1, -1, 0, 1, -1, 0, 0, 1, 0};
    constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
    constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 1, 2};
    Emission emission;
    for (const uint32_t phase : {0u, 1u, 2u, 3u}) {
      DrawList draws;
      const auto layout = phase == 1 ? VertexLayout::Position : VertexLayout::PositionNormal;
      CHECK(draws.Add(DrawItem{.Order = {}, .IndexCount = 3, .Layout = layout}, error),
            "draw adds");
      if (phase == 3) {
        CHECK(draws.Add(DrawItem{.Order = {}, .SourceFirstIndex = 3, .IndexCount = 3}, error),
              "mixed unlit draw adds");
      }
      draws.Compile();
      SubjectMesh mesh;
      mesh.Verts.From = positions.data();
      mesh.Positions = positions;
      mesh.Normals.From = normals.data();
      mesh.Emitted = {.Writes = WriteEmission, .Carrying = &emission};
      mesh.VertexCount = 3;
      mesh.Indices = indices.data();
      mesh.IndexCount = phase == 3 ? 6 : 3;
      mesh.Draws = &draws;
      CHECK(draw.SetMesh(mesh, error), "replacement mesh uploads");
      CHECK(emission.Calls == (phase + 1) / 2, "only unlit consumers execute the emission writer");
      const auto &resident = draw.Resident();
      using Stream = SubjectResidency::Stream;
      if (phase == 0) {
        CHECK(resident.HeldOf(Stream::Emitted) == 0 && !resident.Buffer(Stream::Emitted),
              "lit geometry reserves no unused emission stream");
      } else if (phase == 1 || phase == 3) {
        Readback read;
        constexpr uint32_t vertexBytes = 3 * sizeof(float);
        const uint32_t offset = resident.SubjectVertices().First * vertexBytes;
        CHECK(read.FromBuffer(device.Get(),
                              resident.Buffer(Stream::Emitted).Get(),
                              offset + sizeof(emission.Values)) == ReadState::Ready,
              "unlit emission bytes reach the actual device buffer");
        CHECK(read.Rows() && std::ranges::equal(
                                 std::span(reinterpret_cast<const float *>(read.Rows() + offset),
                                           emission.Values.size()),
                                 emission.Values),
              "unlit and mixed meshes retain every emission component");
      }
    }
  }
  SDL_Quit();
  return Report();
}
