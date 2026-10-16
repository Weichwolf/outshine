#include "LightVisibilityStage.h"
#include "SubjectDraw.h"
#include "ReadbackTransfer.h"
#include "NativeOfflineDevice.h"
#include "NativeDeviceProxy.h"
#include "NativeCommandsProxy.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <ios>
#include <string_view>
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
constexpr float kHalfM = 80;
constexpr float kDepthTolerance = 0.000001f;
constexpr double kDistantCasterM = 6000;
constexpr float kInstanceStepM = 160;
constexpr float kMaterialAlpha = 0.75f;
constexpr std::array<float, 12> kPositions{
    -kHalfM, -kHalfM, 0, kHalfM, -kHalfM, 0, kHalfM, kHalfM, 0, -kHalfM, kHalfM, 0};
constexpr std::array<uint32_t, 6> kIndices{0, 1, 2, 0, 2, 3};
constexpr std::array<uint8_t, 8> kMask{255, 255, 255, 255, 255, 255, 255, 0};
constexpr Mat4 kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
constexpr size_t kTilePixels = size_t{kShadowTilePx} * kShadowTilePx;
constexpr auto kSun = LightVisibilityStage::Overhead{.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}};

struct MaskCase {
  uint32_t Uv = 0;
  bool Tinted = false, Thin = false, Reverse = false;
};

struct Scene {
  SubjectDraw Subjects;
  LightVisibilityStage Shadow;
  std::unique_ptr<Texture> Atlas;

  explicit Scene(Device &device) {
    outshine::Render::Gpu gpu{.Device = &device,
                              .HdrFormat = Format::Rgba16Float,
                              .SurfaceFormat = Format::Rgba8Srgb,
                              .Width = 32,
                              .Height = 32,
                              .SceneColours = {}};
    CHECK(gpu.SceneColours.Add(Resource::SceneHdr),
          "the shadow consumer keeps the existing subject pipeline");
    Require(Subjects.Configure(gpu));
    Require(Shadow.Configure(Subjects, gpu));
    Shadow.Declare(kSun, 256);
    Atlas = Require(
        device.CreateTexture({.Width = kShadowAtlasPx,
                              .Height = kShadowAtlasPx,
                              .Texels = Format::Depth32Float,
                              .Use = TextureUse::DepthAttachment | TextureUse::CopySource}));
  }
};

void SetMask(Scene &scene, MaskCase mask) {
  SubjectMaterial material;
  material.Row.Alpha = AlphaMode::Masked;
  material.Row.BaseColour[3] = kMaterialAlpha;
  material.Row.DoubleSided = true;
  if (mask.Uv > 0) {
    material.Colour = {.Rgba = kMask.data(),
                       .Width = 2,
                       .Height = 1,
                       .WrapU = SubjectWrap::ClampToEdge,
                       .Magnify = SubjectFilter::Nearest,
                       .Minify = SubjectFilter::Nearest,
                       .Mip = SubjectMip::None,
                       .Uv = {},
                       .Set = mask.Uv == 2 ? UvSet::Uv1 : UvSet::Uv0};
    if (mask.Reverse) { material.Colour.Uv.M = {{-1, 0, 1, 0, 1, 0}}; }
  }
  std::string error;
  CheckSceneUpload(scene.Subjects.SetMesh(SubjectMesh{}, error), error);
  CheckSceneUpload(scene.Subjects.SetMaterials(std::span(&material, 1), error), error);
  std::array<float, 12> emitted{};
  std::array<float, 16> colours{};
  for (size_t i = 0; i < 4; ++i) {
    for (size_t c = 0; c < 4; ++c) { colours[i * 4 + c] = c == 3 && mask.Thin ? 0.5f : 1; }
  }
  constexpr std::array<float, 8> uv{0, 0, 1, 0, 1, 1, 0, 1};
  constexpr std::array<float, 8> uv1{1, 0, 0, 0, 0, 1, 1, 1};
  VertexRunsCarried carried;
  carried.Uv = mask.Uv > 0;
  carried.Uv1 = mask.Uv > 1;
  carried.Colour = mask.Tinted;
  VertexLayout layout;
  CHECK(LayoutOf(carried, layout), "the shadow test uses an existing vertex layout");
  const uint32_t instances = mask.Tinted ? 2u : 1u;
  std::array<double, 32> models{};
  std::ranges::copy(kIdentity.Column, models.begin());
  std::ranges::copy(kIdentity.Column, models.begin() + 16);
  models[16 + 12] = kInstanceStepM;
  CheckSceneUpload(scene.Subjects.SetPlacements(models.data(), instances, error), error);
  DrawList draws;
  CheckSceneUpload(draws.Add({.Order = {.Surface = material.State()},
                              .IndexCount = 6,
                              .Layout = layout,
                              .Instances = instances},
                             error),
                   error);
  draws.Compile();
  SubjectMesh mesh;
  mesh.Verts.From = kPositions.data();
  mesh.Positions = kPositions;
  mesh.Emitted.From = emitted.data();
  mesh.VertexCount = 4;
  mesh.Indices = kIndices.data();
  mesh.IndexCount = 6;
  mesh.Draws = &draws;
  if (mask.Uv > 0) { mesh.Uv.From = uv.data(); }
  if (mask.Uv > 1) { mesh.Uv1.From = uv1.data(); }
  if (mask.Tinted) { mesh.Colours.From = colours.data(); }
  CheckSceneUpload(scene.Subjects.SetMesh(mesh, error), error);
}

