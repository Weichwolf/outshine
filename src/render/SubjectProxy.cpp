#include "SubjectProxy.h"

#include <limits>

#include "Heap.h"

#include <numbers>
#include <cmath>
#include <string>

#include "Renderer.h"

namespace outshine::Render {

void SubjectProxy::Stands(const Gltf::Subject &subject, const double anchorEcefM[3]) {
  Subject_ = &subject;
  for (int axis = 0; axis < 3; ++axis) { AnchorEcefM_[axis] = anchorEcefM[axis]; }
  const size_t parts = subject.Parts().size();
  EmittedRadiance_.assign(parts, {0.0f, 0.0f, 0.0f});
  PartSurface_.assign(parts, 0);
  PartPlacement_.assign(parts, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  Placed_ = false;
  Surfaces_.clear();
  Lights_.clear();
}

void SubjectProxy::Sees(const Gltf::Placement &eye, bool standsInside, size_t framedParts) {
  Eye_ = eye;
  StandsInside_ = standsInside;
  FramedParts_ = framedParts;
}

bool SubjectProxy::Wears(std::span<const uint32_t> partSlot,
                         std::span<const SubjectMaterial> slots, std::string &error) {
  if (partSlot.size() != PartPlacement_.size()) {
    error = "the subject proxy stands over " + std::to_string(PartPlacement_.size()) +
            " parts and the surface table names a slot for " + std::to_string(partSlot.size());
    return false;
  }
  PartSurface_.assign(partSlot.begin(), partSlot.end());
  Surfaces_.assign(slots.begin(), slots.end());
  return true;
}

bool SubjectProxy::Emits(size_t part, const std::array<float, 3> &radiance) {
  if (part >= EmittedRadiance_.size()) { return false; }
  EmittedRadiance_[part] = radiance;
  return true;
}

bool SubjectProxy::Places(size_t part, const double m16[16]) {
  if (part >= PartPlacement_.size()) { return false; }
  for (int at = 0; at < 16; ++at) { PartPlacement_[part][at] = m16[at]; }
  Placed_ = true;
  return true;
}


bool Placed(Renderer &renderer, const SubjectProxy &proxy, std::string &error) {
  const size_t rows = proxy.Placements();
  if (!renderer.SubjectPlacementRows(rows, error)) { return false; }
  double ecef[16];
  for (size_t part = 0; part < rows; ++part) {
    Gltf::PlacedInEcef(proxy.Placement(part).data(), ecef);
    renderer.MoveSubjectPlacement(part, ecef);
  }
  return true;
}

bool Moved(Renderer &renderer, size_t rows, size_t from, size_t to, const double ecef[16],
           std::string &error) {
  if (!renderer.SubjectPlacementRows(rows, error)) { return false; }
  for (size_t part = from; part < to; ++part) { renderer.MoveSubjectPlacement(part, ecef); }
  return true;
}

namespace {

void Anchored(const double anchorEcefM[3], const double gltf[3], double out[3]) {
  Gltf::InEcef(gltf, out);
  for (int axis = 0; axis < 3; ++axis) { out[axis] += anchorEcefM[axis]; }
}

[[nodiscard]] bool ClearsNearPlane(const Gltf::Subject &subject, const Gltf::Placement &eye,
                                   size_t framedParts, bool standsInside, std::string &error) {
  const double plane = eye.ZNearM > 0.0 ? eye.ZNearM : (double)Renderer::kNearM;
  double framedLeast[3], framedMost[3];
  subject.BoundsOf(framedParts, framedLeast, framedMost);
  size_t beyond = subject.VertexCount();
  if (framedParts > 0 && framedParts < subject.Parts().size()) {
    const Gltf::Part &last = subject.Parts()[framedParts - 1];
    beyond = last.FirstVertex + last.VertexCount;
  }

  double least = 0.0;
  bool first = true;
  for (int corner = 0; corner < 8; ++corner) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      const double at = (corner & (1 << axis)) != 0 ? framedMost[axis] : framedLeast[axis];
      along += (at - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (first || along < least) {
      least = along;
      first = false;
    }
  }
  if (!first && least > plane) { return true; }
  for (size_t vertex = 0; vertex < beyond; ++vertex) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      along += (subject.PositionsM()[vertex * 3 + (size_t)axis] - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (along <= plane) {
      error = "vertex " + std::to_string(vertex) + " of the " + std::to_string(beyond) +
              " that " + std::to_string(framedParts) + " framed part(s) of " +
              std::to_string(subject.Parts().size()) + " carry sits " + std::to_string(along) +
              " m along the view axis, inside the near plane of " + std::to_string(plane) +
              " m this placement declares";
      return false;
    }
  }
  return true;
}

constexpr double kMagnificationAgreement = 1e-12;

[[nodiscard]] bool SetProjection(Renderer &renderer, const Gltf::Placement &eye,
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
  renderer.SetFovDeg(eye.YfovRad * 180.0 / std::numbers::pi);

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

[[nodiscard]] bool Gathers(const SubjectProxy &proxy) {
  return !proxy.Lights().empty() || proxy.Environment().RadianceLinear[0] > 0.0 ||
         proxy.Environment().RadianceLinear[1] > 0.0 || proxy.Environment().RadianceLinear[2] > 0.0;
}

[[nodiscard]] bool Lit(const SubjectProxy &proxy, const Gltf::Subject &subject, size_t part) {
  return subject.Parts()[part].HasNormal && Gathers(proxy) &&
         !proxy.Slots()[proxy.Slot(part)].Row.Unlit;
}

[[nodiscard]] bool Agrees(const SubjectProxy &proxy, const Gltf::Subject &subject,
                          std::string &error) {
  const size_t parts = subject.Parts().size();
  if (proxy.Slots().empty()) {
    error = "the proxy declares no surface at all, and every draw binds one";
    return false;
  }
  for (size_t part = 0; part < parts; ++part) {
    if (proxy.Slot(part) >= proxy.Slots().size()) {
      error = "part " + std::to_string(part) + " names surface slot " +
              std::to_string(proxy.Slot(part)) + " over a table of " +
              std::to_string(proxy.Slots().size());
      return false;
    }
    if (Gathers(proxy) && !subject.Parts()[part].HasNormal &&
        !proxy.Slots()[proxy.Slot(part)].Row.Unlit) {
      error = "the proxy declares " + std::to_string(proxy.Lights().size()) +
              " punctual lights and an environment, and part " + std::to_string(part) + " of node '" +
              subject.Parts()[part].NodeName +
              "' carries no NORMAL, so there is no direction for the cosine -- and nothing here "
              "derives the flat normal the format asks for";
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool BuildDrawList(const SubjectProxy &proxy, const Gltf::Subject &subject,
                                 DrawList &list, std::string &error) {
  list.Clear();
  for (size_t part = 0; part < subject.Parts().size(); ++part) {
    const Gltf::Part &where = subject.Parts()[part];
    const uint32_t slot = proxy.Slot(part);
    Render::DrawItem item;
    item.Order.Viewport = 0;
    item.Order.Layer = Render::ViewLayer::World;
    item.Order.Surface = proxy.Slots()[slot].State();
    item.Order.DepthFraction = DepthFraction(subject, where, proxy.Eye());
    item.Order.MaterialSlot = slot;
    item.ModelSlot = (uint32_t)part;
    item.SourceFirstIndex = (uint32_t)where.FirstIndex;
    item.IndexCount = (uint32_t)where.IndexCount;

    VertexRunsCarried carried;
    carried.Uv = where.HasUv && proxy.Slots()[slot].ReadsAnyImage();
    carried.Normal = Lit(proxy, subject, part);

    carried.Tangent = carried.Normal && carried.Uv && where.HasTangent() &&
                      proxy.Slots()[slot].Normal.Rgba;

    carried.Uv1 = carried.Uv && where.HasUv1 && proxy.Slots()[slot].ReadsSecondUv();

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

VertexRuns PackVertices(const SubjectProxy &proxy, const Gltf::Subject &subject,
                        std::vector<float> &vertices) {
  vertices.clear();
  vertices.reserve(subject.PositionsM().size() + subject.Uv().size() + subject.Uv1().size() +
                   subject.Normals().size() + subject.Tangents().size() + subject.Colours().size() +
                   subject.VertexCount() * 3);
  for (size_t vertex = 0; vertex < subject.VertexCount(); ++vertex) {
    double ecef[3];
    Gltf::InEcef(&subject.PositionsM()[vertex * 3], ecef);
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
    Gltf::InEcef(&subject.Normals()[vertex * 3], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
  }

  runs.TangentAt = vertices.size();
  for (size_t vertex = 0; vertex * 4 < subject.Tangents().size(); ++vertex) {
    double ecef[3];
    Gltf::InEcef(&subject.Tangents()[vertex * 4], ecef);
    for (int axis = 0; axis < 3; ++axis) { vertices.push_back((float)ecef[axis]); }
    vertices.push_back((float)subject.Tangents()[vertex * 4 + 3]);
  }

  runs.ColourAt = vertices.size();
  for (const double component : subject.Colours()) { vertices.push_back((float)component); }
  runs.PreviousAt = vertices.size();
  if (proxy.Previous()) {
    for (size_t vertex = 0; vertex < proxy.Previous()->size() / 3; ++vertex) {
      double ecef[3];
      Gltf::InEcef(&(*proxy.Previous())[vertex * 3], ecef);
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
            proxy.Emitted(part)[channel];
      }
    }
  }
  return runs;
}

[[nodiscard]] bool PlaceLights(const SubjectProxy &proxy, std::vector<SubjectLight> &out,
                               std::string &error) {
  out.clear();
  out.reserve(proxy.Lights().size());
  for (size_t at = 0; at < proxy.Lights().size(); ++at) {
    const outshine::PunctualLight &declared = proxy.Lights()[at];
    const double gltfDirection[3] = {declared.Direction[0], declared.Direction[1],
                                     declared.Direction[2]};
    double direction[3];
    Gltf::InEcef(gltfDirection, direction);
    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                    direction[2] * direction[2]);
    if (!(length > 0)) {
      error = "light " + std::to_string(at) + " declares a beam of zero length";
      return false;
    }
    SubjectLight placed;
    placed.Light = declared;
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Direction[axis] = (float)(direction[axis] / length);
    }
    const double gltfPosition[3] = {declared.Position[0], declared.Position[1],
                                    declared.Position[2]};
    Anchored(proxy.Anchor(), gltfPosition, placed.PositionEcefM);
    out.push_back(placed);
  }
  return true;
}

}

bool Aim(Renderer &renderer, const Gltf::Subject &subject, const Gltf::Placement &eye,
         const double anchorEcefM[3], std::string &error, bool standsInside, size_t framedParts) {
  if (!SetProjection(renderer, eye, error)) { return false; }
  if (!standsInside && !ClearsNearPlane(subject, eye, framedParts, standsInside, error)) {
    return false;
  }
  double position[3], forward[3], right[3], up[3];
  Anchored(anchorEcefM, eye.EyeM, position);
  Gltf::InEcef(eye.Forward, forward);
  Gltf::InEcef(eye.Right, right);
  Gltf::InEcef(eye.Up, up);
  renderer.SetCameraBasis(position, forward, right, up);
  return true;
}

bool Surface(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
             std::string &error) {
  if (!proxy.Subject()) {
    error = "the proxy declares no subject";
    return false;
  }
  if (!Agrees(proxy, *proxy.Subject(), error)) { return false; }
  if (!renderer.SetSubjectMaterials(proxy.Slots(), error)) { return false; }
  if (!PlaceLights(proxy, scratch.Lights, error)) { return false; }
  if (!renderer.SetSubjectLights(scratch.Lights, error)) { return false; }
  renderer.SetSubjectEnvironment(proxy.Environment());
  return true;
}

bool Show(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
          std::string &error) {
  return Surface(renderer, proxy, scratch, error) && Place(renderer, proxy, scratch, error);
}

bool Place(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
          std::string &error) {
  if (!proxy.Subject()) {
    error = "the proxy declares no subject";
    return false;
  }
  const Gltf::Subject &subject = *proxy.Subject();
  const Gltf::Placement &eye = proxy.Eye();
  if (subject.TriangleCount() == 0) {
    error = "the subject carries no triangle, so there is nothing to stand in the proxy";
    return false;
  }
  if (!Agrees(proxy, subject, error)) { return false; }
  if (proxy.Previous() && proxy.Previous()->size() / 3 != subject.VertexCount()) {
    error = "the proxy's previous pose carries " +
            std::to_string(proxy.Previous()->size() / 3) + " vertices and this one carries " +
            std::to_string(subject.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, eye, proxy.Anchor(), error, proxy.StandsInside(), proxy.FramedParts())) {
    return false;
  }

  {
    const Heap::Tagged inside("draw-list");
    if (!BuildDrawList(proxy, subject, scratch.Draws, error)) { return false; }
  }

  const Heap::Tagged packing("index-run");
  scratch.Indices.clear();
  scratch.Indices.reserve(scratch.Draws.IndexCount());
  for (const IndexRun &run : scratch.Draws.Runs()) {
    for (uint32_t at = 0; at < run.Count; ++at) {
      scratch.Indices.push_back(subject.Indices()[run.SourceFirst + at]);
    }
  }
  const Heap::Tagged vertices("vertex-pack");
  const VertexRuns runs = PackVertices(proxy, subject, scratch.Vertices);

  SubjectMesh mesh;
  mesh.Verts = scratch.Vertices.data();
  mesh.Uv = subject.HasUv() ? scratch.Vertices.data() + runs.UvAt : nullptr;
  mesh.Uv1 = subject.HasUv1() ? scratch.Vertices.data() + runs.Uv1At : nullptr;
  mesh.Normals = subject.HasNormal() ? scratch.Vertices.data() + runs.NormalAt : nullptr;
  mesh.Tangents = subject.HasTangent() ? scratch.Vertices.data() + runs.TangentAt : nullptr;
  mesh.Colours = subject.HasColour() ? scratch.Vertices.data() + runs.ColourAt : nullptr;
  mesh.Emitted = scratch.Vertices.data() + runs.EmittedAt;
  mesh.PrevVerts = proxy.Previous() ? scratch.Vertices.data() + runs.PreviousAt : nullptr;
  mesh.VertexCount = (uint32_t)subject.VertexCount();
  mesh.Indices = scratch.Indices.data();
  mesh.IndexCount = (uint32_t)scratch.Indices.size();
  for (int axis = 0; axis < 3; ++axis) {
    mesh.Anchor[axis] = proxy.Anchor()[axis];

    mesh.PrevAnchor[axis] = proxy.Anchor()[axis];
  }
  mesh.Draws = &scratch.Draws;
  const Heap::Tagged handing("subject-mesh");
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }
  if (!Placed(renderer, proxy, error)) { return false; }

  return true;
}

bool Move(Renderer &renderer, const SubjectProxy &proxy, SubjectScratch &scratch,
          std::string &error) {
  if (!proxy.Subject()) {
    error = "the proxy declares no subject";
    return false;
  }
  const Gltf::Subject &subject = *proxy.Subject();
  if (!Agrees(proxy, subject, error)) { return false; }
  if (proxy.Previous() && proxy.Previous()->size() / 3 != subject.VertexCount()) {
    error = "the proxy's previous pose carries " +
            std::to_string(proxy.Previous()->size() / 3) + " vertices and this one carries " +
            std::to_string(subject.VertexCount()) + ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, proxy.Eye(), proxy.Anchor(), error, proxy.StandsInside(),
           proxy.FramedParts())) {
    return false;
  }

  const Heap::Tagged packing("vertex-pack");
  const VertexRuns runs = PackVertices(proxy, subject, scratch.Vertices);
  SubjectPose pose;
  pose.Verts = scratch.Vertices.data();
  pose.Uv = subject.HasUv() ? scratch.Vertices.data() + runs.UvAt : nullptr;
  pose.Uv1 = subject.HasUv1() ? scratch.Vertices.data() + runs.Uv1At : nullptr;
  pose.Normals = subject.HasNormal() ? scratch.Vertices.data() + runs.NormalAt : nullptr;
  pose.Tangents = subject.HasTangent() ? scratch.Vertices.data() + runs.TangentAt : nullptr;
  pose.Colours = subject.HasColour() ? scratch.Vertices.data() + runs.ColourAt : nullptr;
  pose.Emitted = scratch.Vertices.data() + runs.EmittedAt;
  pose.PrevVerts = proxy.Previous() ? scratch.Vertices.data() + runs.PreviousAt : nullptr;
  pose.VertexCount = (uint32_t)subject.VertexCount();
  for (int axis = 0; axis < 3; ++axis) {
    pose.Anchor[axis] = proxy.Anchor()[axis];
    pose.PrevAnchor[axis] = proxy.Anchor()[axis];
  }
  const Heap::Tagged handing("subject-pose");
  if (!renderer.SetSubjectPose(pose, error)) { return false; }
  return true;
}

}
