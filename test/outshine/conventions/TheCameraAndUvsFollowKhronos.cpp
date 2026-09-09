#include <cmath>
#include <numbers>
#include <limits>
#include <scenario/Scenario.h>
#include <scene/UvTransform.h>
#include "Check.h"
#include "Lens.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto near = [](double a, double b) { return std::abs(a - b) < 1.0e-10; };
  const UvTransform transformed = UvTransformOf(
      {.OffsetUv = {{0, 1}}, .RotationRad = std::numbers::pi / 2, .ScaleUv = {{0.5, 0.5}}});
  const UvPoint u = transformed.Apply({.U = 1, .V = 0});
  const UvPoint v = transformed.Apply({.U = 0, .V = 1});
  // Native rotation is algebraic: +U rotates toward +V.
  // The glTF adapter converts its opposite image-space convention at import.
  CHECK(near(u.U, 0) && near(u.V, 1.5),
        "Native algebraic rotation sends the first basis vector toward positive V");
  CHECK(near(v.U, -0.5) && near(v.V, 1),
        "Native algebraic rotation sends the second basis vector toward negative U");

  Scenario::Camera camera;
  camera.setProjection(Scenario::Camera::Perspective{.FovDeg = 90, .NearM = 1, .FarM = 10});
  Mat4 projection;
  CHECK(camera.projectionMatrix(2, projection),
        "a lens at the origin does not need a look-at target to produce its projection");
  CHECK(near(projection[0], 0.5) && near(projection[5], 1) && near(projection[11], -1),
        "glTF vertical perspective: aspect 2 gives x scale 1/2, y scale 1, w=-z");
  camera.FarM = std::numeric_limits<double>::infinity();
  CHECK(camera.projectionMatrix(2, projection) && near(projection[10], -1) &&
            near(projection[14], -2),
        "positive infinity produces the infinite perspective limit, never NaN");
  camera.FarM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!camera.projectionMatrix(2, projection), "NaN is not an infinite far plane");
  camera.FarM = 10;
  Mat4 view;
  CHECK(camera.viewMatrix(view), "the default camera looks down -Z with +Y up");
  CHECK(view == Mat4{}, "an origin camera with identity rotation has identity view");
  camera.Stands.Facing.Y = std::sqrt(0.5);
  camera.Stands.Facing.W = std::sqrt(0.5);
  camera.Stands.AtM = {{3, 4, 5}};
  CHECK(camera.viewMatrix(view),
        "a camera quaternion defines its orientation without a look-at declaration");
  const Vec3 ahead = view.TransformPoint({{2, 4, 5}});
  CHECK(near(ahead[0], 0) && near(ahead[1], 0) && near(ahead[2], -1),
        "glTF +90 degrees around Y rotates camera -Z to world -X");
  camera.Stands.Facing = {.Z = std::sqrt(0.5), .W = std::sqrt(0.5)};
  camera.Stands.AtM = {};
  CHECK(camera.viewMatrix(view), "camera roll is retained");
  const Vec3 up = view.TransformPoint({{-1, 0, 0}});
  CHECK(near(up[0], 0) && near(up[1], 1), "+90 degrees around Z rotates local +Y to world -X");
  camera.Stands.Facing = {.W = 0};
  Mat4 model;
  CHECK(!camera.modelMatrix(model), "a zero quaternion is not a camera orientation");
  camera.Stands.Facing = {};
  camera.LooksAt = true;
  camera.LookAtM = {{0, 0, -1}};
  camera.UpM = {{0, 1, 0}};
  CHECK(camera.viewMatrix(view) && view == Mat4{},
        "an explicit look-at overrides quaternion orientation");
  camera.UpM[1] = std::numeric_limits<double>::infinity();
  CHECK(!camera.modelMatrix(model), "infinite look-at up is not a camera basis");
  camera.UpM = {{0, 1, 0}};
  camera.LookAtM = {};
  CHECK(!camera.viewMatrix(view), "a look-at target at the eye refuses an undefined direction");
  CHECK(camera.projectionMatrix(2, projection),
        "invalid view orientation cannot invalidate the independent lens");
  camera.setProjection(Scenario::Camera::Ortho{.XMagM = 2, .YMagM = 3, .NearM = 1, .FarM = 11});
  CHECK(camera.projectionMatrix(0, projection),
        "orthographic half extents are independent of viewport aspect and pose");
  CHECK(near(projection[0], 0.5) && near(projection[5], 1.0 / 3),
        "glTF xmag/ymag are half extents");
  const auto depthAt = [](const Mat4f &p, float distance) {
    return (-p[10] * distance + p[14]) / (-p[11] * distance + p[15]);
  };
  Render::Lens gpu{
      .WidePx = 1280, .HighPx = 720, .OrthoWidthM = 4, .OrthoM = 6, .NearM = 1, .FarM = 11};
  Mat4f device = gpu.Projection();
  CHECK(std::abs(device[0] - 0.5f) < 1e-6f && std::abs(device[5] - 1.0f / 3) < 1e-6f,
        "GPU orthographic magnifications remain independent of the 16:9 canvas");
  CHECK(std::abs(depthAt(device, 1) - 1) < 1e-6f && std::abs(depthAt(device, 11)) < 1e-6f,
        "GPU orthographic depth maps the declared near/far to reverse-Z 1/0");
  gpu.OrthoM = gpu.OrthoWidthM = 0;
  gpu.FovDeg = 90;
  device = gpu.Projection();
  CHECK(std::abs(depthAt(device, 1) - 1) < 1e-6f && std::abs(depthAt(device, 11)) < 1e-6f,
        "GPU finite perspective honours both declared clipping planes");
  for (float distance : {1.0f, 3.0f, 11.0f}) {
    const float depth = depthAt(device, distance);
    const float reconstructed =
        (device[14] - depth * device[15]) / (device[10] - depth * device[11]);
    CHECK(std::abs(reconstructed - distance) < 1e-5f,
          "aerial perspective reconstructs physical distance from the finite projection");
  }
  gpu.FarM = 0;
  device = gpu.Projection();
  CHECK(std::abs(depthAt(device, 10) - 0.1f) < 1e-6f, "infinite reverse-Z retains near / distance");
  Covers("public camera projection and quaternion orientation, native algebraic UV rotation "
         "with a top-left image origin; GPU projection extents, finite/infinite "
         "reverse depth; no geodetic-pose coverage");
  return Report();
}
