#include "MeshInput.h"
#include "RenderScene.h"
#include "Check.h"
#include "GeometryRenderer.h"
#include "ReadbackTransfer.h"
#include "SunShadowPass.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <bit>
#include <algorithm>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr Vec3 kAnchor{{1000000, 2000000, 3000000}};
constexpr SunShadowPass::Overhead kSun{.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}};
constexpr std::array<uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};

struct Atlas {
  SunShadowPass Shadow;

  Atlas(GeometryRenderer &geometry, const outshine::Render::Gpu &gpu) {
    Require(Shadow.Configure(geometry, gpu));
    Shadow.Declare(kSun, 8192, true);
  }
};

struct RejectDraw final : NativeCommandsProxy {
  using NativeCommandsProxy::NativeCommandsProxy;
  bool Reject = false;

  Result<void> DrawIndexed(const IndexedDrawDescription &draw) override {
    if (!Reject) { return NativeCommandsProxy::DrawIndexed(draw); }
    return std::unexpected(Error{.Kind = ErrorKind::Driver, .Message = "rejected local patch"});
  }
};

struct RejectSubmit final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;

  Result<Completion> Submit([[maybe_unused]] std::unique_ptr<Commands> commands) override {
    return std::unexpected(
        Error{.Kind = ErrorKind::Driver, .Message = "rejected local submission"});
  }
};

struct RejectPipeline final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;

  Result<std::unique_ptr<GraphicsPipeline>>
  CreateGraphicsPipeline([[maybe_unused]] const GraphicsDescription &description) override {
    return std::unexpected(
        Error{.Kind = ErrorKind::Driver, .Message = "shadow pipeline replacement refused"});
  }
};

void Draw(Device &device,
          GeometryRenderer &geometry,
          Atlas &atlas,
          const FrameContext &frame,
          bool accept = true,
          bool reject = false) {
  std::string error;
  CheckSceneUpload(geometry.HandTables(error), error);
  CheckSceneUpload(geometry.HandPlacements(false, error), error);
  atlas.Shadow.Prepare(frame);
  if (!atlas.Shadow.Casting()) { return; }
  auto commands = Require(device.Begin());
  Require(geometry.FlushCrossings(commands.get()));
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth =
           DepthAttachment{.Image = {.Image = atlas.Shadow.Atlas()},
                           .Begin = atlas.Shadow.PreservesAtlas() ? Load::Load : Load::Clear}}));
  StageSubmission submission;
  RejectDraw fault(std::move(commands));
  fault.Reject = reject;
  auto *target = &fault;
  const auto result =
      atlas.Shadow.Encode(frame,
                          {.Commands = target,
                           .Submission = submission,
                           .Area = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx},
                           .Clip = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}});
  if (reject) {
    CHECK(!result && result.error().Message == "rejected local patch",
          "the dirty patch propagates actual native recording failure");
    return;
  }
  Require(result);
  Require(target->EndRenderPass());
  if (accept) {
    const auto submitted = device.Submit(fault.Take());
    if (!submitted) {
      CHECK(submitted.error().Message == "rejected local submission",
            "the injected device submission failure reaches the native caller");
      return;
    }
    geometry.CommitCrossings();
    submission.Commit();
  }
}

std::vector<std::byte> Read(Device &device, const Atlas &atlas) {
  auto transfer = Require(ReadbackTransfer::Start(
      device, *atlas.Shadow.Atlas(), {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}));
  auto pixels = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(pixels, "the complete real D32 atlas is readable");
  if (!pixels) { Failed(); }
  return {pixels->begin(), pixels->end()};
}

void Compare(Device &device,
             GeometryRenderer &geometry,
             Atlas &partial,
             Atlas &full,
             const FrameContext &frame) {
  Draw(device, geometry, partial, frame);
  const auto *const retained = partial.Shadow.Atlas();
  Require(partial.Shadow.Allocate(device));
  CHECK(partial.Shadow.Atlas() == retained && partial.Shadow.HasSubmittedData(),
        "repeated target demand preserves the submitted atlas and its valid history");
  full.Shadow.Prepare(frame);
  full.Shadow.Invalidate();
  Draw(device, geometry, full, frame);
  CHECK(Read(device, partial) == Read(device, full),
        "all four retained D32 tiles exactly match a full draw of the same held geometry");
  CHECK(std::ranges::equal(partial.Shadow.Regions(),
                           full.Shadow.Regions(),
                           [](const auto &a, const auto &b) {
                             return std::bit_cast<std::array<uint32_t, 4>>(a.Transform) ==
                                        std::bit_cast<std::array<uint32_t, 4>>(b.Transform) &&
                                    std::bit_cast<std::array<uint32_t, 4>>(a.Depth) ==
                                        std::bit_cast<std::array<uint32_t, 4>>(b.Depth) &&
                                    std::bit_cast<std::array<uint32_t, 4>>(a.Atlas) ==
                                        std::bit_cast<std::array<uint32_t, 4>>(b.Atlas);
                           }),
        "both atlas consumers retain identical regional uniforms");
}

