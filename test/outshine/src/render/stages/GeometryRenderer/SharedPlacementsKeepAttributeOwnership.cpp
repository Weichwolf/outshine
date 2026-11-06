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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <span>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

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
    if (!gpu.SceneColours.Add(Resource::SceneLinear) || !Draw.Configure(gpu)) { return false; }
    const std::array<MaterialBinding, 1> materials{};
    if (!Draw.AppendMaterials(materials, Error)) { return false; }
    const std::array<uint32_t, 1> slots{0};
    Draw.SetNativePieceSurfaces(slots);
    return true;
  }

  Native::Result<PieceId>
  Add(std::shared_ptr<const Mat4> placement, bool coloured, std::span<const Mat4> rows = {}) {
    const std::array<StoredVertex, 3> vertices{StoredVertex::Of({{0, 0, 0}}, {}, {{0, 0, 1}}),
                                               StoredVertex::Of({{1, 0, 0}}, {}, {{0, 0, 1}}),
                                               StoredVertex::Of({{0, 1, 0}}, {}, {{0, 0, 1}})};
    const std::array<uint32_t, 3> indices{0, 1, 2};
    const std::array<DagCluster, 1> clusters{
        {{.SelfRadius = 2, .ParentRadius = 4, .ParentErr = kDagRootErr, .First = 0, .Count = 3}}};
    const std::array<float, 12> colours{1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Clusters = clusters;
    piece.Colours = coloured ? std::span(colours) : std::span<const float>{};
    piece.Placement = std::move(placement);
    piece.Instances = rows;
    return Draw.PlacePiece(piece);
  }

  bool Publish() { return Draw.HandTables(Error) && Draw.HandPlacements(false, Error); }

  GpuPlacement Placement(uint32_t slot) {
    const auto &resident = Draw.Resident();
    Readback read;
    CHECK(read.FromBuffer(&Device,
                          resident.Buffer(GeometryResidency::Stream::Placements).get(),
                          resident.HeldOf(GeometryResidency::Stream::Placements)) ==
              ReadState::Ready,
          "actual placement data reaches the native GPU buffer");
    GpuPlacement result;
    if (read.Rows() != nullptr) {
      std::memcpy(&result, read.Rows() + size_t{slot} * sizeof(result), sizeof(result));
    }
    return result;
  }
};

void CheckSharedBatches(Fixture &fixture) {
  auto &draw = fixture.Draw;
  const auto shared = std::make_shared<const Mat4>();
  Mat4 conflicting;
  conflicting[12] = 10;
  CHECK(!fixture.Add(shared, true, std::span(&conflicting, 1)),
        "conflicting instance rows cannot acquire a shared placement");
  Mat4 invalid;
  invalid[0] = std::numeric_limits<double>::infinity();
  CHECK(!fixture.Add(std::make_shared<const Mat4>(invalid), true),
        "nonfinite shared placements are rejected before residency");
  const auto first = fixture.Add(shared, true);
  const auto second = fixture.Add(shared, true);
  CHECK(first && second && fixture.Publish(), "two independently resident pages publish");
  if (!first || !second || draw.Drawn().size() != 1) {
    CHECK(false, "one owner and compatible contiguous streams produce one root/batch");
    return;
  }
  CHECK(draw.Drawn()[0].IndexCount == 6 && draw.Drawn()[0].JobCount == 2 && draw.ClusterJobs() == 2,
        "grouping preserves every index and source cluster");
  const auto separateOwner = fixture.Add(std::make_shared<const Mat4>(), true);
  CHECK(separateOwner && fixture.Publish() && draw.Drawn().size() == 2,
        "equal matrix values do not merge independent owners");
  const auto plain = fixture.Add({}, false);
  const auto lateColour = fixture.Add(shared, true);
  CHECK(plain && lateColour && fixture.Publish() && draw.Drawn().size() == 4,
        "an intervening position-only allocation changes colour addressing");
  if (draw.Drawn().size() != 4) { return; }
  const auto firstColour = fixture.Placement(draw.Drawn()[0].ModelSlot);
  const auto latest = std::ranges::max_element(draw.Drawn(), {}, &DrawBatch::FirstIndex);
  const auto laterColour = fixture.Placement(latest->ModelSlot);
  CHECK(firstColour.ColourOffset != laterColour.ColourOffset,
        "shared transforms cannot alias distinct attribute stream offsets");
  Mat4 moved;
  moved[12] = 10;
  CHECK(draw.SetPieceInstances(*second, std::span(&moved, 1), fixture.Error) && fixture.Publish() &&
            draw.Drawn().size() == 5,
        "moving one member detaches it before rebuilding root and model ownership");
  CHECK(draw.SetPieceInstances(*second, {}, fixture.Error) && fixture.Publish() &&
            draw.Drawn().size() == 4,
        "hiding the detached member retains the rest of the group");
  CHECK(draw.SetPieceInstances(*second, std::span(shared.get(), 1), fixture.Error, shared) &&
            fixture.Publish() && draw.Drawn().size() == 4,
        "an explicit shared owner can restore compatible grouping");
  CHECK(draw.SetPiecePasses(*first, GeometryPasses::Main, fixture.Error) && fixture.Publish() &&
            draw.Drawn().size() == 5,
        "pass eligibility separates otherwise compatible pages");
  CHECK(draw.SetPiecePasses(*first, GeometryPasses::MainAndShadow, fixture.Error) &&
            fixture.Publish() && draw.Drawn().size() == 4,
        "restoring pass eligibility restores the complete group");
  draw.ReleasePiece(*first);
  CHECK(fixture.Publish() && draw.Drawn().size() == 4,
        "evicting the first member preserves every remaining page");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    auto native = Test::Gpu::Require(Native::CreateDevice({.Validation = true}));
    Test::Gpu::NativeOfflineDevice device(*native);
    Fixture fixture(device);
    CHECK(fixture.Configure(), "native renderer and material configure");
    CheckSharedBatches(fixture);
  }
  SDL_Quit();
  return Report();
}
