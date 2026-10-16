#include "SubjectDraw.h"
#include "SubjectCullStage.h"
#include "StageSubmission.h"
#include "ReadbackTransfer.h"
#include "NativeOfflineDevice.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <print>
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
constexpr uint32_t kWidth = 32, kHeight = 24;
constexpr size_t kPixels = size_t{kWidth} * kHeight;
constexpr Mat4f kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
constexpr std::array<float, 3> kColour{0.25f, 0.5f, 0.75f};
constexpr std::array<float, 12> kPositions{
    -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f};
constexpr std::array<uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};

struct MeshData {
  std::array<float, 12> Normals{}, Emitted{};
  std::array<float, 16> Tangents{}, Colours{};
  std::array<float, 8> Uv{}, Uv1{};

  MeshData() {
    for (size_t i = 0; i < 4; ++i) {
      Normals[i * 3 + 2] = 1;
      for (size_t c = 0; c < 3; ++c) { Emitted[i * 3 + c] = kColour[c]; }
      Tangents[i * 4] = Tangents[i * 4 + 3] = 1;
      for (size_t c = 0; c < 4; ++c) { Colours[i * 4 + c] = 1; }
      Uv[i * 2] = Uv[i * 2 + 1] = kColour[0];
      Uv1[i * 2] = Uv1[i * 2 + 1] = kColour[2];
    }
  }

  [[nodiscard]] SubjectMesh Mesh(VertexLayout layout, const DrawList &draws) const {
    SubjectMesh mesh;
    mesh.Verts.From = kPositions.data();
    mesh.Positions = kPositions;
    mesh.VertexCount = 4;
    mesh.Indices = kIndices.data();
    mesh.IndexCount = 6;
    mesh.Draws = &draws;
    mesh.Emitted.From = Emitted.data();
    if (CarriesNormal(layout)) { mesh.Normals.From = Normals.data(); }
    if (CarriesUv(layout)) { mesh.Uv.From = Uv.data(); }
    if (CarriesUv1(layout)) { mesh.Uv1.From = Uv1.data(); }
    if (CarriesTangent(layout)) { mesh.Tangents.From = Tangents.data(); }
    if (CarriesColour(layout)) { mesh.Colours.From = Colours.data(); }
    return mesh;
  }
};

struct Targets {
  std::array<std::unique_ptr<Texture>, 5> Colours;
  std::unique_ptr<Texture> Depth;