PieceId Place(GeometryRenderer &geometry,
              float depth,
              const Mat4 &row = {},
              GeometryPasses passes = GeometryPasses::MainAndShadow) {
  constexpr std::array<Vec3f, 4> points{
      {{{-20, -20, 0}}, {{20, -20, 0}}, {{20, 20, 0}}, {{-20, 20, 0}}}};
  std::array<StoredVertex, 4> vertices;
  for (size_t at = 0; at < vertices.size(); ++at) {
    auto point = points[at];
    point[2] = depth;
    vertices[at] = StoredVertex::Of(point, {}, {{0, 0, 1}});
  }
  PieceMesh piece;
  piece.Passes = passes;
  piece.Verts = vertices;
  piece.Indices = kIndices;
  piece.Row = row;
  piece.MaxInstances = 2;
  return Require(geometry.PlacePiece(piece));
}

void Changes(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  const auto background = Place(geometry, -8);
  const auto moving = Place(geometry, 0);
  std::string error;
  Compare(device, geometry, partial, full, frame);
  Mat4 row;
  constexpr double kMovedX = 30;
  constexpr double kRetriedX = -40;
  constexpr double kSeamShiftM = 255.75;
  row[12] = kMovedX;
  CheckSceneUpload(geometry.SetPieceInstances(moving, std::span(&row, 1), error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(partial.Shadow.PreservesAtlas(), "a local move preserves unchanged atlas pixels");
  CheckSceneUpload(geometry.SetPieceInstances(moving, {}, error), error);
  Compare(device, geometry, partial, full, frame);
  geometry.ReleasePiece(moving);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "releasing an already hidden piece changes no shadow content");
  const auto born = Place(geometry, 4, row);
  Compare(device, geometry, partial, full, frame);
  row[12] = kRetriedX;
  CheckSceneUpload(geometry.SetPieceInstances(born, std::span(&row, 1), error), error);
  const auto before = Read(device, partial);
  Draw(device, geometry, partial, frame, false);
  CHECK(!partial.Shadow.HasSubmittedData() && Read(device, partial) == before,
        "abandoned local recording preserves accepted pixels and leaves the atlas invalid");
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(), "abandoned local updates retry through full fallback");
  geometry.ReleasePiece(born);
  Draw(device, geometry, partial, frame, false, true);
  CHECK(!partial.Shadow.HasSubmittedData(), "failed patch recording never publishes cache success");
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(), "failed patch recording retries through full fallback");
  const auto retained = Read(device, partial);
  frame.PreViewTranslation[0] -= kSeamShiftM;
  Place(geometry, 2, row);
  Compare(device, geometry, partial, full, frame);
  CHECK(Read(device, partial) != retained, "the signed seam case actually changes native pixels");
  geometry.ReleasePiece(background);
  Compare(device, geometry, partial, full, frame);
}

void PiecePasses(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  Compare(device, geometry, partial, full, frame);
  const auto before = Read(device, partial);
  const auto piece = Place(geometry, 0, {}, GeometryPasses::Main);
  std::string error;
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting() && Read(device, partial) == before,
        "main-only publication retains every shadow texel without updating the atlas");
  Mat4 row;
  row[12] = 5;
  CheckSceneUpload(geometry.SetPieceInstances(piece, std::span(&row, 1), error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting() && Read(device, partial) == before,
        "main-only motion does not invalidate retained shadows");
  CheckSceneUpload(geometry.SetPiecePasses(piece, GeometryPasses::Shadow, error), error);
  Compare(device, geometry, partial, full, frame);
  const auto shadow = Read(device, partial);
  CHECK(shadow != before, "a shadow-only piece changes the actual retained depth atlas");
  CheckSceneUpload(geometry.SetPiecePasses(piece, GeometryPasses::MainAndShadow, error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting() && Read(device, partial) == shadow,
        "adding main participation preserves shadow-only depth without recomputation");
  CheckSceneUpload(geometry.SetPiecePasses(piece, GeometryPasses::Shadow, error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "removing main participation preserves the shadow cache");
  CheckSceneUpload(geometry.SetPiecePasses(piece, GeometryPasses::Main, error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(Read(device, partial) == before,
        "removing shadow participation invalidates retained depth");
  geometry.ReleasePiece(piece);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "main-only retirement preserves the shadow cache");
  CHECK(!geometry.SetPiecePasses(piece, GeometryPasses::Shadow, error),
        "a released piece cannot change participation");
}

void Materials(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  Compare(device, geometry, partial, full, frame);
  MaterialBinding masked;
  masked.Row.Alpha = AlphaMode::Masked;
  masked.Row.BaseColour[3] = 0;
  masked.Row.DoubleSided = true;
  std::string error;
  CheckSceneUpload(geometry.AppendMaterials(std::span(&masked, 1), error), error);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "unused material append and table application are metadata");
  geometry.SetNativePieceSurfaces(std::array<uint32_t, 1>{1});
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(), "live opacity mapping requires full invalidation");
  MaterialBinding opaque;
  opaque.Row.DoubleSided = true;
  CheckSceneUpload(geometry.SetMaterials(std::span(&opaque, 1), error), error);
  geometry.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(), "material replacement cannot retain old coverage");
  geometry.SetPiecesVisible(false);
  Compare(device, geometry, partial, full, frame);
  geometry.SetPiecesVisible(true);
  Compare(device, geometry, partial, full, frame);
}

