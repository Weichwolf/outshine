#include <span>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "Check.h"
#include "ClusterDag.h"
#include "math/Units.h"
#include "Gpu.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Readback.h"
#include "SubjectDraw.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstring>
#include <string>

namespace {
using outshine::DagCluster;
using outshine::kDagRootErr;
const std::array<DagCluster, 2> firstClusters{{{.SelfCenter = {{0, 0, 0}},
                                                .SelfRadius = 4,
                                                .ParentCenter = {{0, 0, 0}},
                                                .ParentRadius = 8,
                                                .SelfErr = 0,
                                                .ParentErr = kDagRootErr,
                                                .First = 0,
                                                .Count = 3},
                                               {.SelfCenter = {{1, 0, 0}},
                                                .SelfRadius = 4,
                                                .ParentCenter = {{1, 0, 0}},
                                                .ParentRadius = 8,
                                                .SelfErr = 0.125f,
                                                .ParentErr = kDagRootErr,
                                                .First = 3,
                                                .Count = 3}}};
const std::array<DagCluster, 1> secondClusters{{{.SelfCenter = {{4, 5, 6}},
                                                 .SelfRadius = 16,
                                                 .ParentCenter = {{4, 5, 6}},
                                                 .ParentRadius = 32,
                                                 .SelfErr = 0.5f,
                                                 .ParentErr = kDagRootErr,
                                                 .First = 0,
                                                 .Count = 3}}};
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto native = outshine::Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
    outshine::Test::Gpu::NativeOfflineDevice device(*native);
    SubjectDraw draw;
    std::string error;
    outshine::Render::Gpu gpu;
    gpu.Device = &device;
    gpu.HdrFormat = Native::Format::Rgba16Float;
    gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
    gpu.Width = gpu.Height = 32;
    CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "scene attachment adds");
    const bool configured = draw.Configure(gpu).has_value();
    CHECK(configured, "native pipelines configure");
    if (!configured) { return Report(); }
    const std::array<SubjectMaterial, 1> materials{};
    const bool appended = draw.AppendMaterials(materials, error);
    CHECK(appended, "native material registers");
    if (!appended) { return Report(); }
    constexpr std::array<uint32_t, 1> slots{0};
    draw.SetNativePieceSurfaces(slots);
    const std::array<StoredVertex, 6> vertices{
        StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}}),
        StoredVertex::Of({{1, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{2, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{1, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
    constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
    std::array<Mat4, 3> instances{};
    instances[0][12] = -3;
    instances[1][13] = 7;
    instances[2][0] = 2;
    instances[2][14] = 11;
    std::array<Mat4, 3> otherInstances = instances;
    constexpr double otherOffsetM = 50;
    for (Mat4 &row : otherInstances) {
      row[12] += otherOffsetM;
      row[5] = 0.5;
    }
    PieceMesh firstPiece{};
    firstPiece.Verts = vertices;
    firstPiece.Indices = indices;
    firstPiece.Clusters = firstClusters;
    firstPiece.Instances = instances;
    PieceMesh secondPiece{};
    secondPiece.Verts = vertices;
    secondPiece.Indices = std::span(indices).first(3);
    secondPiece.Clusters = secondClusters;
    secondPiece.Instances = otherInstances;
    const auto first = draw.PlacePiece(firstPiece, error);
    const auto second = draw.PlacePiece(secondPiece, error);
    CHECK(first != kNoPiece && second != kNoPiece, "both independent pieces register");
    if (first == kNoPiece || second == kNoPiece) { return Report(); }
    const bool published = draw.HandTables(error) && draw.HandPlacements(false, error);
    CHECK(published, "GPU tables publish");
    if (!published) { return Report(); }
    const auto &resident = draw.Resident();
    constexpr std::array<float, 36> expected{0, 0, 0, 4,  0, 0, 0, 8,  0,      kDagRootErr, 0, 0,
                                             1, 0, 0, 4,  1, 0, 0, 8,  0.125f, kDagRootErr, 0, 0,
                                             4, 5, 6, 16, 4, 5, 6, 32, 0.5f,   kDagRootErr, 0, 0};
    CHECK(resident.HeldOf(SubjectResidency::Stream::ClusterSpheres) == sizeof(expected),
          "sphere storage depends on three source clusters, not nine instance jobs");
    Readback spheres;
    Readback jobs;
    Readback rows;
    CHECK(spheres.FromBuffer(&device,
                             resident.Buffer(SubjectResidency::Stream::ClusterSpheres).get(),
                             sizeof(expected)) == ReadState::Ready,
          "sphere bytes read from the actual GPU buffer");
    CHECK(spheres.Rows() != nullptr &&
              std::ranges::equal(
                  std::span(reinterpret_cast<const std::byte *>(spheres.Rows()), sizeof(expected)),
                  std::as_bytes(std::span(expected))),
          "each piece retains its own exact sphere and error records");
    CHECK(draw.ClusterJobs() == 9 && draw.Drawn().size() == 6,
          "all instance jobs and batches remain present");
    CHECK(jobs.FromBuffer(&device,
                          resident.Buffer(SubjectResidency::Stream::ClusterJobs).get(),
                          36 * sizeof(uint32_t)) == ReadState::Ready,
          "job references read from the actual GPU buffer");
    CHECK(rows.FromBuffer(&device,
                          resident.Buffer(SubjectResidency::Stream::Placements).get(),
                          6 * sizeof(GpuPlacement)) == ReadState::Ready,
          "instance transforms read from the actual GPU buffer");
    if (spheres.Rows() == nullptr || jobs.Rows() == nullptr || rows.Rows() == nullptr) {
      return Report();
    }
    const auto *words = reinterpret_cast<const uint32_t *>(jobs.Rows());
    constexpr std::array<uint32_t, 9> wantedSpheres{0, 1, 0, 1, 0, 1, 2, 2, 2};
    constexpr std::array<uint32_t, 9> wantedBatches{0, 0, 1, 1, 2, 2, 3, 4, 5};
    for (size_t job = 0; job < wantedSpheres.size(); ++job) {
      CHECK(words[job * 4] == wantedSpheres[job] && words[job * 4 + 1] == wantedBatches[job] &&
                words[job * 4 + 2] == draw.Drawn()[wantedBatches[job]].FirstIndex +
                                          (job < 6 ? static_cast<uint32_t>(job % 2) * 3u : 0u) &&
                words[job * 4 + 3] == 3,
            "each instance job references its source sphere and its own batch");
    }
    for (size_t row = 0; row < draw.Drawn().size(); ++row) {
      const auto slot = draw.Drawn()[row].ModelSlot;
      GpuPlacement placement;
      std::memcpy(&placement, rows.Rows() + slot * sizeof(GpuPlacement), sizeof(placement));
      const Mat4 &wanted = row < 3 ? instances[row] : otherInstances[row - 3];
      for (size_t component = 0; component < 16; ++component) {
        CHECK(placement.Current[component] == static_cast<float>(wanted[component]) &&
                  placement.Previous[component] == static_cast<float>(wanted[component]),
              "current and previous instance transforms remain distinct and intact");
      }
    }
    CHECK(draw.SetPieceInstances(second, {}, error) && draw.HandTables(error),
          "an empty instance set leaves no jobs for its piece");
    CHECK(draw.ClusterJobs() == 6 && draw.Drawn().size() == 3, "only first-piece instances remain");
  }
  SDL_Quit();
  return Report();
}
