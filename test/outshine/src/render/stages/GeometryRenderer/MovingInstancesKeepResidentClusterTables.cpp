#include "Check.h"
#include "GeometryRenderer.h"
#include "Gpu.h"
#include "GpuPlacement.h"
#include "Readback.h"
#include "RenderScene.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using Stream = GeometryResidency::Stream;

constexpr std::array tableStreams{
    Stream::ClusterSpheres, Stream::ClusterJobs, Stream::ClusterBatches, Stream::DrawArguments};

struct Snapshot {
  const Native::Buffer *Buffer = nullptr;
  std::vector<std::byte> Bytes;
};

std::vector<std::byte>
Read(Native::Device &device, const GeometryResidency &resident, Stream which) {
  const auto bytes = resident.HeldOf(which);
  Readback read;
  const auto status = read.FromBuffer(&device, resident.Buffer(which).get(), bytes);
  CHECK(status == ReadState::Ready && read.Rows() != nullptr, "resident GPU bytes read back");
  if (status != ReadState::Ready || read.Rows() == nullptr) { return {}; }
  const auto *first = reinterpret_cast<const std::byte *>(read.Rows());
  return {first, first + bytes};
}

struct Fixture {
  explicit Fixture(Native::Device &device) : Device(device) {}

  Native::Device &Device;
  RenderScene Storage;
  GeometryRenderer Draw{Storage};
  std::string Error;

  bool Configure() {
    Render::Gpu gpu;
    gpu.Device = &Device;
    gpu.HdrFormat = Native::Format::Rgba16Float;
    gpu.SurfaceFormat = Native::Format::Rgba8Unorm;
    gpu.Width = gpu.Height = 32;
    if (!gpu.SceneColours.Add(Resource::SceneLinear)) { return false; }
    const auto configured = Draw.Configure(gpu);
    if (!configured) {
      Error = configured.error().Message;
      return false;
    }
    const std::array<MaterialBinding, 2> materials{};
    if (!Draw.AppendMaterials(materials, Error)) { return false; }
    constexpr std::array<uint32_t, 2> slots{0, 1};
    Draw.SetNativePieceSurfaces(slots);
    return true;
  }

  Native::Result<PieceId> Add(std::span<const Mat4> rows, bool clustered, uint32_t surface) {
    const std::array<StoredVertex, 3> vertices{
        StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
        StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
    constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 1, 2};
    const std::array<DagCluster, 2> clusters{
        {{.SelfRadius = 2, .ParentRadius = 4, .ParentErr = kDagRootErr, .First = 0, .Count = 3},
         {.SelfRadius = 2, .ParentRadius = 4, .ParentErr = kDagRootErr, .First = 3, .Count = 3}}};
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    if (clustered) { piece.Clusters = clusters; }
    piece.Instances = rows;
    piece.Surface = PieceSurface(surface);
    return Draw.PlacePiece(piece);
  }

  bool Publish() { return Draw.HandTables(Error) && Draw.HandPlacements(false, Error); }

  void ResetMetrics() {
    auto &resident = Draw.Residency();
    (void)resident.TakeUploadAttempts();
    (void)resident.TakeUploadBytes();
    (void)resident.TakeBufferAllocationAttempts();
    (void)resident.TakeStagingAllocationAttempts();
    resident.ForgetStagedCount();
  }

  std::array<Snapshot, tableStreams.size()> Tables() {
    std::array<Snapshot, tableStreams.size()> result;
    for (size_t at = 0; at < tableStreams.size(); ++at) {
      result[at].Buffer = Draw.Resident().Buffer(tableStreams[at]).get();
      result[at].Bytes = Read(Device, Draw.Resident(), tableStreams[at]);
    }
    return result;
  }

  void CheckTables(const std::array<Snapshot, tableStreams.size()> &before) {
    for (size_t at = 0; at < tableStreams.size(); ++at) {
      CHECK(before[at].Buffer == Draw.Resident().Buffer(tableStreams[at]).get(),
            "moving instances preserves table buffer ownership");
      CHECK(before[at].Bytes == Read(Device, Draw.Resident(), tableStreams[at]),
            "moving instances preserves exact cluster, batch and argument bytes");
    }
  }
};

