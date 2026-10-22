#include "Check.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "test/harness/render/NativeCommandsProxy.h"
#include "test/harness/render/NativeDeviceProxy.h"
#include <SDL3/SDL.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr float kOutside = 100000;
constexpr size_t kVertexFloats = size_t{6} * 3;
constexpr size_t kSphereFloats = size_t{2} * 12;
constexpr double kEyeHeightM = 100, kNearM = 0.1, kFarM = 20000;

struct Counts {
  uint64_t ShadowIndices = 0;
  uint64_t ShadowPackets = 0;
};

class Commands final : public outshine::Test::Gpu::NativeCommandsProxy {
public:
  Commands(std::unique_ptr<Native::Commands> commands, Counts &counts)
      : NativeCommandsProxy(std::move(commands)), Counts_(counts) {}

  Native::Result<void> DrawIndexed(const Native::IndexedDrawDescription &description) override {
    if (description.State.Area.Width == kShadowTilePx &&
        description.State.Area.Height == kShadowTilePx) {
      ++Counts_.ShadowPackets;
      Counts_.ShadowIndices += uint64_t{description.Range.Count} * description.Range.Instances;
    }
    return NativeCommandsProxy::DrawIndexed(description);
  }

private:
  Counts &Counts_;
};

class Device final : public outshine::Test::Gpu::NativeDeviceProxy {
public:
  Device(std::unique_ptr<Native::Device> device, Counts &counts)
      : NativeDeviceProxy(*device), Device_(std::move(device)), Counts_(counts) {}

  Native::Result<std::unique_ptr<Native::Commands>> Begin() override {
    auto begun = Backing().Begin();
    if (!begun) { return std::unexpected(begun.error()); }
    return std::make_unique<Commands>(std::move(*begun), Counts_);
  }

  Native::Result<Native::Completion> Submit(std::unique_ptr<Native::Commands> commands) override {
    auto *wrapped = static_cast<Commands *>(commands.get());
    return Backing().Submit(wrapped->Take());
  }

private:
  std::unique_ptr<Native::Device> Device_;
  Counts &Counts_;
};

void Exercise() {
  Counts counts;
  const GpuSubmission submission{.Context = &counts,
                                 .Create = [](void *context, Native::DeviceOptions options)
                                     -> Native::Result<std::unique_ptr<Native::Device>> {
                                   auto actual = Native::CreateDevice(options);
                                   if (!actual) { return std::unexpected(actual.error()); }
                                   return std::make_unique<Device>(std::move(*actual),
                                                                   *static_cast<Counts *>(context));
                                 }};
  Geometry geometry;
  const auto surface = geometry.addSurface("caster", Material{});
  if (!surface) { return; }
  const auto part = geometry.addPart("caster", *surface);
  if (!part) { return; }
  std::array<float, kVertexFloats> positions{
      -5, -5, 0, 0, 5, 0, 5, -5, 0, kOutside - 5, -5, 0, kOutside, 5, 0, kOutside + 5, -5, 0};
  constexpr std::array<uint32_t, 6> indices{0, 1, 2, 3, 4, 5};
  CHECK(geometry.setPositions(*part, positions) && geometry.setTriangles(*part, indices),
        "the world owns two original caster triangles");
  Core::Declaration declaration;
  declaration.InitialGeometry = &geometry;
  declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
  declaration.Outputs = {"surface", "shadowAtlas"};
  declaration.DrawsSky = true;
  SceneRenderer renderer(submission);
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
        "caster world opens");
  if (!scene) { return; }
  Viewpoint eye;
  eye.EyeM = {{0, 0, kEyeHeightM}};
  eye.YfovRad = 1;
  eye.ZNearM = kNearM;
  eye.ZFarM = kFarM;
  scene->Eye(eye);
  renderer.SetShadowFrame({{0, 0, 1}}, {{0, 1, 0}}, 16384, true, 0);
  std::array<DagCluster, 2> clusters{};
  clusters[0].Count = clusters[1].Count = 3;
  clusters[1].First = 3;
  clusters[0].SelfRadius = clusters[1].SelfRadius = 8;
  clusters[1].SelfCenter[0] = kOutside;
  std::array<float, kSphereFloats> spheres{};
  spheres[3] = spheres[15] = 8;
  spheres[12] = kOutside;
  DrawList draws;
  DrawItem item;
  item.IndexCount = 6;
  item.ClusterCount = 2;
  CHECK(draws.Add(item, error), "the original index ranges form one clustered batch");
  draws.Compile();
  draws.JobsAddress(clusters);
  std::array<float, kVertexFloats> emitted{};
  SubjectMesh mesh;
  mesh.Verts.From = positions.data();
  mesh.Emitted.From = emitted.data();
  mesh.Positions = positions;
  mesh.VertexCount = 6;
  mesh.Indices = indices.data();
  mesh.IndexCount = indices.size();
  mesh.Draws = &draws;
  mesh.Clusters = clusters;
  mesh.ClusterSpheres = spheres;
  CHECK(renderer.SetGeometryMesh(mesh, error), "qualified current mesh uploads");
  counts.ShadowIndices = counts.ShadowPackets = 0;
  CHECK(scene->Draw(error) && counts.ShadowIndices > 0 && counts.ShadowIndices < 24 &&
            renderer.ShadowCastCount() == counts.ShadowPackets,
        "the original far cluster is rejected before native vertex work");
  const auto selected = counts.ShadowIndices;
  for (size_t vertex = 0; vertex < 3; ++vertex) {
    positions[vertex * 3] += kOutside;
    positions[(vertex + 3) * 3] -= kOutside;
  }
  const SubjectPose pose = mesh;
  CHECK(renderer.SetGeometryPose(pose, error), "a vertex pose moves geometry outside old spheres");
  counts.ShadowIndices = counts.ShadowPackets = 0;
  CHECK(scene->Draw(error) && counts.ShadowIndices > selected &&
            renderer.ShadowCastCount() == counts.ShadowPackets,
        "the actual pose invalidates old cluster rejection and restores full caster coverage");
  spheres[0] = kOutside;
  spheres[12] = 0;
  clusters[0].SelfCenter[0] = kOutside;
  clusters[1].SelfCenter[0] = 0;
  CHECK(renderer.SetGeometryMesh(mesh, error), "a new mesh supplies current source bounds");
  counts.ShadowIndices = counts.ShadowPackets = 0;
  CHECK(scene->Draw(error) && counts.ShadowIndices == selected &&
            renderer.ShadowCastCount() == counts.ShadowPackets,
        "qualified mesh replacement restores fine selection after a pose");
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for actual native draws");
  if (SDL_WasInit(SDL_INIT_VIDEO) != 0) { Exercise(); }
  SDL_Quit();
  return Report();
}
