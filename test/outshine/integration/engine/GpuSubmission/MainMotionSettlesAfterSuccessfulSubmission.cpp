#include "Check.h"
#include "GpuSubmission.h"
#include "MeshInput.h"
#include "DrawList.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"

#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kPixels = 64;
constexpr double kHalfWidthM = 2;
constexpr double kNearM = 0.1;
constexpr float kTolerance = 0.001f;

struct Submission {
  bool Reject = false;

  GpuSubmission Functions() {
    GpuSubmission result;
    result.Context = this;
    result.Create = [](void *, Native::DeviceOptions options) {
      options.Validation = true;
      return Native::CreateDevice(options);
    };
    result.Submit =
        [](void *context,
           Native::Device *device,
           std::unique_ptr<Native::Commands> commands) -> Native::Result<Native::Completion> {
      auto &self = *static_cast<Submission *>(context);
      if (self.Reject) {
        self.Reject = false;
        return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Driver,
                                             .Message = "rejected surface-motion frame"});
      }
      return device->Submit(std::move(commands));
    };
    return result;
  }
};

struct Fixture {
  Submission submission;
  SceneRenderer renderer{submission.Functions()};
  std::unique_ptr<Core::RuntimeScene> scene;
  std::string error;
  MeshInput mesh;
  DrawList draws;
  bool ordered = false;
  static constexpr std::array<float, 12> positions{
      -1.4f, -1.4f, 0, 1.4f, -1.4f, 0, 1.4f, 1.4f, 0, -1.4f, 1.4f, 0};
  static constexpr std::array<float, 12> normals{0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1};
  static constexpr std::array<float, 12> emitted{};
  static constexpr std::array<uint32_t, 6> indices{0, 1, 2, 0, 2, 3};

  bool Open() {
    Geometry base;
    const auto material = base.addSurface("moving-main", Material{});
    if (!material) { return false; }
    const auto part = base.addPart("outside-view", *material);
    constexpr std::array<float, 9> outside{100, 0, 0, 101, 0, 0, 100, 1, 0};
    constexpr std::array<uint32_t, 3> triangle{0, 1, 2};
    if (!part || !base.setPositions(*part, outside) || !base.setTriangles(*part, triangle)) {
      return false;
    }
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = kPixels;
    declaration.Stages = {"opaqueGeometry"};
    declaration.Outputs = {"sceneDepth", "sceneVelocity"};
    if (!Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error)) { return false; }
    Viewpoint eye;
    eye.EyeM = {{0, 0, 4}};
    eye.Kind = CameraKind::Orthographic;
    eye.XMagM = eye.YMagM = kHalfWidthM;
    eye.ZNearM = kNearM;
    eye.ZFarM = 10;
    scene->Eye(eye);
    if (!scene->Draw(error)) { return false; }
    DrawItem batch{};
    batch.IndexCount = 6;
    batch.Layout = VertexLayout::PositionNormal;
    if (!draws.Add(batch, error)) { return false; }
    draws.Compile();
    mesh.Anchor = renderer.LastSubmittedCamera().GeometryAnchorM;
    mesh.Verts.From = positions.data();
    mesh.Positions = positions;
    mesh.Normals.From = normals.data();
    mesh.Emitted.From = emitted.data();
    mesh.VertexCount = 4;
    mesh.Indices = indices.data();
    mesh.IndexCount = 6;
    mesh.Draws = &draws;
    return renderer.SetGeometryMesh(mesh, error) &&
           renderer.SetGeometryPlacements(nullptr, 0, error);
  }

  bool Move(double metres) {
    Mat4 model;
    model[12] = metres;
    if (ordered) {
      renderer.MoveGeometryPlacement(0, model);
      return true;
    }
    mesh.Model = model;
    return renderer.SetGeometryVertexInput(mesh, error);
  }

  uint32_t CheckFrame(float expected) {
    renderer.ForgetGeometryStaging();
    CHECK(scene->Draw(error), "the real native surface frame submits");
    renderer.WaitForGpu();
    std::vector<float> depth;
    std::vector<float> velocity;
    CHECK(renderer.ReadDepth(depth) == ReadState::Ready && depth.size() == kPixels * kPixels,
          "depth proves the native surface probe exists");
    CHECK(renderer.ReadSceneVelocity(velocity) == ReadState::Ready &&
              velocity.size() == kPixels * kPixels * 2,
          "the actual native motion attachment is readable");
    if (depth.size() != kPixels * kPixels || velocity.size() != kPixels * kPixels * 2) { return 0; }
    constexpr size_t probe = (kPixels / 2) * kPixels + kPixels / 2;
    CHECK(depth[probe] > 0, "the moving Main mesh covers the same probe");
    std::println("motion expected={} actual=({}, {})",
                 expected,
                 velocity[2 * probe],
                 velocity[2 * probe + 1]);
    CHECK(std::abs(velocity[2 * probe] - expected) < kTolerance &&
              std::abs(velocity[2 * probe + 1]) < kTolerance,
          "native motion uses the last submitted placement and NDC units");
    return renderer.GeometryBytesStaged();
  }
};
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for native Main motion");
  {
    for (bool ordered : {false, true}) {
      Fixture fixture;
      CHECK(fixture.Open(), "a real native surface scene opens");
      if (fixture.scene) {
        fixture.ordered = ordered;
        std::println("ORDERED {}", ordered);
        if (ordered) {
          Mat4 first;
          CHECK(fixture.renderer.SetGeometryPlacements(first.data(), 1, fixture.error),
                "ordered slots initialize");
        }
        CHECK(fixture.Move(0), "the first placement is present");
        fixture.CheckFrame(0);
        CHECK(fixture.Move(0.25), "the submitted Main mesh translates");
        fixture.CheckFrame(0.125f);
        CHECK(fixture.Move(0.5) && fixture.Move(0.75), "two updates precede one submission");
        fixture.CheckFrame(0.25f);
        CHECK(fixture.CheckFrame(0) > 0, "first stillstand retires GPU placement motion once");
        CHECK(fixture.CheckFrame(0) == 0, "second stillstand reuses settled GPU placements");
        CHECK(fixture.Move(1), "a rejected frame has a candidate placement");
        fixture.submission.Reject = true;
        CHECK(!fixture.scene->Draw(fixture.error), "a real native frame submission is rejected");
        CHECK(fixture.Move(1.25),
              "retry updates the candidate without publishing the rejected pose");
        fixture.CheckFrame(0.25f);
        CHECK(fixture.CheckFrame(0) > 0, "successful retry retires its motion once");
        CHECK(fixture.CheckFrame(0) == 0, "retry stillstand reuses settled GPU placements");
      }
    }
  }
  SDL_Quit();
  return Report();
}
