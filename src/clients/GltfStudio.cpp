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

/* WHETHER ONE PART IS SHADED AT ALL, in the one place that can answer it: a cosine needs a normal,
 * a light list to measure against, and a surface that reads light. `KHR_materials_unlit` says the
 * base colour IS the output -- "no lighting, no shadow ray, no BRDF" -- so an unlit part takes the
 * emitted arm however many lights the scene declares, and the radiance it emits is the surface's own
 * base colour, which is what the caller declares per part. Two spellings of this predicate, one
 * deciding the refusal below and one deciding the pipeline, is the disagreement that would draw an
 * unlit caption black. */
[[nodiscard]] bool Lit(const Studio &studio, const Gltf::Subject &subject, size_t part) {
  return subject.Parts()[part].HasNormal && !studio.Lights.empty() &&
         !studio.Surfaces[studio.PartSurface[part]].Row.Unlit;
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
    /* A LIT SCENE OVER A PART WITH NO NORMAL IS A REFUSAL AND NOT A DARKER DRAW. Falling back to the
     * emitted arm would draw that part in a radiance nothing declared -- black, in a scene where
     * every other body is lit -- which reads as a shading bug rather than as the missing attribute
     * it is. glTF says a client MUST compute flat normals for such a primitive; until something
     * does, this is what says so. */
    if (!studio.Lights.empty() && !subject.Parts()[part].HasNormal &&
        !studio.Surfaces[studio.PartSurface[part]].Row.Unlit) {
      error = "the studio declares " + std::to_string(studio.Lights.size()) +
              " punctual lights and part " + std::to_string(part) + " of node '" +
              subject.Parts()[part].NodeName +
              "' carries no NORMAL, so there is no direction for the cosine -- and nothing here "
              "derives the flat normal the format asks for";
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
    item.Order.Surface = studio.Surfaces[slot].State();
    item.Order.DepthFraction = DepthFraction(subject, where, studio.Eye);
    item.Order.MaterialSlot = slot;
    item.SourceFirstIndex = (uint32_t)where.FirstIndex;
    item.IndexCount = (uint32_t)where.IndexCount;
    /* THE LAYOUT IS A PROPERTY OF THE DRAW AND NEEDS BOTH HALVES: a part that carries uvs but wears
     * a surface with no image would otherwise take the textured pipeline and sample the one white
     * texel that only exists to make the bind group complete -- a stand-in wearing a texture's
     * name. THE SAME HOLDS OF THE NORMAL, and its second half is the SCENE rather than the surface:
     * a subject nothing lights takes the emitted arm however many normals its file carries, because
     * there is no direction for a cosine to be measured against and the declaration's own radiance
     * is the whole answer. */
    Render::VertexRunsCarried carried;
    carried.Uv = where.HasUv && studio.Surfaces[slot].Colour.Rgba;
    carried.Normal = Lit(studio, subject, part);
    /* THE NORMAL-MAPPED LAYOUT NEEDS BOTH HALVES TOO, and the second half is the SURFACE: a part
     * that carries a tangent basis under a surface with no normal map would sample the one white
     * texel that only exists to complete the bind group, and white decodes to the tangent-space
     * direction (1, 1, 1), which is a tilt no file asked for. */
    carried.Tangent = carried.Normal && carried.Uv && where.HasTangent() &&
                      studio.Surfaces[slot].Normal.Rgba;
    /* THE SECOND UV SET NEEDS BOTH HALVES TOO, and they are the part's attribute and the SURFACE's
     * declaration (board:1182): a part that carries `TEXCOORD_1` under a surface no socket of which
     * reads it would bind a run nothing samples, and a surface that reads it over a part carrying
     * none is refused in `SetSubjectMesh` rather than drawn from the first set. */
    carried.Uv1 = carried.Uv && where.HasUv1 && studio.Surfaces[slot].ReadsSecondUv();
    /* THE VERTEX COLOUR HAS NO SECOND HALF AND THE ASYMMETRY IS THE POINT (board:1193): the three
     * above are runs that only exist to address a texture, so a surface declaring none leaves them
     * addressing a stand-in. `COLOR_0` multiplies BASE COLOUR, and every surface has one -- there is
     * no socket for it to be missing, so the part's own attribute is the whole condition. */
    carried.Colour = where.HasColour;
    if (!Render::LayoutOf(carried, item.Layout)) {
      error = "part " + std::to_string(part) + " of node '" + where.NodeName +
              "' names a set of vertex runs that is not one of this engine's layouts";
      return false;
    }
    if (!list.Add(item, error)) { return false; }
  }
  list.Compile();
  return true;
}

