#include "Shape.h"
#include <array>
#include <atomic>
#include <chrono>
#include "SubjectProxy.h"
#include "math/Vec3.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

#include "Heap.h"

#include <numbers>
#include <cmath>
#include <span>
#include <ratio>
#include <string>
#include <vector>

#include "SceneRenderer.h"

namespace outshine::Render {

void SubjectProxy::Stands(const Shape &subject, const Vec3 &anchorEcefM) {
  Shape_ = &subject;
  for (int axis = 0; axis < 3; ++axis) { AnchorEcefM_[axis] = anchorEcefM[axis]; }
  const size_t parts = subject.Parts.size();
  EmittedRadiance_.assign(parts, {0.0f, 0.0f, 0.0f});
  PartSurface_.assign(parts, 0);
  Instances_ = 1;
  PartPlacement_.assign(parts, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  Placed_ = false;
  Surfaces_.clear();
  Lights_.clear();
}

bool SubjectProxy::Carries(size_t instances) {
  if (instances == 0 || Shape_ == nullptr) { return false; }
  const size_t parts = Parts();
  if (instances == Instances_) { return true; }
  std::vector<std::array<double, 16>> moved(parts * instances,
                                            {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  const size_t kept = instances < Instances_ ? instances : Instances_;
  for (size_t part = 0; part < parts; ++part) {
    for (size_t one = 0; one < kept; ++one) {
      moved[part * instances + one] = PartPlacement_[part * Instances_ + one];
    }
  }
  PartPlacement_.swap(moved);
  Instances_ = instances;
  return true;
}

bool SubjectProxy::Wears(std::span<const uint32_t> partSlot,
                         std::span<const SubjectMaterial> slots,
                         std::string &error) {
  if (partSlot.size() != Parts()) {
    error = "the subject proxy stands over " + std::to_string(Parts()) +
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

bool SubjectProxy::Places(size_t part, const double placement[16]) {
  return Places(part, 0, placement);
}

bool SubjectProxy::Places(size_t part, size_t instance, const double placement[16]) {
  if (instance >= Instances_) { return false; }
  const size_t row = part * Instances_ + instance;
  if (row >= PartPlacement_.size()) { return false; }
  for (int at = 0; at < 16; ++at) { PartPlacement_[row][at] = placement[at]; }
  Placed_ = true;
  return true;
}

bool Placed(SceneRenderer &renderer, const SubjectProxy &proxy, std::string &error) {
  const size_t rows = proxy.Placements();
  if (!renderer.SubjectPlacementRows(rows, error)) { return false; }
  double ecef[16];
  for (size_t part = 0; part < rows; ++part) {
    for (int at = 0; at < 16; ++at) { ecef[at] = proxy.Placement(part)[at]; }
    renderer.MoveSubjectPlacement(part, ecef);
  }
  return renderer.HandSubjectPlacements(error);
}

bool MovedInstance(SceneRenderer &renderer,
                   size_t rows,
                   size_t instances,
                   size_t instance,
                   size_t fromPart,
                   size_t toPart,
                   const double ecef[16],
                   std::string &error) {
  if (instances == 0 || instance >= instances) { return true; }
  if (!renderer.SubjectPlacementRows(rows, error)) { return false; }
  for (size_t part = fromPart; part < toPart; ++part) {
    renderer.MoveSubjectPlacement(part * instances + instance, ecef);
  }
  return renderer.HandSubjectPlacements(error);
}

bool Moved(SceneRenderer &renderer,
           size_t rows,
           size_t from,
           size_t to,
           const double ecef[16],
           std::string &error) {
  if (!renderer.SubjectPlacementRows(rows, error)) { return false; }
  for (size_t part = from; part < to; ++part) { renderer.MoveSubjectPlacement(part, ecef); }
  return renderer.HandSubjectPlacements(error);
}

namespace {

void Anchored(const Vec3 &anchorEcefM, const Vec3 &gltf, Vec3 &out) {
  for (int axis = 0; axis < 3; ++axis) { out[axis] = gltf[axis]; }
  for (int axis = 0; axis < 3; ++axis) { out[axis] += anchorEcefM[axis]; }
}

[[nodiscard]] bool ClearsNearPlane(const Shape &subject,
                                   const Viewpoint &eye,
                                   size_t framedParts,
                                   bool standsInside,
                                   std::string &error) {
  const double plane = eye.ZNearM > 0.0 ? eye.ZNearM : static_cast<double>(SceneRenderer::kNearM);
  Vec3 framedLeast;
  Vec3 framedMost;
  subject.BoundsOf(framedParts, framedLeast, framedMost);
  size_t beyond = subject.VertexCount();
  if (framedParts > 0 && framedParts < subject.Parts.size()) {
    const ShapePart &last = subject.Parts[framedParts - 1];
    beyond = last.FirstVertex + last.VertexCount;
  }

  double least = 0.0;
  bool first = true;
  for (int corner = 0; corner < 8; ++corner) {
    double along = 0;
    for (int axis = 0; axis < 3; ++axis) {
      const double at = (static_cast<uint32_t>(corner) & (1u << static_cast<uint32_t>(axis))) != 0
                            ? framedMost[axis]
                            : framedLeast[axis];
      along += (at - eye.EyeM[axis]) * eye.Forward[axis];
    }
    if (first || along < least) {
      least = along;
      first = false;
    }
  }
  if (!first && least > plane) { return true; }
  for (const ShapePart &one : subject.Parts) {
    if (one.FirstVertex >= beyond) { break; }
    for (size_t within = 0; within < one.VertexCount && (within + 1) * 3 <= one.PositionsM.size();
         ++within) {
      const size_t vertex = one.FirstVertex + within;
      if (vertex >= beyond) { break; }
      double along = 0;
      for (int axis = 0; axis < 3; ++axis) {
        along += (static_cast<double>(one.PositionsM[within * 3 + static_cast<size_t>(axis)]) -
                  eye.EyeM[axis]) *
                 eye.Forward[axis];
      }
      if (along <= plane) {
        error = "vertex " + std::to_string(vertex) + " of the " + std::to_string(beyond) +
                " that " + std::to_string(framedParts) + " framed part(s) of " +
                std::to_string(subject.Parts.size()) + " carry sits " + std::to_string(along) +
                " m along the view axis, inside the near plane of " + std::to_string(plane) +
                " m this placement declares";
        return false;
      }
    }
  }
  return true;
}

constexpr double kMagnificationAgreement = 1e-12;

[[nodiscard]] bool
SetProjection(SceneRenderer &renderer, const Viewpoint &eye, std::string &error) {
  if (eye.Kind == CameraKind::Orthographic) {
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

double DepthFraction(const Shape &subject, const ShapePart &part, const Viewpoint &eye) {
  if (part.VertexCount == 0) { return 0.0; }
  if (part.PositionsM.size() < 3) { return 0.0; }
  Vec3 low;
  Vec3 high;
  for (int axis = 0; axis < 3; ++axis) {
    low[axis] = high[axis] = static_cast<double>(part.PositionsM[static_cast<size_t>(axis)]);
  }
  for (size_t vertex = 1; vertex < part.VertexCount && (vertex + 1) * 3 <= part.PositionsM.size();
       ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const auto value =
          static_cast<double>(part.PositionsM[vertex * 3 + static_cast<size_t>(axis)]);
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
  return !proxy.Lights().empty() || proxy.IndirectLight().RadianceLinear[0] > 0.0 ||
         proxy.IndirectLight().RadianceLinear[1] > 0.0 ||
         proxy.IndirectLight().RadianceLinear[2] > 0.0;
}

[[nodiscard]] bool Lit(const SubjectProxy &proxy, const Shape &subject, size_t part) {
  const SubjectMaterial &wearing = proxy.Slots()[proxy.Slot(part)];
  return subject.Parts[part].HasNormal && (Gathers(proxy) || wearing.Emissive.Rgba != nullptr) &&
         !wearing.Row.Unlit;
}

[[nodiscard]] bool Agrees(const SubjectProxy &proxy, const Shape &subject, std::string &error) {
  const size_t parts = subject.Parts.size();
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
    if (Gathers(proxy) && !subject.Parts[part].HasNormal &&
        !proxy.Slots()[proxy.Slot(part)].Row.Unlit) {
      error = "the proxy declares " + std::to_string(proxy.Lights().size()) +
              " punctual lights and an environment, and part " + std::to_string(part) +
              " of node '" + std::string(subject.Parts[part].Name) +
              "' carries no NORMAL, so there is no direction for the cosine -- and nothing here "
              "derives the flat normal the format asks for";
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool BuildDrawList(const SubjectProxy &proxy,
                                 const Eye &view,
                                 const Shape &subject,
                                 DrawList &list,
                                 std::string &error) {
  list.Clear();
  for (size_t part = 0; part < subject.Parts.size(); ++part) {
    const ShapePart &where = subject.Parts[part];
    const uint32_t slot = proxy.Slot(part);
    Render::DrawItem item;
    item.Order.Viewport = 0;
    item.Order.Layer = Render::ViewLayer::World;
    item.Order.Surface = proxy.Slots()[slot].State();
    item.Order.DepthFraction = DepthFraction(subject, where, view.Eye);
    item.Order.MaterialSlot = slot;
    item.ModelSlot = static_cast<uint32_t>(part * proxy.Instances());
    item.Instances = static_cast<uint32_t>(proxy.Instances());
    item.SourceFirstIndex = static_cast<uint32_t>(where.FirstIndex);
    item.IndexCount = static_cast<uint32_t>(where.IndexCount);
    item.FirstCluster = where.FirstCluster;
    item.ClusterCount = where.ClusterCount;

    VertexRunsCarried carried;
    carried.Uv = where.HasUv && (proxy.Slots()[slot].ReadsAnyImage() ||
                                 proxy.Slots()[slot].Domain == SurfaceDomain::Ground);
    carried.Normal = Lit(proxy, subject, part);

    carried.Tangent = carried.Normal && carried.Uv && where.HasTangent &&
                      (proxy.Slots()[slot].Normal.Rgba != nullptr);

    carried.Uv1 = carried.Uv && where.HasUv1 && proxy.Slots()[slot].ReadsSecondUv();

    carried.Colour = where.HasColour;
    if (!Render::LayoutOf(carried, item.Layout)) {
      error = "part " + std::to_string(part) + " of node '" + std::string(where.Name) +
              "' names a set of vertex runs that is not one of this engine's layouts";
      return false;
    }
    if (!list.Add(item, error)) { return false; }
  }
  list.Compile();
  list.JobsAddress(subject.Clusters);
  return true;
}

struct ChannelPack {
  const Shape *From = nullptr;
  std::span<const float> ShapePart::*Channel = nullptr;
  uint32_t Wide = 0;
};

void PackChannel(const void *carrying, float *into, uint32_t floats) {
  const ChannelPack &what = *static_cast<const ChannelPack *>(carrying);
  uint32_t at = 0;
  for (const ShapePart &one : what.From->Parts) {
    const std::span<const float> held = one.*what.Channel;
    const auto want = static_cast<uint32_t>(one.VertexCount * what.Wide);
    if (at + want > floats) { break; }
    if (held.size() >= one.VertexCount * what.Wide) {
      std::memcpy(into + at, held.data(), static_cast<size_t>(want) * sizeof(float));
    } else {
      std::memset(into + at, 0, static_cast<size_t>(want) * sizeof(float));
    }
    at += want;
  }
  if (at < floats) { std::memset(into + at, 0, static_cast<size_t>(floats - at) * sizeof(float)); }
}

struct EmitPack {
  const Shape *From = nullptr;
  const SubjectProxy *Proxy = nullptr;
};

void PackEmitted(const void *carrying, float *into, uint32_t floats) {
  const EmitPack &what = *static_cast<const EmitPack *>(carrying);
  uint32_t at = 0;
  for (size_t part = 0; part < what.From->Parts.size(); ++part) {
    const ShapePart &one = what.From->Parts[part];
    const std::array<float, 3> &radiance = what.Proxy->Emitted(part);
    for (size_t vertex = 0; vertex < one.VertexCount && at + 3u <= floats; ++vertex) {
      into[at + 0] = radiance[0];
      into[at + 1] = radiance[1];
      into[at + 2] = radiance[2];
      at += 3u;
    }
  }
  if (at < floats) { std::memset(into + at, 0, static_cast<size_t>(floats - at) * sizeof(float)); }
}

void PackPrevious(const void *carrying, float *into, uint32_t floats) {
  const std::vector<double> &held = *static_cast<const std::vector<double> *>(carrying);
  const uint32_t many = held.size() < floats ? static_cast<uint32_t>(held.size()) : floats;
  for (uint32_t at = 0; at < many; ++at) { into[at] = static_cast<float>(held[at]); }
  if (many < floats) {
    std::memset(into + many, 0, static_cast<size_t>(floats - many) * sizeof(float));
  }
}

[[nodiscard]] bool
PlaceLights(const SubjectProxy &proxy, std::vector<SubjectLight> &out, std::string &error) {
  out.clear();
  out.reserve(proxy.Lights().size());
  for (size_t at = 0; at < proxy.Lights().size(); ++at) {
    const outshine::PunctualLight &declared = proxy.Lights()[at];
    const Vec3 gltfDirection = {
        {declared.Direction[0], declared.Direction[1], declared.Direction[2]}};
    Vec3 direction;
    for (int axis = 0; axis < 3; ++axis) { direction[axis] = gltfDirection[axis]; }
    const double length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1] +
                                    direction[2] * direction[2]);
    if (!(length > 0)) {
      error = "light " + std::to_string(at) + " declares a beam of zero length";
      return false;
    }
    SubjectLight placed;
    placed.Light = declared;
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Direction[axis] = static_cast<float>(direction[axis] / length);
    }
    const Vec3 gltfPosition = {{declared.Position[0], declared.Position[1], declared.Position[2]}};
    Anchored(proxy.Anchor(), gltfPosition, placed.PositionEcefM);
    out.push_back(placed);
  }
  return true;
}

} // namespace

bool Aim(SceneRenderer &renderer,
         const Shape &subject,
         const Eye &view,
         const Vec3 &anchorEcefM,
         std::string &error) {
  const Viewpoint &eye = view.Eye;
  if (!SetProjection(renderer, eye, error)) { return false; }
  if (!view.StandsInside &&
      !ClearsNearPlane(subject, eye, view.FramedParts, view.StandsInside, error)) {
    return false;
  }
  Vec3 position;
  Anchored(anchorEcefM, eye.EyeM, position);
  renderer.SetCameraBasis(position, eye.Forward, eye.Right, eye.Up);
  return true;
}

bool Surface(SceneRenderer &renderer,
             const SubjectProxy &proxy,
             const Eye &view,
             SubjectScratch &scratch,
             std::string &error) {
  if (proxy.Shaped() == nullptr) {
    error = "the proxy declares no subject";
    return false;
  }
  if (!Agrees(proxy, *proxy.Shaped(), error)) { return false; }
  if (!renderer.SetSubjectMaterials(proxy.Slots(), error)) { return false; }
  if (!PlaceLights(proxy, scratch.Lights, error)) { return false; }
  if (!renderer.SetSubjectLights(scratch.Lights, error)) { return false; }
  renderer.SetSubjectEnvironment(proxy.IndirectLight());
  return true;
}

bool Show(SceneRenderer &renderer,
          const SubjectProxy &proxy,
          const Eye &view,
          SubjectScratch &scratch,
          std::string &error) {
  return Surface(renderer, proxy, view, scratch, error) &&
         Place(renderer, proxy, view, scratch, error);
}

namespace {

std::atomic<double> gPackMs{0.0};
std::atomic<unsigned long long> gGeometryDigest{0};
std::atomic<double> gHandMs{0.0};
std::atomic<double> gDigestMs{0.0};

} // namespace

double PackedMs() {
  return gPackMs.load(std::memory_order_relaxed);
}

double HandedMs() {
  return gHandMs.load(std::memory_order_relaxed);
}

double DigestedMs() {
  return gDigestMs.load(std::memory_order_relaxed);
}

double HandedGeometryDigest() {
  return static_cast<double>(gGeometryDigest.load(std::memory_order_relaxed) & 0xffffffffffffull);
}

bool Place(SceneRenderer &renderer,
           const SubjectProxy &proxy,
           const Eye &view,
           SubjectScratch &scratch,
           std::string &error) {
  if (proxy.Shaped() == nullptr) {
    error = "the proxy declares no subject";
    return false;
  }
  const Shape &subject = *proxy.Shaped();
  if (subject.TriangleCount() == 0) {
    error = "the subject carries no triangle, so there is nothing to stand in the proxy";
    return false;
  }
  if (!Agrees(proxy, subject, error)) { return false; }
  if ((proxy.Previous() != nullptr) && proxy.Previous()->size() / 3 != subject.VertexCount()) {
    error = "the proxy's previous pose carries " + std::to_string(proxy.Previous()->size() / 3) +
            " vertices and this one carries " + std::to_string(subject.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, view, proxy.Anchor(), error)) { return false; }

  {
    const Heap::Tagged inside("draw-list");
    if (!BuildDrawList(proxy, view, subject, scratch.Draws, error)) { return false; }
  }

  const Heap::Tagged packing("index-run");
  scratch.Indices.clear();
  scratch.Indices.reserve(scratch.Draws.IndexCount());
  for (const IndexRun &run : scratch.Draws.Runs()) {
    for (uint32_t at = 0; at < run.Count; ++at) {
      scratch.Indices.push_back(subject.Indices[run.SourceFirst + at]);
    }
  }
  gPackMs.store(0.0, std::memory_order_relaxed);

  SubjectMesh mesh;
  const ChannelPack positions{.From = &subject, .Channel = &ShapePart::PositionsM, .Wide = 3};
  const ChannelPack uv{.From = &subject, .Channel = &ShapePart::Uv, .Wide = 2};
  const ChannelPack uv1{.From = &subject, .Channel = &ShapePart::Uv1, .Wide = 2};
  const ChannelPack normals{.From = &subject, .Channel = &ShapePart::Normals, .Wide = 3};
  const ChannelPack tangents{.From = &subject, .Channel = &ShapePart::Tangents, .Wide = 4};
  const ChannelPack colours{.From = &subject, .Channel = &ShapePart::Colours, .Wide = 4};
  const EmitPack emitted{.From = &subject, .Proxy = &proxy};

  mesh.Verts = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &positions};
  if (subject.CarriesUv) {
    mesh.Uv = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &uv};
  }
  if (subject.CarriesUv1) {
    mesh.Uv1 = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &uv1};
  }
  if (subject.CarriesNormal) {
    mesh.Normals = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &normals};
  }
  if (subject.CarriesTangent) {
    mesh.Tangents = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &tangents};
  }
  if (subject.CarriesColour) {
    mesh.Colours = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &colours};
  }
  mesh.Emitted = SubjectStream{.From = nullptr, .Writes = PackEmitted, .Carrying = &emitted};
  scratch.Vertices.resize(subject.VertexCount() * 3u);
  PackChannel(&positions, scratch.Vertices.data(), static_cast<uint32_t>(scratch.Vertices.size()));
  mesh.Positions = scratch.Vertices;
  if (proxy.Previous() != nullptr) {
    mesh.PrevVerts =
        SubjectStream{.From = nullptr, .Writes = PackPrevious, .Carrying = proxy.Previous()};
  }
  mesh.VertexCount = static_cast<uint32_t>(subject.VertexCount());
  mesh.Indices = scratch.Indices.data();
  mesh.IndexCount = static_cast<uint32_t>(scratch.Indices.size());
  for (int axis = 0; axis < 3; ++axis) { mesh.Anchor[axis] = proxy.Anchor()[axis]; }
  if (scratch.Digests) {
    const auto digestedFrom = std::chrono::steady_clock::now();
    unsigned long long digest = 1469598103934665603ull;
    const auto eat = [&digest](const void *from, size_t bytes) {
      const auto *at = static_cast<const unsigned char *>(from);
      for (size_t one = 0; one < bytes; ++one) { digest = (digest ^ at[one]) * 1099511628211ull; }
    };
    eat(scratch.Indices.data(), scratch.Indices.size() * sizeof(uint32_t));
    for (const ShapePart &one : subject.Parts) {
      for (const std::span<const float> run :
           {one.PositionsM, one.Normals, one.Tangents, one.Uv, one.Uv1, one.Colours}) {
        eat(run.data(), run.size() * sizeof(float));
      }
    }
    gGeometryDigest.store(digest, std::memory_order_relaxed);
    gDigestMs.store(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - digestedFrom)
            .count(),
        std::memory_order_relaxed);
  } else {
    gGeometryDigest.store(0, std::memory_order_relaxed);
    gDigestMs.store(0.0, std::memory_order_relaxed);
  }
  mesh.Draws = &scratch.Draws;
  mesh.Clusters = subject.Clusters;
  mesh.ClusterSpheres = subject.ClusterSpheres;
  const Heap::Tagged handing("subject-mesh");
  const auto handedFrom = std::chrono::steady_clock::now();
  if (!renderer.SetSubjectMesh(mesh, error)) { return false; }
  gHandMs.store(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handedFrom)
          .count(),
      std::memory_order_relaxed);