void Record(Device &device, Scene &scene, const FrameContext &frame, bool submit) {
  scene.Shadow.Prepare(frame);
  if (!scene.Shadow.Casting()) { return; }
  auto commands = Require(device.Begin());
  Require(scene.Subjects.FlushCrossings(commands.get()));
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{.Image = {.Image = scene.Atlas.get()}, .Begin = Load::Clear}}));
  StageSubmission submission;
  Require(scene.Shadow.Encode(frame,
                              {.Commands = commands.get(),
                               .Submission = submission,
                               .Area = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx},
                               .Clip = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}}));
  Require(commands->EndRenderPass());
  if (submit) {
    Require(device.Submit(std::move(commands)));
    scene.Subjects.CommitCrossings();
    submission.Commit();
  }
}

std::vector<float> Read(Device &device, Scene &scene, size_t region = 0) {
  auto read =
      Require(ReadbackTransfer::Start(device,
                                      *scene.Atlas,
                                      {.X = static_cast<uint32_t>(region % 2) * kShadowTilePx,
                                       .Y = static_cast<uint32_t>(region / 2) * kShadowTilePx,
                                       .Width = kShadowTilePx,
                                       .Height = kShadowTilePx}));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "actual regional depth completes within the explicit offline read budget");
  if (!bytes) { Failed(); }
  std::vector<float> pixels(kTilePixels);
  std::memcpy(pixels.data(), bytes->data(), pixels.size() * sizeof(float));
  return pixels;
}

struct WorldPoint {
  double X, Y;
};

bool CastsAt(WorldPoint point, MaskCase mask) {
  if ((mask.Tinted && mask.Thin) || std::abs(point.Y) >= kHalfM) { return false; }
  const uint32_t instances = mask.Tinted ? 2u : 1u;
  for (uint32_t instance = 0; instance < instances; ++instance) {
    const double localX = point.X - static_cast<double>(instance) * kInstanceStepM;
    if (std::abs(localX) >= kHalfM) { continue; }
    double u = (localX + kHalfM) / (2 * kHalfM);
    if (mask.Uv == 2) { u = 1 - u; }
    if (mask.Reverse) { u = 1 - u; }
    if (mask.Uv == 0 || u < 0.5) { return true; }
  }
  return false;
}

void Coverage(Scene &scene, std::span<const float> pixels, MaskCase mask) {
  const auto &matrix = scene.Shadow.RegionProjection(0);
  const bool covered = !mask.Tinted || !mask.Thin;
  size_t errors = 0;
  size_t occupied = 0;
  for (int y = 0; y < kShadowTilePx; ++y) {
    for (int x = 0; x < kShadowTilePx; ++x) {
      const double worldX = ((2 * (x + 0.5) / kShadowTilePx - 1) - matrix[12]) / matrix[0];
      const double worldY = ((1 - 2 * (y + 0.5) / kShadowTilePx) - matrix[13]) / matrix[5];
      const bool casts = CastsAt({.X = worldX, .Y = worldY}, mask);
      const auto value =
          pixels[size_t{static_cast<uint32_t>(y)} * kShadowTilePx + static_cast<uint32_t>(x)];
      const float expected = casts ? static_cast<float>(matrix[14]) : 0;
      if (std::abs(value - expected) > kDepthTolerance) { ++errors; }
      if (value > 0) { ++occupied; }
    }
  }
  CHECK(errors == 0,
        "every shadow texel matches independent quad, material/vertex alpha, UV selection and "
        "transform");
  CHECK((occupied > 0) == covered, "masked alpha preserves the expected occupied or empty atlas");
}

void Masks(Device &device, Scene &scene) {
  for (uint32_t uv = 0; uv <= 2; ++uv) {
    for (const bool tinted : {false, true}) {
      for (const bool thin : {false, true}) {
        for (const bool reverse : {false, true}) {
          if (uv == 0 && reverse) { continue; }
          const MaskCase mask{.Uv = uv, .Tinted = tinted, .Thin = thin, .Reverse = reverse};
          SetMask(scene, mask);
          const FrameContext frame;
          Record(device, scene, frame, true);
          CHECK(scene.Shadow.HasSubmittedData() && scene.Shadow.CastBatches() == 1,
                "one actual masked batch casts every declared instance");
          const auto pixels = Read(device, scene);
          Coverage(scene, pixels, mask);
          scene.Shadow.Prepare(frame);
          CHECK(scene.Shadow.Cached(), "unchanged caster data retains its accepted atlas");
        }
      }
    }
  }
}

