#include "GltfStudio.h"

#include <cmath>
#include <string>

#include "Renderer.h"

namespace outshine::Clients {

void EcefFromGltf(const double gltf[3], double out[3]) {
  out[0] = gltf[1];  /* up */
  out[1] = gltf[0];  /* east */
  out[2] = -gltf[2]; /* north */
}

namespace {

void Anchored(const double gltf[3], double out[3]) {
  EcefFromGltf(gltf, out);
  for (int axis = 0; axis < 3; ++axis) { out[axis] += kStudioAnchorEcefM[axis]; }
}

/* THE ENGINE'S NEAR PLANE IS FIXED and the declaration's own clip range does not reach it. That
 * costs nothing where the picture is decided by coverage or by depth -- x and y do not depend on it
 * and the far plane is infinite -- but a subject nearer than it would be silently cropped, so it is
 * a refusal that names both numbers. */
[[nodiscard]] bool ClearsNearPlane(const Gltf::Subject &subject, const Gltf::Placement &eye,
                                   std::string &error) {
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      along += (subject.PositionsM()[vertex * 3 + (size_t)axis] - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (along <= (double)Render::Renderer::kNearM) {
      error = "vertex " + std::to_string(vertex) + " sits " + std::to_string(along) +
              " m along the view axis, inside the engine's fixed near plane of " +
              std::to_string((double)Render::Renderer::kNearM) + " m";
      return false;
    }
  }
  return true;
}

/* THE ENGINE'S PARALLEL PROJECTION IS ONE NUMBER -- the vertical extent it covers -- and glTF
 * declares two magnifications. Where the two disagree the engine would silently render the vertical
 * one and drop the horizontal, so the mismatch is a refusal naming both. 1e-12 relative is float
 * noise on a round-tripped decimal; the smallest mistake this catches is a sensor fit, which is a
 * factor of the aspect ratio away. */
constexpr double kMagnificationAgreement = 1e-12; /* [SET] */

[[nodiscard]] bool SetProjection(Render::Renderer &renderer, const Gltf::Placement &eye,
                                 std::string &error) {
  if (eye.Kind == Gltf::CameraKind::Orthographic) {
    if (!(eye.YMagM > 0) || !(eye.XMagM > 0)) {
      error = "the placement is orthographic and declares no magnification";
      return false;
    }
    const double wanted = eye.YMagM * renderer.SceneAspect();
    if (std::fabs(eye.XMagM - wanted) > kMagnificationAgreement * wanted) {
      error = "the placement declares xmag " + std::to_string(eye.XMagM) + " where ymag " +
              std::to_string(eye.YMagM) + " at the frame's aspect " +
              std::to_string(renderer.SceneAspect()) + " gives " + std::to_string(wanted) +
              ", and the engine's parallel projection carries only the vertical extent";
      return false;
    }
    renderer.SetOrthoM(2.0 * eye.YMagM);
    return true;
  }
  if (!(eye.YfovRad > 0)) {
    error = "the placement declares no field of view";
    return false;
  }
  renderer.SetFovDeg(eye.YfovRad * 180.0 / 3.14159265358979323846);
  return true;
}

} // namespace

bool Show(Render::Renderer &renderer, const Studio &studio, std::vector<float> &scratch,
          std::string &error) {
  if (!studio.Geometry) {
    error = "the studio declares no subject";
    return false;
  }
  const Gltf::Subject &subject = *studio.Geometry;
  const Gltf::Placement &eye = studio.Eye;
  if (subject.TriangleCount() == 0) {
    error = "the subject carries no triangle, so there is nothing to stand in the studio";
    return false;
  }
  if (!SetProjection(renderer, eye, error)) { return false; }
  if (!ClearsNearPlane(subject, eye, error)) { return false; }

  /* ONE BUFFER, TWO RUNS: the positions first and the uvs after them, so a caller reusing capacity
   * across a loop of cases pays one allocation and the two vertex buffers are two pointers into it.
   * The uv run is written even when it is empty, so `HasUv` decides the pipeline and not a length. */
  scratch.clear();
  scratch.reserve(subject.PositionsM().size() + subject.Uv().size());
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.PositionsM()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { scratch.push_back((float)ecef[axis]); }
  }
  const size_t uvAt = scratch.size();
  for (const double coordinate : subject.Uv()) { scratch.push_back((float)coordinate); }
  renderer.SetSubjectMesh(scratch.data(), subject.HasUv() ? scratch.data() + uvAt : nullptr,
                          (uint32_t)subject.VertexCount(), subject.Indices().data(),
                          (uint32_t)subject.Indices().size(), kStudioAnchorEcefM);
  renderer.SetSubjectSurface(studio.Surface);
  renderer.SetSubjectTexture(studio.BaseColour);

  double position[3], forward[3], right[3], up[3];
  Anchored(eye.EyeM, position);
  EcefFromGltf(eye.Forward, forward);
  EcefFromGltf(eye.Right, right);
  EcefFromGltf(eye.Up, up);
  renderer.SetCameraBasis(position, forward, right, up);
  /* NO TEMPORAL ACCUMULATION IN A STUDIO. A jittered sample grid asks the coverage question
   * somewhere other than the pixel centre, which is the exact quantity a parity rung measures. */
  renderer.PinJitter(0.0f, 0.0f);
  return true;
}

} // namespace outshine::Clients