  { std::vector<uint32_t>().swap(scratch.Indices); }
  { std::vector<float>().swap(scratch.Vertices); }
  if (!Placed(renderer, proxy, error)) { return false; }

  return true;
}

bool Move(SceneRenderer &renderer,
          const SubjectProxy &proxy,
          const Eye &view,
          SubjectScratch &scratch,
          std::string &error) {
  if (proxy.Shaped() == nullptr) {
    error = "the proxy declares no subject";
    return false;
  }
  const Shape &subject = *proxy.Shaped();
  if (!Agrees(proxy, subject, error)) { return false; }
  if ((proxy.Previous() != nullptr) && proxy.Previous()->size() / 3 != subject.VertexCount()) {
    error = "the proxy's previous pose carries " + std::to_string(proxy.Previous()->size() / 3) +
            " vertices and this one carries " + std::to_string(subject.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, subject, view, proxy.Anchor(), error)) { return false; }

  SubjectPose pose;
  const ChannelPack positions{.From = &subject, .Channel = &ShapePart::PositionsM, .Wide = 3};
  const ChannelPack uv{.From = &subject, .Channel = &ShapePart::Uv, .Wide = 2};
  const ChannelPack uv1{.From = &subject, .Channel = &ShapePart::Uv1, .Wide = 2};
  const ChannelPack normals{.From = &subject, .Channel = &ShapePart::Normals, .Wide = 3};
  const ChannelPack tangents{.From = &subject, .Channel = &ShapePart::Tangents, .Wide = 4};
  const ChannelPack colours{.From = &subject, .Channel = &ShapePart::Colours, .Wide = 4};
  const EmitPack emitted{.From = &subject, .Proxy = &proxy};

  pose.Verts = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &positions};
  if (subject.CarriesUv) {
    pose.Uv = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &uv};
  }
  if (subject.CarriesUv1) {
    pose.Uv1 = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &uv1};
  }
  if (subject.CarriesNormal) {
    pose.Normals = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &normals};
  }
  if (subject.CarriesTangent) {
    pose.Tangents = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &tangents};
  }
  if (subject.CarriesColour) {
    pose.Colours = SubjectStream{.From = nullptr, .Writes = PackChannel, .Carrying = &colours};
  }
  pose.Emitted = SubjectStream{.From = nullptr, .Writes = PackEmitted, .Carrying = &emitted};
  scratch.Vertices.resize(subject.VertexCount() * 3u);
  PackChannel(&positions, scratch.Vertices.data(), static_cast<uint32_t>(scratch.Vertices.size()));
  if (scratch.Digests) {
    const auto digestedFrom = std::chrono::steady_clock::now();
    unsigned long long digest = 1469598103934665603ull;
    const auto *at = reinterpret_cast<const unsigned char *>(scratch.Vertices.data());
    for (size_t one = 0; one < scratch.Vertices.size() * sizeof(float); ++one) {
      digest = (digest ^ at[one]) * 1099511628211ull;
    }
    gGeometryDigest.store(digest, std::memory_order_relaxed);
    gDigestMs.store(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - digestedFrom)
            .count(),
        std::memory_order_relaxed);
  } else {
    gGeometryDigest.store(0, std::memory_order_relaxed);
    gDigestMs.store(0.0, std::memory_order_relaxed);
  }
  pose.Positions = scratch.Vertices;
  if (proxy.Previous() != nullptr) {
    pose.PrevVerts =
        SubjectStream{.From = nullptr, .Writes = PackPrevious, .Carrying = proxy.Previous()};
  }
  pose.VertexCount = static_cast<uint32_t>(subject.VertexCount());
  for (int axis = 0; axis < 3; ++axis) { pose.Anchor[axis] = proxy.Anchor()[axis]; }
  const Heap::Tagged handing("subject-pose");
  if (!renderer.SetSubjectPose(pose, error)) { return false; }
  return true;
}

} // namespace outshine::Render
