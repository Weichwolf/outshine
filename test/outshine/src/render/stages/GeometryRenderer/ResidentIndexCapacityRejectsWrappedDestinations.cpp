#include "Check.h"
#include "GeometryRenderer.h"
#include "RenderScene.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <span>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  using namespace outshine::Test::Gpu;
  auto actual = Require(Native::CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  RenderScene storage;
  GeometryRenderer draw{storage};
  Render::Gpu gpu;
  gpu.Device = &device;
  gpu.HdrFormat = Native::Format::Rgba16Float;
  gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
  gpu.Width = gpu.Height = 4;
  CHECK(gpu.SceneColours.Add(Resource::SceneLinear), "the native surface profile is admitted");
  Require(draw.Configure(gpu));
  const std::array<MaterialBinding, 1> materials{};
  std::string error;
  CHECK(draw.AppendMaterials(materials, error), "the resident test material is admitted");
  if (!error.empty()) { return Report(); }
  constexpr std::array<uint32_t, 1> slots{0};
  draw.SetNativePieceSurfaces(slots);
  const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
                                             StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
  constexpr uint32_t indexCount = 1048578;
  constexpr uint32_t instances = 1024;
  std::vector<uint32_t> indices(indexCount);
  for (uint32_t at = 0; at < indices.size(); ++at) { indices[at] = at % 3; }
  const std::array<DagCluster, 1> clusters{DagCluster{
      .SelfRadius = 2, .ParentRadius = 2, .ParentErr = kDagRootErr, .Count = indexCount}};
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = indices;
  piece.Clusters = clusters;
  piece.MaxInstances = instances;
  const auto id = Require(draw.PlacePiece(piece));
  CHECK(draw.HandTables(error) && draw.HandPlacements(false, error),
        "one complete source product has an ordinary four-MiB destination");
  if (!error.empty()) { return Report(); }
  const auto &resident = draw.Resident();
  const auto before = resident.Buffer(GeometryResidency::Stream::DrawIndex)->Description().Bytes;
  CHECK(before == uint64_t{indexCount} * sizeof(uint32_t),
        "the positive control allocates the actual full index capacity");
  (void)draw.Residency().TakeBufferAllocationAttempts();
  (void)draw.Residency().TakeUploadAttempts();
  const std::vector<Mat4> rows(instances);
  CHECK(draw.SetPieceInstances(id, rows, error), "the declared instance capacity is valid");
  CHECK(!draw.HandTables(error) && !error.empty() && draw.ClusterJobs() == 0,
        "4GiB plus8192bytes is rejected before wrapping the actual renderer destination");
  CHECK(draw.Residency().TakeBufferAllocationAttempts() == 0 &&
            draw.Residency().TakeUploadAttempts() == 0 &&
            resident.Buffer(GeometryResidency::Stream::DrawIndex)->Description().Bytes == before,
        "failed aggregate admission neither allocates the wrapped target nor mutates GPU tables");
  error.clear();
  CHECK(draw.SetPieceInstances(id, std::span(rows).first(1), error) && draw.HandTables(error),
        "the accepted product remains usable after restoring a valid instance count");
  CHECK(draw.Hierarchy().IndexWords == indexCount && draw.Hierarchy().TemplateVisits == 2,
        "recovery preserves exact capacity and validates the shared template only once");
  return Report();
}