void Prisms(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  const auto source = std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::vector<PrismSpan>{{.Lower = -6, .Upper = 8}}),
                .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 32, .PitchZ = 32}}});
  PrismInstance instance;
  const auto id = Require(geometry.PlacePrismGrid(source, std::span(&instance, 1), 0));
  Compare(device, geometry, partial, full, frame);
  const auto revision = geometry.Prisms().ShadowChangesSince(0).Revision;
  const auto generation = geometry.Prisms().Generation();
  const auto uploaded = geometry.Prisms().Uploads().PatchBytes;
  constexpr float kPreviousX = 100;
  constexpr float kMovedX = -35;
  instance.Previous[12] = kPreviousX;
  Require(geometry.SetPrismGridInstances(id, std::span(&instance, 1)));
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting() &&
            geometry.Prisms().ShadowChangesSince(revision).Revision == revision &&
            geometry.Prisms().Generation() > generation &&
            geometry.Prisms().Uploads().PatchBytes > uploaded,
        "proven Previous-only records retain shadow coverage while uploading velocity history");
  instance.Current[12] = kMovedX;
  Require(geometry.SetPrismGridInstances(id, std::span(&instance, 1)));
  Compare(device, geometry, partial, full, frame);
  Require(geometry.SetPrismGridInstances(id, {}));
  Compare(device, geometry, partial, full, frame);
  geometry.ReleasePrismGrid(id);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "retiring already empty prisms preserves the atlas");
}

void SubmissionAndReaders(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  const auto piece = Place(geometry, 5);
  Compare(device, geometry, partial, full, frame);
  Mat4 row;
  constexpr double kReaderOffsetM = 90;
  row[12] = kReaderOffsetM;
  std::string error;
  CheckSceneUpload(geometry.SetPieceInstances(piece, std::span(&row, 1), error), error);
  const auto before = Read(device, partial);
  RejectSubmit fault(device);
  Draw(fault, geometry, partial, frame);
  CHECK(!partial.Shadow.HasSubmittedData() && Read(device, partial) == before,
        "rejected device submission retains accepted GPU pixels and cannot commit the atlas");
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(), "rejected submission retries all atlas regions");
  row[12] = -kReaderOffsetM;
  CheckSceneUpload(geometry.SetPieceInstances(piece, std::span(&row, 1), error), error);
  Draw(device, geometry, full, frame);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.PreservesAtlas(),
        "a second consumer retiring the retained interval requires the older reader to rebuild");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  outshine::Render::Gpu gpu{.Device = &device,
                            .HdrFormat = Format::Rgba16Float,
                            .SurfaceFormat = Format::Rgba8Srgb,
                            .Width = 32,
                            .Height = 32,
                            .SceneColours = {}};
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr),
        "the fixture uses the existing geometry pipeline");
  RenderScene geometryStorage;
  GeometryRenderer geometry{geometryStorage};
  Require(geometry.Configure(gpu));
  std::string error;
  MeshInput mesh;
  mesh.Anchor = kAnchor;
  CheckSceneUpload(geometry.SetMesh(mesh, error), error);
  MaterialBinding material;
  material.Row.DoubleSided = true;
  CheckSceneUpload(geometry.SetMaterials(std::span(&material, 1), error), error);
  geometry.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
  geometry.SetRegisteredPieceSurfaces(std::array<uint32_t, 1>{0});
  Atlas partial(geometry, gpu);
  Atlas full(geometry, gpu);
  Changes(device, geometry, partial, full);
  PiecePasses(device, geometry, partial, full);
  Materials(device, geometry, partial, full);
  const auto *const atlas = partial.Shadow.Atlas();
  const bool submitted = partial.Shadow.HasSubmittedData();
  RejectPipeline rejected(device);
  const auto reconfigured =
      partial.Shadow.Configure(geometry, {.Device = &rejected, .SceneColours = {}});
  CHECK(!reconfigured && reconfigured.error().Kind == ErrorKind::Driver &&
            partial.Shadow.Atlas() == atlas && partial.Shadow.HasSubmittedData() == submitted,
        "failed pipeline replacement retains the owned atlas and accepted cache");
  Prisms(device, geometry, partial, full);
  SubmissionAndReaders(device, geometry, partial, full);
  return Report();
}