void CheckShiftedBounds(std::span<const DrawBatch> before,
                        std::span<const DrawBatch> after,
                        uint32_t material,
                        float shift) {
  CHECK(before.size() == after.size(), "motion preserves the material-sorted batch count");
  for (size_t at = 0; at < std::min(before.size(), after.size()); ++at) {
    const auto &old = before[at];
    const auto &now = after[at];
    CHECK(old.MaterialSlot == now.MaterialSlot && old.ModelSlot == now.ModelSlot &&
              old.FirstJob == now.FirstJob && old.JobCount == now.JobCount &&
              old.FirstIndex == now.FirstIndex && old.IndexCount == now.IndexCount &&
              old.Instances == now.Instances,
          "motion preserves draw identity and source index ranges");
    CHECK(old.WorldSphere.has_value() && now.WorldSphere.has_value(),
          "finite instances retain conservative bounds");
    if (!old.WorldSphere || !now.WorldSphere) { continue; }
    const float moved = now.MaterialSlot == material ? shift : 0;
    CHECK(std::abs((*now.WorldSphere)[0] - (*old.WorldSphere)[0] - moved) < 0.001f &&
              std::abs((*now.WorldSphere)[1] - (*old.WorldSphere)[1]) < 0.001f &&
              std::abs((*now.WorldSphere)[2] - (*old.WorldSphere)[2]) < 0.001f &&
              std::abs((*now.WorldSphere)[3] - (*old.WorldSphere)[3]) < 0.001f,
          "only the moved piece changes its world bounds after material sorting");
  }
}

void CheckPlacements(Fixture &fixture,
                     std::span<const Mat4> plain,
                     std::span<const Mat4> clustered) {
  const auto bytes = Read(fixture.Device, fixture.Draw.Resident(), Stream::Placements);
  CHECK(bytes.size() >= (plain.size() + clustered.size()) * sizeof(GpuPlacement),
        "all source instance transforms remain resident");
  for (const auto &batch : fixture.Draw.Drawn()) {
    const auto source = batch.MaterialSlot == 0 ? clustered : plain;
    const size_t first = batch.MaterialSlot == 0 ? plain.size() : 0;
    for (uint32_t one = 0; one < batch.Instances; ++one) {
      const size_t row = batch.ModelSlot + one;
      if (row >= bytes.size() / sizeof(GpuPlacement) || row < first ||
          row - first >= source.size()) {
        CHECK(false, "draw placement references its own source instance");
        continue;
      }
      GpuPlacement placement;
      std::memcpy(&placement, bytes.data() + row * sizeof(placement), sizeof(placement));
      for (size_t component = 0; component < 16; ++component) {
        const auto expected = static_cast<float>(source[row - first][component]);
        CHECK(placement.Current[component] == expected && placement.Previous[component] == expected,
              "motion reaches GPU placement bytes without changing the existing history contract");
      }
    }
  }
}

void CheckMotion(Fixture &fixture, PieceId piece, std::span<const Mat4> rows, uint32_t material) {
  const auto before = fixture.Tables();
  const std::vector<DrawBatch> batches(fixture.Draw.Drawn().begin(), fixture.Draw.Drawn().end());
  const auto generation = fixture.Draw.Generation();
  const auto revision = fixture.Draw.ShadowChangesSince(0).Revision;
  fixture.Draw.AcknowledgeShadowChanges(revision);
  fixture.ResetMetrics();
  CHECK(fixture.Draw.SetPieceInstances(piece, rows, fixture.Error) &&
            fixture.Draw.HandTables(fixture.Error),
        "same-count instance motion keeps ready tables");
  CHECK(fixture.Draw.Generation() != generation, "motion invalidates cached visibility");
  const auto changes = fixture.Draw.ShadowChangesSince(revision);
  CHECK(changes.Complete && changes.Revision != revision,
        "motion retains explicit local shadow invalidation");
  auto &resident = fixture.Draw.Residency();
  const auto crossings = resident.RecordedCrossings();
  CHECK(resident.TakeUploadBytes() == 0 && resident.TakeUploadAttempts() == 0 &&
            resident.StagedBytes() == 0 && resident.TakeBufferAllocationAttempts() == 0 &&
            resident.TakeStagingAllocationAttempts() == 0,
        "motion causes no table uploads or GPU/staging allocations");
  CHECK(fixture.Draw.HandPlacements(false, fixture.Error), "changed placements publish");
  CHECK(resident.TakeUploadBytes() == 5 * sizeof(GpuPlacement) &&
            resident.TakeUploadAttempts() == 1 && resident.RecordedCrossings() == crossings,
        "only existing placement rows upload, independent of cluster count");
  CheckShiftedBounds(batches, fixture.Draw.Drawn(), material, 100);
  fixture.CheckTables(before);
}

