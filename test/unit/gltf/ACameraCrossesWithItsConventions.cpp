#include <cmath>
#include <string>
#include <vector>

#include "Check.h"
#include "Glb.h"

#include "Document.h"

using outshine::Gltf::Camera;
using outshine::Gltf::CameraKind;
using outshine::Gltf::Document;
using outshine::Gltf::Transform;
using outshine::Gltf::Viewport;
using outshine::Test::Glb;

namespace {

constexpr double kYfovRad = 0.47108996144172666;
constexpr double kNearM = 0.1;
constexpr double kFarM = 100.0;

const char *const kJson = R"({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [ { "nodes": [0, 1] } ],
  "cameras": [
    { "type": "perspective", "perspective": { "yfov": 0.47108996144172666, "znear": 0.1, "zfar": 100.0 } },
    { "type": "perspective", "perspective": { "yfov": 0.47108996144172666, "znear": 0.1 } },
    { "type": "orthographic", "orthographic": { "xmag": 4.0, "ymag": 2.0, "znear": 0.1, "zfar": 100.0 } }
  ],
  "nodes": [
    { "camera": 0, "matrix": [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 7, 8, 9, 1] },
    { "camera": 2, "translation": [0, 0, 5] }
  ]
})";

double NdcZ(const Transform &projection, double viewZ) {
  const double point[3] = {0.0, 0.0, viewZ};
  double ndc[3] = {0, 0, 0};
  projection.Point(point, ndc);
  return ndc[2];
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<uint8_t> glb = Glb(kJson, {});
  Document document;
  const bool read = document.Read({glb.data(), glb.size()}, "cameras.glb");
  CHECK(read, "a GLB carrying three cameras is read");
  if (!read) {
    std::printf("       %s\n", document.Error().c_str());
    return Report();
  }
  CHECK(document.Cameras().size() == 3, "three cameras cross");
  CHECK(document.Cameras()[0].Kind == CameraKind::Perspective &&
            document.Cameras()[2].Kind == CameraKind::Orthographic,
        "each camera keeps the kind its file states");
  CHECK_NEAR(document.Cameras()[0].YfovRad, kYfovRad, 1e-15, "rad",
             "the vertical field of view crosses unchanged");
  CHECK(document.Cameras()[0].AspectRatio == 0.0,
        "an absent aspectRatio is absent and not a substituted one");
  CHECK(document.Cameras()[1].ZFarM == 0.0,
        "an absent zfar is absent and not a large number standing in for infinity");

  const Viewport viewport{1280.0, 720.0};
  Transform projection;
  CHECK(document.Cameras()[0].Projection(viewport.Aspect(), projection),
        "a finite perspective frustum yields a projection");

  CHECK_NEAR(NdcZ(projection, -kNearM), -1.0, 1e-12, "ndc",
             "the near plane is at NDC z = -1, which is the format's clip convention");
  CHECK_NEAR(NdcZ(projection, -kFarM), 1.0, 1e-12, "ndc",
             "the far plane is at NDC z = +1");

  const double halfHeightAtNear = kNearM * std::tan(0.5 * kYfovRad);
  const double top[3] = {0.0, halfHeightAtNear, -kNearM};
  double ndc[3] = {0, 0, 0};
  projection.Point(top, ndc);
  CHECK_NEAR(ndc[1], 1.0, 1e-12, "ndc", "the frustum's top edge is at NDC y = +1");
  double pixel[2] = {0, 0};
  viewport.Raster(ndc, pixel);
  CHECK_NEAR(pixel[1], -0.5, 1e-9, "px",
             "NDC y = +1 is the TOP of the raster: the integer coordinate is the pixel centre, so "
             "the edge above row 0 is -0.5");

  const double right[3] = {viewport.Aspect() * halfHeightAtNear, 0.0, -kNearM};
  projection.Point(right, ndc);
  CHECK_NEAR(ndc[0], 1.0, 1e-12, "ndc",
             "the frustum's right edge is at NDC x = +1, with the aspect taken from the viewport");
  viewport.Raster(ndc, pixel);
  CHECK_NEAR(pixel[0], 1279.5, 1e-9, "px", "NDC x = +1 is the right edge of the last column");

  Transform infinite;
  CHECK(document.Cameras()[1].Projection(viewport.Aspect(), infinite),
        "a frustum with no zfar yields a projection of its own");
  const double farAway = NdcZ(infinite, -1.0e9);
  CHECK(farAway < 1.0 && farAway > 1.0 - 1e-6,
        "an infinite frustum approaches NDC z = +1 without reaching it");
  Note("how far NDC z falls short of +1 at 1e9 m in an infinite frustum", 1.0 - farAway, "ndc");

  Transform orthographic;
  CHECK(document.Cameras()[2].Projection(viewport.Aspect(), orthographic),
        "an orthographic camera yields a projection");
  const double corner[3] = {4.0, 2.0, -kNearM};
  orthographic.Point(corner, ndc);
  CHECK_NEAR(ndc[0], 1.0, 1e-12, "ndc", "xmag is the half width, so x = xmag is NDC x = +1");
  CHECK_NEAR(ndc[1], 1.0, 1e-12, "ndc", "ymag is the half height");
  CHECK_NEAR(ndc[2], -1.0, 1e-12, "ndc", "the orthographic near plane is at NDC z = -1 too");

  Transform view;
  CHECK(document.ViewTransform(0, view), "a camera node has a view transform");
  const double eye[3] = {7.0, 8.0, 9.0};
  double inView[3] = {0, 0, 0};
  view.Point(eye, inView);
  CHECK_NEAR(inView[0], 0.0, 1e-12, "m", "the camera sits at the origin of its own view space");
  CHECK_NEAR(inView[1], 0.0, 1e-12, "m", "the camera sits at the origin of its own view space");
  CHECK_NEAR(inView[2], 0.0, 1e-12, "m", "the camera sits at the origin of its own view space");
  const double ahead[3] = {7.0, 8.0, 8.0};
  view.Point(ahead, inView);
  CHECK_NEAR(inView[2], -1.0, 1e-12, "m",
             "a point one metre in front of the camera is at view z = -1: glTF cameras look down -Z");

  Covers("I.51 cameras: perspective and orthographic, with the glTF sign and handedness conventions");
  return Report();
}
