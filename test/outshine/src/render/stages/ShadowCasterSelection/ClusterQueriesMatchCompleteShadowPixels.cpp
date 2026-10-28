#include "Check.h"
#include "GeometryRenderer.h"
#include "Gpu.h"
#include "ReadbackTransfer.h"
#include "RenderScene.h"
#include "StageSubmission.h"
#include "SunShadowPass.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

constexpr float kDistantX = 5000;
constexpr std::array<uint32_t, 12> indices{0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};

struct Fixture {
  RenderScene Storage;
  GeometryRenderer Geometry{Storage};
  SunShadowPass Shadow;
  PieceId Piece = kNoPiece;
  std::string Error;

  Fixture(Device &device, bool clustered, std::span<const Mat4> rows) {
    outshine::Render::Gpu gpu{.Device = &device,
                              .HdrFormat = Format::Rgba16Float,
                              .SurfaceFormat = Format::Rgba8Srgb,
                              .Width = 32,
                              .Height = 32,
                              .SceneColours = {}};
    CHECK(gpu.SceneColours.Add(Resource::SceneHdr), "native geometry output configures");
    Require(Geometry.Configure(gpu));
    MaterialBinding material;
    material.Row.DoubleSided = true;
    CheckSceneUpload(Geometry.SetMaterials(std::span(&material, 1), Error), Error);
    Geometry.SetNativePieceSurfaces(std::array<uint32_t, 1>{0});
    Require(Shadow.Configure(Geometry, gpu));
    Shadow.Declare({.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}}, 8192, true);
    constexpr std::array<Vec3f, 8> points{{{{-20, -20, 4}},
                                           {{20, -20, 4}},
                                           {{20, 20, 4}},
                                           {{-20, 20, 4}},
                                           {{kDistantX - 20, -20, 8}},
                                           {{kDistantX + 20, -20, 8}},
                                           {{kDistantX + 20, 20, 8}},
                                           {{kDistantX - 20, 20, 8}}}};
    std::array<StoredVertex, points.size()> vertices;
    for (size_t at = 0; at < points.size(); ++at) {
      vertices[at] = StoredVertex::Of(points[at], {}, {{0, 0, 1}});
    }
    const std::array<DagCluster, 2> clusters{{{.SelfCenter = {{0, 0, 4}},
                                               .SelfRadius = 32,
                                               .ParentErr = kDagRootErr,
                                               .First = 0,
                                               .Count = 6},
                                              {.SelfCenter = {{kDistantX, 0, 8}},
                                               .SelfRadius = 32,
                                               .ParentErr = kDagRootErr,
                                               .First = 6,
                                               .Count = 6}}};
    PieceMesh piece;
    piece.Verts = vertices;
    piece.Indices = indices;
    piece.Instances = rows;
    if (clustered) { piece.Clusters = clusters; }
    Piece = Require(Geometry.PlacePiece(piece));
  }

  void Draw(Device &device, const FrameContext &frame) {
    CheckSceneUpload(Geometry.HandTables(Error) && Geometry.HandPlacements(false, Error), Error);
    Shadow.Invalidate();
    Shadow.Prepare(frame);
    CHECK(Shadow.Casting(), "a changed source supplies a shadow rendering demand");
    auto commands = Require(device.Begin());
    Require(Geometry.FlushCrossings(commands.get()));
    Require(commands->BeginRenderPass(
        {.Colours = {},
         .Depth = DepthAttachment{.Image = {.Image = Shadow.Atlas()}, .Begin = Load::Clear}}));
    StageSubmission submission;
    Require(Shadow.Encode(frame,
                          {.Commands = commands.get(),
                           .Submission = submission,
                           .Area = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx},
                           .Clip = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}}));
    Require(commands->EndRenderPass());
    Require(device.Submit(std::move(commands)));
    Geometry.CommitCrossings();
    submission.Commit();
  }

  std::vector<std::byte> Pixels(Device &device) const {
    auto read = Require(ReadbackTransfer::Start(
        device, *Shadow.Atlas(), {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}));
    const auto pixels = Require(read.Wait(device, std::chrono::seconds(5)));
    CHECK(pixels.has_value(), "the complete native D32 atlas reads back");
    if (!pixels) { Failed(); }
    return {pixels->begin(), pixels->end()};
  }
};

void Compare(Device &device, Fixture &selected, Fixture &complete, const FrameContext &frame) {
  selected.Draw(device, frame);
  complete.Draw(device, frame);
  const auto actual = selected.Pixels(device);
  const auto original = complete.Pixels(device);
  CHECK(actual == original,
        "all four D32 regions exactly match complete original index draws without cluster culling");
  CHECK(std::ranges::any_of(actual, [](std::byte value) { return value != std::byte{}; }),
        "the image equivalence includes actual rasterized occluder pixels");
}
}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  std::array<Mat4, 2> rows{};
  rows[1][12] = 100000;
  Fixture selected(device, true, rows);
  Fixture complete(device, false, rows);
  FrameContext frame;
  Compare(device, selected, complete, frame);
  rows[0][12] = 40;
  rows[0][0] = 1.5;
  rows[0][4] = 0.75;
  rows[1][12] = 120;
  CheckSceneUpload(selected.Geometry.SetPieceInstances(selected.Piece, rows, selected.Error),
                   selected.Error);
  CheckSceneUpload(complete.Geometry.SetPieceInstances(complete.Piece, rows, complete.Error),
                   complete.Error);
  Compare(device, selected, complete, frame);
  frame.PreViewTranslation = {{-40, 12, 0}};
  Compare(device, selected, complete, frame);
  CheckSceneUpload(selected.Geometry.SetPieceInstances(selected.Piece, {}, selected.Error),
                   selected.Error);
  CheckSceneUpload(complete.Geometry.SetPieceInstances(complete.Piece, {}, complete.Error),
                   complete.Error);
  selected.Draw(device, frame);
  complete.Draw(device, frame);
  const auto empty = selected.Pixels(device);
  CHECK(empty == complete.Pixels(device) &&
            std::ranges::all_of(empty, [](std::byte value) { return value == std::byte{}; }),
        "removed instances leave the same completely empty D32 atlas");
  return Report();
}