  explicit Targets(Device &device) {
    constexpr std::array formats{Format::Rgba32Float,
                                 Format::Rgba16Float,
                                 Format::Rgba32Float,
                                 Format::Rgba16Float,
                                 Format::Rg16Float};
    for (size_t i = 0; i < Colours.size(); ++i) {
      Colours[i] = Require(
          device.CreateTexture({.Width = kWidth,
                                .Height = kHeight,
                                .Texels = formats[i],
                                .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    }
    Depth = Require(device.CreateTexture({.Width = kWidth,
                                          .Height = kHeight,
                                          .Texels = Format::Depth32Float,
                                          .Use = TextureUse::DepthAttachment}));
  }
};

constexpr std::array<uint16_t, 4> kBaseValues{0x3400, 0x3800, 0x3a00, 0x3c00};

void Pixels(Device &device, Texture &image, const std::array<uint16_t, 4> &values) {
  auto read = Require(ReadbackTransfer::Start(device, image, {.Width = kWidth, .Height = kHeight}));
  auto data = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(data, "native mesh pixels reach the bounded readback");
  if (!data) { Failed(); }
  std::array<uint16_t, kPixels * 4> pixels{};
  std::memcpy(pixels.data(), data->data(), sizeof(pixels));
  size_t errors = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const bool covered =
          x >= kWidth / 4 && x < kWidth * 3 / 4 && y >= kHeight / 4 && y < kHeight * 3 / 4;
      for (size_t c = 0; c < 4; ++c) {
        const uint16_t expected = covered ? values[c] : 0;
        if (pixels[(size_t{y} * kWidth + x) * 4 + c] != expected) { ++errors; }
      }
    }
  }
  CHECK(errors == 0,
        "every native base-colour pixel matches independent quad coverage and material values");
}

void Draw(Device &device,
          SubjectDraw &subject,
          Targets &targets,
          SubjectCullStage *cull = nullptr,
          const std::array<uint16_t, 4> &values = kBaseValues) {
  auto commands = Require(device.Begin());
  Require(subject.FlushCrossings(commands.get()));
  FrameContext frame;
  frame.Mvp = frame.PrevMvp = kIdentity;
  StageSubmission submission;
  const PassRecording recording{.Commands = commands.get(),
                                .Submission = submission,
                                .Area = {.Width = kWidth, .Height = kHeight},
                                .Clip = {.Width = kWidth, .Height = kHeight}};
  if (cull != nullptr) {
    Require(cull->EncodeCull(frame, recording));
    Require(cull->EncodeScan(recording));
    Require(cull->EncodeCompact(recording));
  }
  std::array<ColourAttachment, 5> attachments;
  for (size_t i = 0; i < attachments.size(); ++i) {
    attachments[i] = {.Image = {.Image = targets.Colours[i].get()}, .Begin = Load::Clear};
  }
  Require(commands->BeginRenderPass(
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = targets.Depth.get()}, .Begin = Load::Clear}}));
  Require(subject.Encode(frame, recording));
  Require(commands->EndRenderPass());
  Require(device.Submit(std::move(commands)));
  subject.CommitCrossings();
  submission.Commit();
  CHECK(subject.EncodedDrawCalls() == 1, "the native mesh retains one draw for its compiled batch");
  Pixels(device, *targets.Colours[3], values);
}

void Indirect(Device &device,
              SubjectDraw &subject,
              Targets &targets,
              const MeshData &source,
              const outshine::Render::Gpu &gpu) {
  std::array<DagCluster, 2> clusters;
  constexpr size_t sphereFloats = 12;
  std::array<float, 2 * sphereFloats> spheres{};
  for (size_t i = 0; i < clusters.size(); ++i) {
    clusters[i].SelfCenter = clusters[i].ParentCenter = {{0, 0, 0.5f}};
    clusters[i].SelfRadius = clusters[i].ParentRadius = 1;
    clusters[i].ParentErr = kDagRootErr;
    clusters[i].First = static_cast<uint32_t>(i * 3);
    clusters[i].Count = 3;
    spheres[i * 12 + 2] = spheres[i * 12 + 6] = 0.5f;
    spheres[i * 12 + 3] = spheres[i * 12 + 7] = 1;
    spheres[i * 12 + 9] = kDagRootErr;
  }
  std::string error;
  DrawList draws;
  CheckSceneUpload(draws.Add({.Order = {},
                              .IndexCount = 6,
                              .Layout = VertexLayout::PositionNormalUvUv1TangentColour,
                              .ClusterCount = 2},
                             error),
                   error);
  draws.Compile();
  draws.JobsAddress(clusters);
  auto mesh = source.Mesh(VertexLayout::PositionNormalUvUv1TangentColour, draws);
  mesh.Clusters = clusters;
  mesh.ClusterSpheres = spheres;
  CheckSceneUpload(subject.SetMesh(mesh, error), error);
  SubjectCullStage stage;
  Require(stage.Configure({.Resident = &subject.Resident(),
                           .AnchorM = subject.AnchorM(),
                           .Jobs = subject.ClusterJobs(),
                           .BatchRows = subject.ClusterBatchRows(),
                           .Generation = subject.Generation(),
                           .GroundGeneration = subject.Ground().Generation()},
                          gpu));
  stage.Projects(kHeight);
  Draw(device, subject, targets, &stage);
  CHECK(stage.JobsSwept() == 2,
        "both real mesh clusters pass through the three native culling stages");
}