void Cancel(Device &device, Scene &scene) {
  const MaskCase mask{.Uv = 2};
  SetMask(scene, mask);
  const FrameContext frame;
  const auto previous = Read(device, scene);
  Record(device, scene, frame, false);
  CHECK(!scene.Shadow.HasSubmittedData(), "an abandoned shadow recording publishes no cache state");
  CHECK(Read(device, scene) == previous,
        "abandoning the real command buffer preserves the previous GPU depths");
  Record(device, scene, frame, true);
  Coverage(scene, Read(device, scene), mask);
  CHECK(scene.Shadow.HasSubmittedData(),
        "the abandoned native shadow can be recorded and accepted again");
}

void Regions(Device &device, Scene &scene, std::string_view output) {
  SetMask(scene, {.Tinted = true});
  Mat4 distant = kIdentity;
  distant[12] = kDistantCasterM;
  scene.Subjects.MovePlacement(1, distant);
  std::string error;
  CheckSceneUpload(scene.Subjects.HandPlacements(false, error), error);
  scene.Shadow.Declare(kSun, 8192, true);
  FrameContext frame;
  Record(device, scene, frame, true);
  CHECK(scene.Shadow.RegionCount() == 4 && scene.Shadow.CastBatches() == 4,
        "four real atlas tiles draw the complete independent caster set");
  std::array<std::vector<float>, 4> before;
  for (size_t region = 0; region < before.size(); ++region) {
    before[region] = Read(device, scene, region);
    CHECK(std::ranges::any_of(before[region], [](float value) { return value > 0; }),
          "each unmodified 2048 by 2048 shadow region contains actual geometry");
  }
  bool far = false;
  for (int y = 0; y < kShadowTilePx; ++y) {
    for (int x = 0; x < kShadowTilePx / 4; ++x) {
      far = far ||
            before[3][size_t{static_cast<uint32_t>(y)} * kShadowTilePx + static_cast<uint32_t>(x)] >
                0;
    }
  }
  CHECK(far, "the far tile retains the six-kilometre offscreen instance");
  if (!output.empty()) {
    std::ofstream file(std::string(output) + "-atlas.bin", std::ios::binary);
    for (const auto &tile : before) {
      file.write(reinterpret_cast<const char *>(tile.data()),
                 static_cast<std::streamsize>(tile.size() * sizeof(float)));
    }
    file.flush();
    CHECK(file, "optional lossless atlas diagnostic retains every actual regional depth");
  }

  frame.Mvp[0] = 0;
  frame.Mvp[2] = 1;
  frame.Mvp[8] = -1;
  scene.Shadow.Prepare(frame);
  CHECK(scene.Shadow.Cached() && !scene.Shadow.Casting(),
        "camera rotation performs no new shadow draw for unchanged world coverage");
  for (size_t region = 0; region < before.size(); ++region) {
    CHECK(Read(device, scene, region) == before[region],
          "camera rotation preserves every accepted regional depth byte");
  }
  scene.Shadow.CastsBelow(0);
  Record(device, scene, frame, true);
  CHECK(scene.Shadow.CastBatches() == 0,
        "the existing caster-row limit excludes every ordinary subject row");
  for (size_t region = 0; region < before.size(); ++region) {
    CHECK(std::ranges::all_of(Read(device, scene, region), [](float value) { return value == 0; }),
          "a valid empty caster set clears each whole atlas tile");
  }
  scene.Shadow.CastsBelow(kNoBatch);
}

struct RefuseDraw final : NativeCommandsProxy {
  using NativeCommandsProxy::NativeCommandsProxy;
  uint32_t Calls = 0;

  Result<void> DrawIndexed(const IndexedDrawDescription &description) override {
    if (++Calls == 2) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver, .Message = "injected shadow draw failure"});
    }
    return NativeCommandsProxy::DrawIndexed(description);
  }
};

