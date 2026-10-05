#include "Check.h"
#include "ClusterCook.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace {
bool ReplaceMain(outshine::Render::SubjectDraw &draw, std::string &error) {
  using namespace outshine::Render;
  DrawList draws;
  DrawItem item;
  item.IndexCount = 3;
  if (!draws.Add(item, error)) { return false; }
  draws.Compile();
  constexpr std::array<float, 9> points{-100, 0, 0, -100, 1, 0, -99, 0, 0};
  constexpr std::array<float, 9> emitted{};
  constexpr std::array<uint32_t, 3> indices{0, 1, 2};
  SubjectMesh mesh;
  mesh.Verts.From = points.data();
  mesh.Positions = points;
  mesh.Emitted.From = emitted.data();
  mesh.VertexCount = 3;
  mesh.Indices = indices.data();
  mesh.IndexCount = 3;
  mesh.Draws = &draws;
  return draw.SetMesh(mesh, error);
}

void Compare(const outshine::Render::SubjectDraw &draw, SDL_GPUDevice *device) {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  const auto &resident = draw.Resident();
  const auto read = [&](SubjectResidency::Stream stream, Readback &into) {
    return into.FromBuffer(device, resident.Buffer(stream).Get(), resident.HeldOf(stream)) ==
           ReadState::Ready;
  };
  Readback vertices;
  Readback indices;
  Readback jobs;
  Readback spheres;
  CHECK(read(SubjectResidency::Stream::Vertex, vertices) &&
            read(SubjectResidency::Stream::Index, indices) &&
            read(SubjectResidency::Stream::ClusterJobs, jobs) &&
            read(SubjectResidency::Stream::ClusterSpheres, spheres),
        "actual uploaded native vertex, index, job and sphere buffers are readable");
  if (vertices.Rows() == nullptr || indices.Rows() == nullptr || jobs.Rows() == nullptr ||
      spheres.Rows() == nullptr) {
    return;
  }
  const auto *points = reinterpret_cast<const float *>(vertices.Rows());
  const auto *run = reinterpret_cast<const uint32_t *>(indices.Rows());
  const auto *tasks = reinterpret_cast<const uint32_t *>(jobs.Rows());
  const auto *bounds = reinterpret_cast<const float *>(spheres.Rows());
  bool contained = true;
  for (size_t at = 0; at < draw.ClusterJobs(); ++at) {
    const uint32_t *job = tasks + at * 4;
    const float *sphere = bounds + static_cast<size_t>(job[0]) * 12;
    for (size_t corner = 0; corner < job[3]; ++corner) {
      const float *point = points + static_cast<size_t>(run[job[2] + corner]) * 3;
      double squared = 0;
      for (size_t axis = 0; axis < 3; ++axis) {
        const double delta = static_cast<double>(point[axis]) - sphere[axis];
        squared += delta * delta;
      }
      contained &= std::sqrt(squared) <= sphere[3];
    }
  }
  CHECK(contained, "each cluster sphere contains every actual GPU index it selects");
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
    std::string error;
    CHECK(gpu.SceneColours.Add(Resource::SceneLinear) && draw.Configure(gpu, error),
          "native pipelines configure");
    const std::array<SubjectMaterial, 1> materials{};
    CHECK(draw.AppendMaterials(materials, error), "native material registers");
    constexpr std::array<uint32_t, 1> slots{0};
    draw.SetNativePieceSurfaces(slots);
    CHECK(ReplaceMain(draw, error), "initial main mesh shares residency with native pieces");
    constexpr std::array<uint32_t, 6> indices{3, 4, 5, 0, 1, 2};
    constexpr std::array<float, 24> colours{1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1,
                                            0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1};
    PieceId previous = kNoPiece;
    for (size_t at = 0; at < 256; ++at) {
      const auto x = static_cast<float>(at * 10);
      const std::array<StoredVertex, 6> vertices{
          StoredVertex::Of({{x, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x + 1, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x, 1, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x + 2, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x + 3, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{x + 2, 1, 0}}, {{0, 0}}, {{0, 0, 1}})};
      const auto clustered =
          CookClusters({.PositionsM = {reinterpret_cast<const float *>(vertices.data()),
                                       vertices.size() * kStoredVertexFloats},
                        .Indices = indices,
                        .StrideFloats = kStoredVertexFloats},
                       1);
      CHECK(clustered.has_value(), "source triangle clusters cook");
      if (!clustered) { return Report(); }
      PieceMesh piece;
      piece.Verts = vertices;
      piece.Indices = clustered->Index;
      piece.Clusters = clustered->Clusters;
      piece.Colours = colours;
      piece.Textured = true;
      const PieceId placed = draw.PlacePiece(piece, error);
      CHECK(placed != kNoPiece, "distinct native pieces survive vertex and index buffer growth");
      if (at % 2 == 1) { draw.ReleasePiece(previous); }
      previous = placed;
    }
    CHECK(draw.HandTables(error), "cluster job tables publish");
    CHECK(draw.ClusterJobs() == 256, "both clusters of every remaining native piece survive");
    Compare(draw, device.Get());
    CHECK(ReplaceMain(draw, error) && draw.HandTables(error), "main mesh replaces independently");
    Compare(draw, device.Get());
  }
  SDL_Quit();
  return Report();
}
