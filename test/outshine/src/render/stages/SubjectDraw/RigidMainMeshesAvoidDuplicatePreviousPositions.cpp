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
    CHECK(gpu.SceneColours.Add(Resource::SceneLinear) &&
              gpu.SceneColours.Add(Resource::SceneVelocity),
          "radiance and motion attachments add");
    std::string error;
    CHECK(draw.Configure(gpu, error), "velocity-writing pipelines configure");
    const std::array<SubjectMaterial, 1> materials{};
    CHECK(draw.AppendMaterials(materials, error), "material registers");
    constexpr std::array<float, 9> positions{-1, -1, 0, 1, -1, 0, 0, 1, 0};
    constexpr std::array<float, 9> previous{-2, -1, 0, 0, -1, 0, -1, 1, 0};
    constexpr std::array<float, 9> normals{0, 0, 1, 0, 0, 1, 0, 0, 1};
    constexpr std::array<uint32_t, 3> indices{0, 1, 2};
    const std::array<StoredVertex, 3> piece{StoredVertex::Of({{-1, -1, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                            StoredVertex::Of({{1, -1, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                            StoredVertex::Of({{0, 1, 0}}, {{0, 0}}, {{0, 0, 1}})};
    draw.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
    CHECK(draw.PlacePiece({.Verts = piece, .Indices = indices}, error) != kNoPiece,
          "native geometry precedes the main mesh in the vertex arena");
    DrawList draws;
    CHECK(draws.Add({.IndexCount = 3, .Layout = VertexLayout::PositionNormal}, error),
          "main draw adds");
    draws.Compile();
    SubjectMesh mesh;
    mesh.Verts.From = positions.data();
    mesh.Positions = positions;
    mesh.Normals.From = normals.data();
    mesh.Emitted.From = normals.data();
    mesh.VertexCount = 3;
    mesh.Indices = indices.data();
    mesh.IndexCount = 3;
    mesh.Draws = &draws;
    CHECK(draw.SetMesh(mesh, error), "rigid main mesh uploads");
    const auto &resident = draw.Resident();
    using Stream = SubjectResidency::Stream;
    CHECK(resident.SubjectVertices().First > 0, "main mesh follows a native range");
    CHECK(resident.HeldOf(Stream::Previous) == 0 && !resident.Buffer(Stream::Previous),
          "a rigid main mesh allocates no duplicated positions or native-prefix hole");
    SubjectPose pose = mesh;
    pose.PrevVerts.From = previous.data();
    CHECK(draw.SetPose(pose, error), "the same mesh can begin deforming without replacement");
    CHECK(draw.Owned().SubmitPendingUploads(error), "deferred pose uploads submit");
    CHECK(resident.Buffer(Stream::Previous), "deformation acquires its own previous stream");
    if (resident.Buffer(Stream::Previous)) {
      Readback read;
      constexpr uint32_t positionBytes = 3 * sizeof(float);
      const uint32_t offset = resident.SubjectVertices().First * positionBytes;
      CHECK(read.FromBuffer(device.Get(),
                            resident.Buffer(Stream::Previous).Get(),
                            offset + sizeof(previous)) == ReadState::Ready,
            "deforming previous positions reach the GPU");
      CHECK(read.Rows() &&
                std::ranges::equal(std::span(reinterpret_cast<const float *>(read.Rows() + offset),
                                             previous.size()),
                                   previous),
            "every distinct previous-position component survives");
    }
    pose.PrevVerts = {};
    CHECK(draw.SetPose(pose, error), "deformation can stop without replacing the mesh");
  }
  SDL_Quit();
  return Report();
}