/* WHERE EACH VERTEX RUN STARTS inside the one buffer the caller reuses. */
struct VertexRuns {
  size_t UvAt = 0;
  size_t Uv1At = 0;
  size_t NormalAt = 0;
  size_t TangentAt = 0;
  size_t ColourAt = 0;
  size_t EmittedAt = 0;
  size_t PreviousAt = 0;
};

/* ONE BUFFER, ONE RUN PER ATTRIBUTE: the positions, the two uv sets, the normals, the tangents and
 * the per-vertex radiance, so a caller reusing capacity across a loop of cases pays one allocation
 * and the vertex buffers are pointers into it. */
VertexRuns PackVertices(const Studio &studio, const Gltf::Subject &subject,
                        std::vector<float> &vertices) {
  vertices.clear();
  vertices.reserve(subject.PositionsM().size() + subject.Uv().size() + subject.Uv1().size() +
                   subject.Normals().size() + subject.Tangents().size() + subject.Colours().size() +
                   subject.VertexCount() * 3);
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.PositionsM()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
  }
  VertexRuns runs;
  runs.UvAt = vertices.size();
  for (const double coordinate : subject.Uv()) { vertices.push_back((float)coordinate); }
  runs.Uv1At = vertices.size();
  for (const double coordinate : subject.Uv1()) { vertices.push_back((float)coordinate); }
  /* THE NORMAL TAKES THE SAME PERMUTATION AS THE POSITION AND NOT AN INVERSE TRANSPOSE OF IT. The
   * map from glTF's frame to the engine's is a signed permutation whose determinant is +1, so it is
   * its own inverse transpose and a normal stays a normal and stays unit under it. Anything else
   * here would be re-deriving `EcefFromGltf` in a second form. */
  runs.NormalAt = vertices.size();
  for (size_t vertex = 0; vertex * 3 < subject.Normals().size(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.Normals()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
  }
  /* THE TANGENT TAKES THE SAME PERMUTATION AS THE POSITION, for the same reason the normal does --
   * it is a direction in the surface, and the map between the two frames is a signed permutation of
   * determinant +1. `w` IS NOT PERMUTED AND MUST NOT BE: it is a handedness relative to the normal
   * and the tangent, and a map that preserves orientation preserves it. */
  runs.TangentAt = vertices.size();
  for (size_t vertex = 0; vertex * 4 < subject.Tangents().size(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.Tangents()[vertex * 4], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
    vertices.push_back((float)subject.Tangents()[vertex * 4 + 3]);
  }
  /* THE VERTEX COLOUR CROSSES UNPERMUTED AND UNDECODED (board:1193). It is not a direction, so the
   * frame map has nothing to do to it, and it is already the LINEAR multiplier glTF says it is -- a
   * transfer function applied on this side would be the plausible wrong picture the case exists to
   * catch. Narrowing to f32 is the same narrowing every other run takes. */
  runs.ColourAt = vertices.size();
  for (const double component : subject.Colours()) { vertices.push_back((float)component); }
  runs.PreviousAt = vertices.size();
  if (studio.Previous) {
    for (size_t vertex = 0; vertex < studio.Previous->VertexCount(); ++vertex) {
      double ecef[3];
      EcefFromGltf(&studio.Previous->PositionsM()[vertex * 3], ecef);
      for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
    }
  }
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

/* THE DECLARED LIGHTS IN THE ENGINE'S FRAME. The position becomes an ECEF double, because a float
 * metre at the Earth's radius is a half-metre quantum and a light half a metre out of place is a
 * shading error nobody would attribute to a cast; the direction and the two cone angles cross
 * unchanged in shape, the direction only permuted.
 *
 * REFUSES A LIGHT WHOSE BEAM IS NOT A DIRECTION. `Gltf::Subject` normalises what it places, so a
 * zero here is a caller that built the list itself -- and a zero beam would make every facet face
 * away from a directional light and the whole subject black. */
[[nodiscard]] bool PlaceLights(const Studio &studio, std::vector<Render::SubjectLight> &out,
                               std::string &error) {
  out.clear();
  out.reserve(studio.Lights.size());
  for (size_t at = 0; at < studio.Lights.size(); ++at) {
    const outshine::PunctualLight &declared = studio.Lights[at];
    const double gltfDirection[3] = {declared.Direction[0], declared.Direction[1],
                                     declared.Direction[2]};
    double direction[3];
    EcefFromGltf(gltfDirection, direction);
    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                    direction[2] * direction[2]);
    if (!(length > 0)) {
      error = "light " + std::to_string(at) + " declares a beam of zero length";
      return false;
    }
    Render::SubjectLight placed;
    placed.Light = declared;
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Direction[axis] = (float)(direction[axis] / length);
    }
    const double gltfPosition[3] = {declared.Position[0], declared.Position[1],
                                    declared.Position[2]};
    Anchored(gltfPosition, placed.PositionEcefM);
    out.push_back(placed);
  }
  return true;
}

} // namespace