void RejectedRecording(Device &device, Scene &scene) {
  const FrameContext frame;
  Record(device, scene, frame, true);
  const auto previous = Read(device, scene);
  scene.Shadow.Invalidate();
  scene.Shadow.Prepare(frame);
  {
    RefuseDraw commands(Require(device.Begin()));
    Require(commands.BeginRenderPass(
        {.Colours = {},
         .Depth = DepthAttachment{.Image = {.Image = scene.Atlas.get()}, .Begin = Load::Clear}}));
    StageSubmission submission;
    const auto rejected =
        scene.Shadow.Encode(frame,
                            {.Commands = &commands,
                             .Submission = submission,
                             .Area = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx},
                             .Clip = {.Width = kShadowAtlasPx, .Height = kShadowAtlasPx}});
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Driver && commands.Calls == 2,
          "a native draw failure in the second region reaches the caller");
  }
  CHECK(!scene.Shadow.HasSubmittedData(),
        "a partly recorded atlas never becomes a submitted shadow cache");
  CHECK(Read(device, scene) == previous,
        "discarding a failed regional recording preserves accepted GPU data");
  Record(device, scene, frame, true);
  CHECK(scene.Shadow.HasSubmittedData(),
        "every region can be retried after the rejected native draw");
}

struct RefusePipeline final : NativeDeviceProxy {
  using NativeDeviceProxy::NativeDeviceProxy;

  Result<std::unique_ptr<GraphicsPipeline>>
  CreateGraphicsPipeline([[maybe_unused]] const GraphicsDescription &description) override {
    return std::unexpected(
        Error{.Kind = ErrorKind::Driver, .Message = "injected shadow pipeline failure"});
  }
};

void ConfigureRollback(Device &device, Scene &scene) {
  const FrameContext frame;
  scene.Shadow.Prepare(frame);
  CHECK(scene.Shadow.Cached(), "the accepted state is cached before the configuration fault");
  RefusePipeline rejected(device);
  const auto changed =
      scene.Shadow.Configure(scene.Subjects, {.Device = &rejected, .SceneColours = {}});
  CHECK(!changed && changed.error().Kind == ErrorKind::Driver && scene.Shadow.Cached(),
        "failed native pipeline creation preserves the accepted cache and pipeline set");
  SubjectDraw other;
  scene.Shadow.Binds(other);
  CHECK(!scene.Shadow.HasSubmittedData(),
        "rebinding another subject owner explicitly invalidates the previous world atlas");
  scene.Shadow.Binds(scene.Subjects);
}

void GroundOnly(Device &device, Scene &scene) {
  std::string error;
  CheckSceneUpload(scene.Subjects.SetMesh(SubjectMesh{}, error), error);
  CheckSceneUpload(scene.Subjects.SetPlacements(nullptr, 0, error), error);
  auto &ground = scene.Subjects.Ground();
  std::vector<float> heights(GroundLattice::kPageNodes, 3);
  const auto page = Require(ground.PlacePage(heights));
  GroundTile tile;
  tile.Instance.Corners = {{-64, 64, 64, 64, -64, -64, 64, -64}};
  tile.Instance.Page = static_cast<float>(page);
  tile.Instance.StepE = tile.Instance.StepN = 4;
  tile.LowM = tile.HighM = 3;
  Require(ground.SetInstances(std::span(&tile, 1), {}));
  scene.Shadow.Declare(kSun, 256);
  const FrameContext frame;
  Record(device, scene, frame, true);
  const auto pixels = Read(device, scene);
  const auto &matrix = scene.Shadow.RegionProjection(0);
  size_t errors = 0;
  size_t occupied = 0;
  for (int y = 0; y < kShadowTilePx; ++y) {
    for (int x = 0; x < kShadowTilePx; ++x) {
      const double worldX = ((2 * (x + 0.5) / kShadowTilePx - 1) - matrix[12]) / matrix[0];
      const double worldY = ((1 - 2 * (y + 0.5) / kShadowTilePx) - matrix[13]) / matrix[5];
      const bool inside = std::abs(worldX) < 64 && std::abs(worldY) < 64;
      const float expected = inside ? static_cast<float>(matrix[10] * 3 + matrix[14]) : 0;
      const float value =
          pixels[size_t{static_cast<uint32_t>(y)} * kShadowTilePx + static_cast<uint32_t>(x)];
      if (std::abs(value - expected) > kDepthTolerance) { ++errors; }
      if (value > 0) { ++occupied; }
    }
  }
  CHECK(errors == 0 && occupied > 0 && scene.Shadow.CastBatches() == 0,
        "ground-only shadows match the physical flat sheet and never its render skirts");
  ground.SetDrawEnabled(false);
  Record(device, scene, frame, true);
  CHECK(std::ranges::all_of(Read(device, scene), [](float value) { return value == 0; }),
        "ground generation invalidates and clears the accepted atlas without subject vertices");
}

}

int main(int argc, char **argv) {
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  Scene scene(device);
  Masks(device, scene);
  Cancel(device, scene);
  Regions(device, scene, argc > 1 ? argv[1] : std::string_view{});
  RejectedRecording(device, scene);
  ConfigureRollback(device, scene);
  GroundOnly(device, scene);
  return Report();
}