void CheckTopology(Fixture &fixture, PieceId clustered, std::array<Mat4, 3> rows) {
  fixture.ResetMetrics();
  const auto crossings = fixture.Draw.Residency().RecordedCrossings();
  CHECK(fixture.Draw.SetPieceInstances(clustered, std::span(rows).first(1), fixture.Error) &&
            fixture.Publish(),
        "a changed instance count rebuilds the actual topology");
  CHECK(fixture.Draw.ClusterJobs() == 2 && fixture.Draw.Drawn().size() == 2 &&
            fixture.Draw.Residency().StagedBytes() > 0 &&
            fixture.Draw.Residency().RecordedCrossings() == crossings + 4,
        "removed instances leave neither cluster jobs nor stale batches");
  rows[0][12] += 100;
  fixture.ResetMetrics();
  CHECK(fixture.Draw.SetPieceInstances(clustered, std::span(rows).first(1), fixture.Error) &&
            fixture.Draw.HandTables(fixture.Error),
        "motion after topology rebuild uses refreshed batch ownership");
  CHECK(fixture.Draw.Residency().TakeUploadBytes() == 0 && fixture.Publish(),
        "the new one-instance topology remains resident during motion");
  CHECK(fixture.Draw.SetPieceInstances(clustered, {}, fixture.Error) && fixture.Publish(),
        "empty instances remove their batches");
  CHECK(fixture.Draw.ClusterJobs() == 0 && fixture.Draw.Drawn().size() == 1,
        "the other material remains after emptying the clustered piece");
  CHECK(fixture.Draw.SetPieceInstances(clustered, rows, fixture.Error) && fixture.Publish(),
        "restoring instances restores their topology");
  constexpr std::array<uint32_t, 2> swapped{1, 0};
  fixture.Draw.SetNativePieceSurfaces(swapped);
  CHECK(fixture.Publish(), "changed material ordering rebuilds ownership");
  rows[0][0] = std::numeric_limits<double>::quiet_NaN();
  fixture.ResetMetrics();
  CHECK(fixture.Draw.SetPieceInstances(clustered, rows, fixture.Error) &&
            fixture.Draw.HandTables(fixture.Error),
        "unknown transforms update bounds without inventing visibility");
  CHECK(fixture.Draw.Drawn()[1].ModelSlot == 2 && !fixture.Draw.Drawn()[1].WorldSphere &&
            fixture.Draw.Residency().TakeUploadBytes() == 0,
        "unknown moved instance remains fail-open after the material reorder");
  fixture.Draw.SetPiecesVisible(false);
  CHECK(fixture.Publish() && fixture.Draw.Drawn().empty(), "hidden pieces own no batches");
  rows[0][0] = 1;
  CHECK(fixture.Draw.SetPieceInstances(clustered, rows, fixture.Error) && fixture.Publish(),
        "hidden instance motion does not address an old batch range");
  fixture.Draw.SetPiecesVisible(true);
  CHECK(fixture.Publish() && fixture.Draw.Drawn().size() == 4,
        "showing pieces restores all batches");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto native = outshine::Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
    outshine::Test::Gpu::NativeOfflineDevice device(*native);
    Fixture fixture(device);
    const bool configured = fixture.Configure();
    outshine::Test::Gpu::CheckSceneUpload(configured, fixture.Error);
    if (!configured) { return Report(); }
    std::array<Mat4, 2> plain{};
    std::array<Mat4, 3> clustered{};
    plain[1][12] = 10;
    clustered[1][12] = 20;
    clustered[2][12] = 40;
    const auto first = fixture.Add(plain, false, 1);
    const auto second = fixture.Add(clustered, true, 0);
    CHECK(first.has_value() && second.has_value(), "pieces register in reverse material order");
    if (!first || !second) { return Report(); }
    CHECK(fixture.Publish() && fixture.Draw.ClusterJobs() == 6 && fixture.Draw.Drawn().size() == 4,
          "both clustered and grouped nonclustered pieces publish");
    for (auto &row : clustered) { row[12] += 100; }
    CheckMotion(fixture, *second, clustered, 0);
    CheckPlacements(fixture, plain, clustered);
    for (auto &row : plain) { row[12] += 100; }
    CheckMotion(fixture, *first, plain, 1);
    CheckPlacements(fixture, plain, clustered);
    fixture.ResetMetrics();
    CHECK(fixture.Draw.SetPieceInstances(*second, clustered, fixture.Error) && fixture.Publish() &&
              fixture.Draw.Residency().TakeUploadBytes() == 0,
          "identical transforms cause no uploads");
    CheckTopology(fixture, *second, clustered);
  }
  SDL_Quit();
  return Report();
}
