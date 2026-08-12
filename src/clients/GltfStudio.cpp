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

/* WHERE A PART SITS IN THE DEPTH FIELD OF ITS DRAW KEY: 0 at the declaration's own near plane, 1 at
 * its far plane, measured along the view axis to the CENTRE OF THE PART'S BOX. The box centre and
 * not a vertex mean, because min and max are exact and commutative in IEEE-754 while a mean moves
 * when the loader's order does -- and a draw order that moved with the loader's order would be a
 * pace-dependent picture. */
double DepthFraction(const Gltf::Subject &subject, const Gltf::Part &part,
                     const Gltf::Placement &eye) {
  if (part.VertexCount == 0) { return 0.0; }
  double low[3], high[3];
  for (int axis = 0; axis < 3; ++axis) {
    low[axis] = high[axis] = subject.PositionsM()[part.FirstVertex * 3 + (size_t)axis];
  }
  for (size_t vertex = 1; vertex < part.VertexCount; ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const double value = subject.PositionsM()[(part.FirstVertex + vertex) * 3 + (size_t)axis];
      if (value < low[axis]) { low[axis] = value; }
      if (value > high[axis]) { high[axis] = value; }
    }
  }
  double along = 0;
  for (int axis = 0; axis < 3; ++axis) {
    along += (0.5 * (low[axis] + high[axis]) - eye.EyeM[axis]) * eye.Forward[axis];
  }
  const double span = eye.ZFarM - eye.ZNearM;
  if (!(span > 0)) { return 0.0; }
  return (along - eye.ZNearM) / span;
}

[[nodiscard]] bool Declared(const Studio &studio, const Gltf::Subject &subject,
                            std::string &error) {
  const size_t parts = subject.Parts().size();
  if (studio.EmittedRadiance.size() != parts) {
    error = "the studio declares " + std::to_string(studio.EmittedRadiance.size()) +
            " emitted radiances over a subject of " + std::to_string(parts) +
            " drawn primitives, and every part emits what it was declared to emit";
    return false;
  }
  if (studio.PartSurface.size() != parts) {
    error = "the studio declares " + std::to_string(studio.PartSurface.size()) +
            " surface slots over a subject of " + std::to_string(parts) + " drawn primitives";
    return false;
  }
  if (studio.Surfaces.empty()) {
    error = "the studio declares no surface at all, and every draw binds one";
    return false;
  }
  for (size_t part = 0; part < parts; ++part) {
    if (studio.PartSurface[part] >= studio.Surfaces.size()) {
      error = "part " + std::to_string(part) + " names surface slot " +
              std::to_string(studio.PartSurface[part]) + " over a table of " +
              std::to_string(studio.Surfaces.size());
      return false;
    }
  }
  return true;
}

/* ONE DRAW ITEM PER DRAWN PRIMITIVE, then compiled: the sort puts correctness first and the layout
 * puts two draws of one surface next to each other in the index run, which is what lets them become
 * one call. */
[[nodiscard]] bool BuildDrawList(const Studio &studio, const Gltf::Subject &subject,
                                 Render::DrawList &list, std::string &error) {
  list.Clear();
  for (size_t part = 0; part < subject.Parts().size(); ++part) {
    const Gltf::Part &where = subject.Parts()[part];
    const uint32_t slot = studio.PartSurface[part];
    Render::DrawItem item;
    item.Order.Viewport = 0;
    item.Order.Layer = Render::ViewLayer::World;
    item.Order.Surface = studio.Surfaces[slot].Surface;
    item.Order.DepthFraction = DepthFraction(subject, where, studio.Eye);
    item.Order.MaterialSlot = slot;
    item.SourceFirstIndex = (uint32_t)where.FirstIndex;
    item.IndexCount = (uint32_t)where.IndexCount;
    /* THE LAYOUT IS A PROPERTY OF THE DRAW AND NEEDS BOTH HALVES: a part that carries uvs but wears
     * a surface with no image would otherwise take the textured pipeline and sample the one white
     * texel that only exists to make the bind group complete -- a stand-in wearing a texture's
     * name. */
    item.Layout = where.HasUv && studio.Surfaces[slot].BaseColour.Rgba
                      ? Render::VertexLayout::PositionUv
                      : Render::VertexLayout::Position;
    if (!list.Add(item, error)) { return false; }
  }
  list.Compile();
  return true;
}

/* WHERE THE THREE VERTEX RUNS START inside the one buffer the caller reuses. */
struct VertexRuns {
  size_t UvAt = 0;
  size_t EmittedAt = 0;
};

/* ONE BUFFER, THREE RUNS: the positions, then the uvs, then the per-vertex radiance, so a caller
 * reusing capacity across a loop of cases pays one allocation and the three vertex buffers are three
 * pointers into it. */
VertexRuns PackVertices(const Studio &studio, const Gltf::Subject &subject,
                        std::vector<float> &vertices) {
  vertices.clear();
  vertices.reserve(subject.PositionsM().size() + subject.Uv().size() + subject.VertexCount() * 3);
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.PositionsM()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
  }
  VertexRuns runs;
  runs.UvAt = vertices.size();
  for (const double coordinate : subject.Uv()) { vertices.push_back((float)coordinate); }
  runs.EmittedAt = vertices.size();
  vertices.resize(runs.EmittedAt + subject.VertexCount() * 3, 0.0f);
  for (size_t part = 0; part < subject.Parts().size(); ++part) {
    const Gltf::Part &where = subject.Parts()[part];
    for (size_t vertex = 0; vertex < where.VertexCount; ++vertex) {
      for (size_t channel = 0; channel < 3; ++channel) {
        vertices[runs.EmittedAt + (where.FirstVertex + vertex) * 3 + channel] =
            studio.EmittedRadiance[part][channel];
      }
    }
  }
  return runs;
}

} // namespace

bool Show(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
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
  if (!Declared(studio, subject, error)) { return false; }
  if (!SetProjection(renderer, eye, error)) { return false; }
  if (!ClearsNearPlane(subject, eye, error)) { return false; }

  if (!BuildDrawList(studio, subject, scratch.Draws, error)) { return false; }

  scratch.Indices.clear();
  scratch.Indices.reserve(scratch.Draws.IndexCount());
  for (const Render::IndexRun &run : scratch.Draws.Runs()) {
    for (uint32_t at = 0; at < run.Count; ++at) {
      scratch.Indices.push_back(subject.Indices()[run.SourceFirst + at]);
    }
  }
  const VertexRuns runs = PackVertices(studio, subject, scratch.Vertices);

  if (!renderer.SetSubjectMaterials(studio.Surfaces, error)) { return false; }
  Render::SubjectMesh mesh;
  mesh.Verts = scratch.Vertices.data();
  mesh.Uv = subject.HasUv() ? scratch.Vertices.data() + runs.UvAt : nullptr;
  mesh.Emitted = scratch.Vertices.data() + runs.EmittedAt;
  mesh.VertexCount = (uint32_t)subject.VertexCount();
  mesh.Indices = scratch.Indices.data();
  mesh.IndexCount = (uint32_t)scratch.Indices.size();
  for (int axis = 0; axis < 3; ++axis) { mesh.Anchor[axis] = kStudioAnchorEcefM[axis]; }
  mesh.Draws = &scratch.Draws;
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }

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
