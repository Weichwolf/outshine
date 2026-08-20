#include "GltfStudio.h"

#include "Heap.h"

#include <cmath>
#include <string>

#include "Renderer.h"

namespace outshine::Clients {

void EcefFromGltf(const double gltf[3], double out[3]) {
  out[0] = gltf[1];
  out[1] = gltf[0];
  out[2] = -gltf[2];
}

namespace {

void Anchored(const double gltf[3], double out[3]) {
  EcefFromGltf(gltf, out);
  for (int axis = 0; axis < 3; ++axis) { out[axis] += kStudioAnchorEcefM[axis]; }
}

[[nodiscard]] bool ClearsNearPlane(const Gltf::Subject &subject, const Gltf::Placement &eye,
                                   std::string &error) {
  const double plane = eye.ZNearM > 0.0 ? eye.ZNearM : (double)Render::Renderer::kNearM;

  double least = 0.0;
  bool first = true;
  for (int corner = 0; corner < 8; ++corner) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      const double at = (corner & (1 << axis)) != 0 ? subject.MaxM()[axis] : subject.MinM()[axis];
      along += (at - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (first || along < least) {
      least = along;
      first = false;
    }
  }
  if (!first && least > plane) { return true; }
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      along += (subject.PositionsM()[vertex * 3 + (size_t)axis] - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (along <= plane) {
      error = "vertex " + std::to_string(vertex) + " sits " + std::to_string(along) +
              " m along the view axis, inside the near plane of " + std::to_string(plane) +
              " m this placement declares";
      return false;
    }
  }
  return true;
}

constexpr double kMagnificationAgreement = 1e-12;

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

  renderer.SetNearM(eye.ZNearM);
  return true;
}

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

[[nodiscard]] bool Gathers(const Studio &studio) {
  return !studio.Lights.empty() || studio.Environment.RadianceLinear[0] > 0.0 ||
         studio.Environment.RadianceLinear[1] > 0.0 || studio.Environment.RadianceLinear[2] > 0.0;
}

[[nodiscard]] bool Lit(const Studio &studio, const Gltf::Subject &subject, size_t part) {
  return subject.Parts()[part].HasNormal && Gathers(studio) &&
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

    if (Gathers(studio) && !subject.Parts()[part].HasNormal &&
        !studio.Surfaces[studio.PartSurface[part]].Row.Unlit) {
      error = "the studio declares " + std::to_string(studio.Lights.size()) +
              " punctual lights and an environment, and part " + std::to_string(part) + " of node '" +
              subject.Parts()[part].NodeName +
              "' carries no NORMAL, so there is no direction for the cosine -- and nothing here "
              "derives the flat normal the format asks for";
      return false;
    }
  }
  return true;
}

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

    Render::VertexRunsCarried carried;
    carried.Uv = where.HasUv && studio.Surfaces[slot].ReadsAnyImage();
    carried.Normal = Lit(studio, subject, part);

    carried.Tangent = carried.Normal && carried.Uv && where.HasTangent() &&
                      studio.Surfaces[slot].Normal.Rgba;

    carried.Uv1 = carried.Uv && where.HasUv1 && studio.Surfaces[slot].ReadsSecondUv();

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

struct VertexRuns {
  size_t UvAt = 0;
  size_t Uv1At = 0;
  size_t NormalAt = 0;
  size_t TangentAt = 0;
  size_t ColourAt = 0;
  size_t EmittedAt = 0;
  size_t PreviousAt = 0;
};

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

  runs.NormalAt = vertices.size();
  for (size_t vertex = 0; vertex * 3 < subject.Normals().size(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.Normals()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
  }

  runs.TangentAt = vertices.size();
  for (size_t vertex = 0; vertex * 4 < subject.Tangents().size(); ++vertex) {
    double ecef[3];
    EcefFromGltf(&subject.Tangents()[vertex * 4], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
    vertices.push_back((float)subject.Tangents()[vertex * 4 + 3]);
  }

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

}

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

bool Surface(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
             std::string &error) {
  if (!studio.Geometry) {
    error = "the studio declares no subject";
    return false;
  }
  if (!Declared(studio, *studio.Geometry, error)) { return false; }
  if (!renderer.SetSubjectMaterials(studio.Surfaces, error)) { return false; }
  if (!PlaceLights(studio, scratch.Lights, error)) { return false; }
  if (!renderer.SetSubjectLights(scratch.Lights, error)) { return false; }
  renderer.SetSubjectEnvironment(studio.Environment);
  return true;
}

bool Show(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
          std::string &error) {
  return Surface(renderer, studio, scratch, error) && Place(renderer, studio, scratch, error);
}

bool Place(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
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

  {
    const Heap::Tagged inside("draw-list");
    if (!BuildDrawList(studio, subject, scratch.Draws, error)) { return false; }
  }

  const Heap::Tagged packing("index-run");
  scratch.Indices.clear();
  scratch.Indices.reserve(scratch.Draws.IndexCount());
  for (const Render::IndexRun &run : scratch.Draws.Runs()) {
    for (uint32_t at = 0; at < run.Count; ++at) {
      scratch.Indices.push_back(subject.Indices()[run.SourceFirst + at]);
    }
  }
  const Heap::Tagged vertices("vertex-pack");
  const VertexRuns runs = PackVertices(studio, subject, scratch.Vertices);

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

    mesh.PrevAnchor[axis] = kStudioAnchorEcefM[axis];
  }
  mesh.Draws = &scratch.Draws;
  const Heap::Tagged handing("subject-mesh");
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }

  return true;
}

bool Move(Render::Renderer &renderer, const Studio &studio, StudioScratch &scratch,
          std::string &error) {
  if (!studio.Geometry) {
    error = "the studio declares no subject";
    return false;
  }
  const Gltf::Subject &subject = *studio.Geometry;
  if (!Declared(studio, subject, error)) { return false; }
  if (studio.Previous && studio.Previous->VertexCount() != subject.VertexCount()) {
    error = "the studio's previous pose carries " +
            std::to_string(studio.Previous->VertexCount()) + " vertices and this one carries " +
            std::to_string(subject.VertexCount()) + ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, studio.Eye, error)) { return false; }

  const Heap::Tagged packing("vertex-pack");
  const VertexRuns runs = PackVertices(studio, subject, scratch.Vertices);
  Render::SubjectPose pose;
  pose.Verts = scratch.Vertices.data();
  pose.Uv = subject.HasUv() ? scratch.Vertices.data() + runs.UvAt : nullptr;
  pose.Uv1 = subject.HasUv1() ? scratch.Vertices.data() + runs.Uv1At : nullptr;
  pose.Normals = subject.HasNormal() ? scratch.Vertices.data() + runs.NormalAt : nullptr;
  pose.Tangents = subject.HasTangent() ? scratch.Vertices.data() + runs.TangentAt : nullptr;
  pose.Colours = subject.HasColour() ? scratch.Vertices.data() + runs.ColourAt : nullptr;
  pose.Emitted = scratch.Vertices.data() + runs.EmittedAt;
  pose.PrevVerts = studio.Previous ? scratch.Vertices.data() + runs.PreviousAt : nullptr;
  pose.VertexCount = (uint32_t)subject.VertexCount();
  for (int axis = 0; axis < 3; ++axis) {
    pose.Anchor[axis] = kStudioAnchorEcefM[axis];
    pose.PrevAnchor[axis] = kStudioAnchorEcefM[axis];
  }
  const Heap::Tagged handing("subject-pose");
  return renderer.SetSubjectPose(pose, error);
}

}