bool Aim(Render::Renderer &renderer, const Gltf::Subject &subject, const Gltf::Placement &eye,
         std::string &error) {
  if (!SetProjection(renderer, eye, error)) { return false; }
  if (!ClearsNearPlane(subject, eye, error)) { return false; }
  double position[3], forward[3], right[3], up[3];
  Anchored(eye.EyeM, position);
  EcefFromGltf(eye.Forward, forward);
  EcefFromGltf(eye.Right, right);
  EcefFromGltf(eye.Up, up);
  renderer.SetCameraBasis(position, forward, right, up);
  return true;
}

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
  if (studio.Previous && studio.Previous->VertexCount() != subject.VertexCount()) {
    error = "the studio's previous pose carries " +
            std::to_string(studio.Previous->VertexCount()) + " vertices and this one carries " +
            std::to_string(subject.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, eye, error)) { return false; }

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
  if (!PlaceLights(studio, scratch.Lights, error)) { return false; }
  if (!renderer.SetSubjectLights(scratch.Lights, error)) { return false; }
  renderer.SetSubjectEnvironment(studio.Environment);
  Render::SubjectMesh mesh;
  mesh.Verts = scratch.Vertices.data();
  mesh.Uv = subject.HasUv() ? scratch.Vertices.data() + runs.UvAt : nullptr;
  mesh.Uv1 = subject.HasUv1() ? scratch.Vertices.data() + runs.Uv1At : nullptr;
  mesh.Normals = subject.HasNormal() ? scratch.Vertices.data() + runs.NormalAt : nullptr;
  mesh.Tangents = subject.HasTangent() ? scratch.Vertices.data() + runs.TangentAt : nullptr;
  mesh.Colours = subject.HasColour() ? scratch.Vertices.data() + runs.ColourAt : nullptr;
  mesh.Emitted = scratch.Vertices.data() + runs.EmittedAt;
  mesh.PrevVerts = studio.Previous ? scratch.Vertices.data() + runs.PreviousAt : nullptr;
  mesh.VertexCount = (uint32_t)subject.VertexCount();
  mesh.Indices = scratch.Indices.data();
  mesh.IndexCount = (uint32_t)scratch.Indices.size();
  for (int axis = 0; axis < 3; ++axis) {
    mesh.Anchor[axis] = kStudioAnchorEcefM[axis];
    /* The subject stands at one place on the globe whatever it is doing, so the previous frame's
     * anchor is this one: what moved is the vertices and the eye, and both of those are carried. */
    mesh.PrevAnchor[axis] = kStudioAnchorEcefM[axis];
  }
  mesh.Draws = &scratch.Draws;
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }

  return true;
}

} // namespace outshine::Clients
