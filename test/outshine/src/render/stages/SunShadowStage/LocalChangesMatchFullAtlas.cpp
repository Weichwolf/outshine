#include "Check.h"
#include "GeometryRenderer.h"
#include "ReadbackTransfer.h"
#include "SunShadowStage.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
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
constexpr SunShadowStage::Overhead kSun{.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}};
constexpr std::array<uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};

struct Atlas {
  SunShadowStage Shadow;
  std::unique_ptr<Texture> Image;

  Atlas(Device &device, GeometryRenderer &geometry, const outshine::Render::Gpu &gpu) {
    Require(Shadow.Configure(geometry, gpu));
    Shadow.Declare(kSun, 8192, true);
    Image = Require(
        device.CreateTexture({.Width = kShadowAtlasPx,
                              .Height = kShadowAtlasPx,
                              .Texels = Format::Depth32Float,
                              .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
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
           DepthAttachment{.Image = {.Image = atlas.Image.get()},
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
    Require(device.Submit(fault.Take()));
    geometry.CommitCrossings();
    submission.Commit();
  }
}

std::vector<std::byte> Read(Device &device, const Atlas &atlas) {
  auto transfer = Require(ReadbackTransfer::Start(
      device, *atlas.Image, {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}));
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
  full.Shadow.Prepare(frame);
  full.Shadow.Invalidate();
  Draw(device, geometry, full, frame);
  CHECK(Read(device, partial) == Read(device, full),
        "all four retained D32 tiles exactly match a full draw of the same held geometry");
  CHECK(std::memcmp(partial.Shadow.Regions().data(),
                    full.Shadow.Regions().data(),
                    sizeof(partial.Shadow.Regions())) == 0,
        "both atlas consumers retain identical regional uniforms");
}

PieceId Place(GeometryRenderer &geometry, float depth, const Mat4 &row = {}) {
  constexpr std::array<Vec3f, 4> points{
      {{{-20, -20, 0}}, {{20, -20, 0}}, {{20, 20, 0}}, {{-20, 20, 0}}}};
  std::array<StoredVertex, 4> vertices;
  for (size_t at = 0; at < vertices.size(); ++at) {
    auto point = points[at];
    point[2] = depth;
    vertices[at] = StoredVertex::Of(point, {}, {{0, 0, 1}});
  }
  PieceMesh piece;
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
  row[12] = 30;
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
  row[12] = -40;
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
  frame.PreViewTranslation[0] -= 255.75;
  Place(geometry, 2, row);
  Compare(device, geometry, partial, full, frame);
  CHECK(Read(device, partial) != retained, "the signed seam case actually changes native pixels");
  geometry.ReleasePiece(background);
  Compare(device, geometry, partial, full, frame);
}

void Materials(Device &device, GeometryRenderer &geometry, Atlas &partial, Atlas &full) {
  FrameContext frame;
  for (size_t axis = 0; axis < 3; ++axis) { frame.PreViewTranslation[axis] = -kAnchor[axis]; }
  Compare(device, geometry, partial, full, frame);
  SubjectMaterial masked;
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
  SubjectMaterial opaque;
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
  auto source = std::make_shared<const PrismGrid>(
      PrismGrid{.Samples = std::make_shared<const std::vector<PrismSpan>>(
                    std::vector<PrismSpan>{{.Lower = -6, .Upper = 8}}),
                .Patches = {{.Columns = 1, .Rows = 1, .PitchX = 32, .PitchZ = 32}}});
  PrismInstance instance;
  const auto id = Require(geometry.PlacePrismGrid(source, std::span(&instance, 1), 0));
  Compare(device, geometry, partial, full, frame);
  const auto revision = geometry.Prisms().ShadowChangesSince(0).Revision;
  const auto generation = geometry.Prisms().Generation();
  const auto uploaded = geometry.Prisms().Uploads().PatchBytes;
  instance.Previous[12] = 100;
  Require(geometry.SetPrismGridInstances(id, std::span(&instance, 1)));
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting() &&
            geometry.Prisms().ShadowChangesSince(revision).Revision == revision &&
            geometry.Prisms().Generation() > generation &&
            geometry.Prisms().Uploads().PatchBytes > uploaded,
        "proven Previous-only records retain shadow coverage while uploading velocity history");
  instance.Current[12] = -35;
  Require(geometry.SetPrismGridInstances(id, std::span(&instance, 1)));
  Compare(device, geometry, partial, full, frame);
  Require(geometry.SetPrismGridInstances(id, {}));
  Compare(device, geometry, partial, full, frame);
  geometry.ReleasePrismGrid(id);
  Compare(device, geometry, partial, full, frame);
  CHECK(!partial.Shadow.Casting(), "retiring already empty prisms preserves the atlas");
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
  CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "the fixture uses the existing subject pipeline");
  GeometryRenderer geometry;
  Require(geometry.Configure(gpu));
  std::string error;
  SubjectMesh mesh;
  mesh.Anchor = kAnchor;
  CheckSceneUpload(geometry.SetMesh(mesh, error), error);
  SubjectMaterial material;
  material.Row.DoubleSided = true;
  CheckSceneUpload(geometry.SetMaterials(std::span(&material, 1), error), error);
  geometry.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
  geometry.SetRegisteredPieceSurfaces(std::array<uint32_t, 1>{0});
  Atlas partial(device, geometry, gpu), full(device, geometry, gpu);
  Changes(device, geometry, partial, full);
  Materials(device, geometry, partial, full);
  Prisms(device, geometry, partial, full);
  return Report();
}