void PieceUv1(Device &device, SubjectDraw &subject, Targets &targets, const MeshData &source) {
  std::string error;
  CheckSceneUpload(subject.SetMesh(SubjectMesh{}, error), error);
  constexpr std::array<uint8_t, 8> texels{255, 0, 0, 255, 0, 0, 255, 255};
  SubjectMaterial material;
  material.Row.Unlit = true;
  material.Row.DoubleSided = true;
  material.Row.BaseColour = {{kColour[0], kColour[1], kColour[2], 1}};
  material.Colour = {.Rgba = texels.data(),
                     .Width = 2,
                     .Height = 1,
                     .Magnify = SubjectFilter::Nearest,
                     .Minify = SubjectFilter::Nearest,
                     .Mip = SubjectMip::None,
                     .Uv = {},
                     .Set = UvSet::Uv1};
  CheckSceneUpload(subject.SetMaterials(std::span(&material, 1), error), error);
  std::array<StoredVertex, 4> vertices;
  for (size_t i = 0; i < vertices.size(); ++i) {
    vertices[i] =
        StoredVertex::Of({{kPositions[i * 3], kPositions[i * 3 + 1], kPositions[i * 3 + 2]}},
                         {{kColour[0], kColour[0]}},
                         {{0, 0, 1}});
  }
  const std::array<uint32_t, 1> slots{0};
  subject.SetNativePieceSurfaces(slots);
  PieceMesh piece;
  piece.Verts = vertices;
  piece.Indices = kIndices;
  piece.Uv1 = source.Uv1;
  piece.Surface = PieceSurface(0);
  piece.Textured = true;
  PieceMesh missing = piece;
  missing.Uv1 = {};
  CHECK(subject.PlacePiece(missing, error) == kNoPiece &&
            error == "piece surface requires a complete second UV run",
        "the integrated material contract rejects a missing second UV run before allocation");
  const auto id = subject.PlacePiece(piece, error);
  CHECK(id != kNoPiece, "the actual piece allocator carries authored second UVs");
  if (id == kNoPiece) { Failed(); }
  CheckSceneUpload(subject.HandTables(error), error);
  constexpr std::array<uint16_t, 4> blue{0, 0, 0x3a00, 0x3c00};
  Draw(device, subject, targets, nullptr, blue);
  subject.ReleasePiece(id);
}

}

int main() {
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  outshine::Render::Gpu gpu{.Device = &device,
                            .HdrFormat = Format::Rgba32Float,
                            .SurfaceFormat = Format::Rgba8Srgb,
                            .Width = kWidth,
                            .Height = kHeight,
                            .SceneColours = {}};
  for (auto resource : {Resource::SceneHdr,
                        Resource::SceneShadingNormal,
                        Resource::SceneSurfaceIdentity,
                        Resource::SceneSurfaceBase,
                        Resource::SceneSurfaceMetalRough}) {
    CHECK(gpu.SceneColours.Add(resource), "the full native material attachment order is accepted");
  }
  SubjectDraw subject;
  Require(subject.Configure(gpu));
  CHECK(subject.PipelineCount() > 0,
        "all existing vertex/domain/coverage variants compile natively");
  SubjectMaterial material;
  material.Row.DoubleSided = true;
  material.Row.BaseColour = {{kColour[0], kColour[1], kColour[2], 1}};
  std::string error;
  CheckSceneUpload(subject.SetMaterials(std::span(&material, 1), error), error);
  Targets targets(device);
  const MeshData source;
  for (const auto &row : kVertexLayouts) {
    DrawList draws;
    CheckSceneUpload(draws.Add({.Order = {}, .IndexCount = 6, .Layout = row.Layout}, error), error);
    draws.Compile();
    auto mesh = source.Mesh(row.Layout, draws);
    CheckSceneUpload(subject.SetMesh(mesh, error), error);
    Draw(device, subject, targets);
  }
  Indirect(device, subject, targets, source, gpu);
  PieceUv1(device, subject, targets, source);
  return Report();
}
