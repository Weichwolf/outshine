#include "SceneLighting.h"
#include "MeshInput.h"
#include "Digest.h"
#include "Units.h"
#include "Mat4.h"
#include "RenderGeometryView.h"
#include <algorithm>
#include <array>
#include <chrono>
#include "RenderObject.h"
#include "Vec3.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <limits>

#include "Heap.h"

#include <cmath>
#include <span>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

#include "SceneRenderer.h"
#include "Lens.h"

namespace outshine::Render {

namespace Says {
constexpr auto kInvalidLens =
    "the camera projection is invalid or cannot be represented by the GPU lens";
}

void RenderObject::ResetGeometry(const RenderGeometryView &geometry, const Vec3 &anchorEcefM) {
  Geometry_ = &geometry;
  for (int axis = 0; axis < 3; ++axis) { AnchorEcefM_[axis] = anchorEcefM[axis]; }
  const size_t parts = geometry.Parts.size();
  EmittedRadiance_.assign(parts, {0.0f, 0.0f, 0.0f});
  PartSurface_.assign(parts, 0);
  PartVisible_.assign(parts, 1);
  Instances_ = 1;
  PartPlacement_.assign(parts, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
  Placed_ = false;
  Surfaces_.clear();
  Lights_.clear();
}

void RenderObject::SelectParts(std::span<const std::string> names) {
  for (size_t part = 0; part < PartVisible_.size(); ++part) {
    PartVisible_[part] = static_cast<uint8_t>(
        names.empty() || std::ranges::find(names, Geometry_->Parts[part].Name) != names.end());
  }
}

bool RenderObject::ResizeInstances(size_t instances) {
  if (instances == 0 || Geometry_ == nullptr) { return false; }
  const size_t parts = Parts();
  if (instances == Instances_) { return true; }
  std::vector<Mat4> moved(parts * instances, {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
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

bool RenderObject::SetMaterials(std::span<const uint32_t> partSlot,
                                std::span<const MaterialBinding> slots,
                                std::string &error) {
  if (partSlot.size() != Parts()) {
    error = "the geometry proxy stands over " + std::to_string(Parts()) +
            " parts and the surface table names a slot for " + std::to_string(partSlot.size());
    return false;
  }
  PartSurface_.assign(partSlot.begin(), partSlot.end());
  Surfaces_.assign(slots.begin(), slots.end());
  return true;
}

bool RenderObject::SetEmission(size_t part, const std::array<float, 3> &radiance) {
  if (part >= EmittedRadiance_.size()) { return false; }
  EmittedRadiance_[part] = radiance;
  return true;
}

bool RenderObject::SetPlacement(size_t part, const Mat4 &placement) {
  return SetPlacement(part, 0, placement);
}

bool RenderObject::SetPlacement(size_t part, size_t instance, const Mat4 &placement) {
  if (instance >= Instances_) { return false; }
  const size_t row = part * Instances_ + instance;
  if (row >= PartPlacement_.size()) { return false; }
  for (int at = 0; at < 16; ++at) { PartPlacement_[row][at] = placement[at]; }
  Placed_ = true;
  return true;
}

bool Placed(SceneRenderer &renderer, const RenderObject &proxy, std::string &error) {
  const size_t rows = proxy.Placements();
  if (!renderer.GeometryPlacementRows(rows, error)) { return false; }
  Mat4 ecef{};
  for (size_t part = 0; part < rows; ++part) {
    for (int at = 0; at < 16; ++at) { ecef[at] = proxy.Placement(part)[at]; }
    renderer.MoveGeometryPlacement(part, ecef);
  }
  return renderer.HandGeometryPlacements(error);
}

bool Moved(SceneRenderer &renderer, Moving what, const Mat4 &ecef, std::string &error) {
  if (what.Many == 0 || what.Which >= what.Many) { return true; }
  if (!renderer.GeometryPlacementRows(what.Rows, error)) { return false; }
  for (size_t part = what.FromPart; part < what.ToPart; ++part) {
    renderer.MoveGeometryPlacement(part * what.Many + what.Which, ecef);
  }
  return true;
}

namespace {

[[nodiscard]] bool ClearsNearPlane(const RenderGeometryView &geometry,
                                   const Viewpoint &eye,
                                   size_t framedParts,
                                   std::string &error) {
  const double plane = eye.ZNearM > 0.0 ? eye.ZNearM : static_cast<double>(SceneRenderer::kNearM);
  const Box framed = geometry.BoundsOf(framedParts);
  size_t beyond = geometry.VertexCount();
  if (framedParts > 0 && framedParts < geometry.Parts.size()) {
    const RenderGeometryPart &last = geometry.Parts[framedParts - 1];
    beyond = last.FirstVertex + last.VertexCount;
  }

  double least = Dot(framed.Corner(0) - eye.EyeM, eye.Forward);
  for (unsigned corner = 1; corner < 8; ++corner) {
    least = std::min(least, Dot(framed.Corner(corner) - eye.EyeM, eye.Forward));
  }
  if (least > plane) { return true; }
  for (const RenderGeometryPart &one : geometry.Parts) {
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
                std::to_string(geometry.Parts.size()) + " carry sits " + std::to_string(along) +
                " m along the view axis, inside the near plane of " + std::to_string(plane) +
                " m this placement declares";
        return false;
      }
    }
  }
  return true;
}

double DepthFraction([[maybe_unused]] const RenderGeometryView &geometry,
                     const RenderGeometryPart &part,
                     const Viewpoint &eye) {
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
      low[axis] = std::min(value, low[axis]);
      high[axis] = std::max(value, high[axis]);
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

[[nodiscard]] bool Gathers(const RenderObject &proxy) {
  return !proxy.Lights().empty() || proxy.IndirectLight().SkyLux > 0.0 ||
         proxy.IndirectLight().RadianceLinear[0] > 0.0 ||
         proxy.IndirectLight().RadianceLinear[1] > 0.0 ||
         proxy.IndirectLight().RadianceLinear[2] > 0.0;
}

[[nodiscard]] bool Lit(const RenderObject &proxy, const RenderGeometryView &geometry, size_t part) {
  const MaterialBinding &wearing = proxy.Slots()[proxy.Slot(part)];
  return geometry.Parts[part].HasNormal && (Gathers(proxy) || wearing.Emissive.Rgba != nullptr) &&
         !wearing.Row.Unlit;
}

[[nodiscard]] bool
Agrees(const RenderObject &proxy, const RenderGeometryView &geometry, std::string &error) {
  const size_t parts = geometry.Parts.size();
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
    if (Gathers(proxy) && !geometry.Parts[part].HasNormal &&
        !proxy.Slots()[proxy.Slot(part)].Row.Unlit) {
      error = "the proxy declares " + std::to_string(proxy.Lights().size()) +
              " punctual lights and an environment, and part " + std::to_string(part) +
              " of node '" + std::string(geometry.Parts[part].Name) +
              "' carries no NORMAL, so there is no direction for the cosine -- and nothing here "
              "derives the flat normal the format asks for";
      return false;
    }
  }
  return true;
}

[[nodiscard]] bool BuildDrawList(const RenderObject &proxy,
                                 const PreparedView &view,
                                 const RenderGeometryView &geometry,
                                 DrawList &list,
                                 std::string &error) {
  list.Clear();
  for (size_t part = 0; part < geometry.Parts.size(); ++part) {
    if (!proxy.PartVisible(part)) { continue; }
    const RenderGeometryPart &where = geometry.Parts[part];
    const uint32_t slot = proxy.Slot(part);
    Render::DrawItem item;
    item.Order.Viewport = 0;
    item.Order.Layer = Render::ViewLayer::World;
    item.Order.Surface = proxy.Slots()[slot].State();
    item.Order.DepthFraction = DepthFraction(geometry, where, view.Eye);
    item.Order.MaterialSlot = slot;
    item.ModelSlot = static_cast<uint32_t>(part * proxy.Instances());
    item.Instances = static_cast<uint32_t>(proxy.Instances());
    item.SourceFirstIndex = static_cast<uint32_t>(where.FirstIndex);
    item.IndexCount = static_cast<uint32_t>(where.IndexCount);
    item.FirstCluster = where.FirstCluster;
    item.ClusterCount = where.ClusterCount;

    VertexRunsCarried carried;
    carried.Uv = where.HasUv && (proxy.Slots()[slot].ReadsAnyImage() ||
                                 proxy.Slots()[slot].Row.Pattern != SurfacePattern::None ||
                                 proxy.Slots()[slot].Domain == SurfaceDomain::Ground);
    carried.Normal = Lit(proxy, geometry, part);

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
  list.JobsAddress(geometry.Clusters);
  return true;
}

struct ChannelPack {
  const RenderGeometryView *From = nullptr;
  std::span<const float> RenderGeometryPart::*Channel = nullptr;
  uint32_t Wide = 0;
};

void PackChannel(const void *carrying, float *into, uint32_t floats) {
  const ChannelPack &what = *static_cast<const ChannelPack *>(carrying);
  uint32_t at = 0;
  for (const RenderGeometryPart &one : what.From->Parts) {
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
  const RenderGeometryView *From = nullptr;
  const RenderObject *Proxy = nullptr;
};

void PackEmitted(const void *carrying, float *into, uint32_t floats) {
  const EmitPack &what = *static_cast<const EmitPack *>(carrying);
  uint32_t at = 0;
  for (size_t part = 0; part < what.From->Parts.size(); ++part) {
    const RenderGeometryPart &one = what.From->Parts[part];
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
  const std::span<const float> held = *static_cast<const std::span<const float> *>(carrying);
  const uint32_t many = held.size() < floats ? static_cast<uint32_t>(held.size()) : floats;
  if (many > 0) { std::memcpy(into, held.data(), static_cast<size_t>(many) * sizeof(float)); }
  if (many < floats) {
    std::memset(into + many, 0, static_cast<size_t>(floats - many) * sizeof(float));
  }
}

[[nodiscard]] bool
PlaceLights(const RenderObject &proxy, std::vector<SceneLight> &out, std::string &error) {
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
    SceneLight placed;
    placed.Light = declared;
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Direction[axis] = static_cast<float>(direction[axis] / length);
    }
    const Vec3 gltfPosition = {{declared.Position[0], declared.Position[1], declared.Position[2]}};
    placed.PositionEcefM = gltfPosition + proxy.Anchor();
    out.push_back(placed);
  }
  return true;
}

}

bool Aim(SceneRenderer &renderer,
         const RenderGeometryView &geometry,
         const PreparedView &view,
         const Vec3 &anchorEcefM,
         std::string &error) {
  const Viewpoint &eye = view.Eye;
  const auto lens = Lens::From(eye, renderer.PictureW(), renderer.PictureH());
  if (!lens) {
    error = Says::kInvalidLens;
    return false;
  }
  if (!view.HasExplicitCamera && !ClearsNearPlane(geometry, eye, view.FramedParts, error)) {
    return false;
  }
  Vec3 position;
  position = eye.EyeM + anchorEcefM;
  renderer.SetCamera({.EyeM = position, .Forward = eye.Forward, .Right = eye.Right, .Up = eye.Up},
                     *lens);
  return true;
}

bool BindIllumination(SceneRenderer &renderer,
                      const RenderObject &proxy,
                      RenderObjectPreparation &scratch,
                      std::string &error) {
  if (!PlaceLights(proxy, scratch.Lights, error)) { return false; }
  if (!renderer.SetGeometryLights(scratch.Lights, error)) { return false; }
  renderer.SetGeometryEnvironment(proxy.IndirectLight());
  return true;
}

bool Surface(SceneRenderer &renderer,
             const RenderObject &proxy,
             [[maybe_unused]] const PreparedView &view,
             RenderObjectPreparation &scratch,
             std::string &error) {
  if (proxy.GeometryView() == nullptr) {
    error = "the proxy declares no geometry";
    return false;
  }
  if (!Agrees(proxy, *proxy.GeometryView(), error)) { return false; }
  if (!renderer.SetGeometryMaterials(proxy.Slots(), error)) { return false; }
  return BindIllumination(renderer, proxy, scratch, error);
}

namespace {

size_t
DigestBytes(uint64_t &digest, const void *source, size_t bytes, size_t &offset, size_t wordsMost) {
  const size_t remaining = bytes - offset;
  const size_t count = wordsMost >= (remaining + sizeof(uint32_t) - 1u) / sizeof(uint32_t)
                           ? remaining
                           : wordsMost * sizeof(uint32_t);
  if (count == 0) { return 0; }
  const auto *at = static_cast<const unsigned char *>(source) + offset;
  for (size_t byte = 0; byte < count; ++byte) { digest = DigestFolded(digest, at[byte]); }
  offset += count;
  return count / sizeof(uint32_t);
}

bool PackIndexSlice(const RenderGeometryView &geometry,
                    RenderObjectPreparation &scratch,
                    size_t wordsMost) {
  RenderObjectPreparation::PackCursor &cursor = scratch.Packing;
  const std::vector<IndexRun> &runs = scratch.Draws.Runs();
  if (cursor.Run == 0 && cursor.RunWord == 0) {
    scratch.Indices.reserve(scratch.Draws.IndexCount());
  }
  while (cursor.Run < runs.size() && wordsMost > 0) {
    const IndexRun &run = runs[cursor.Run];
    const size_t count = std::min(wordsMost, static_cast<size_t>(run.Count) - cursor.RunWord);
    const size_t first = static_cast<size_t>(run.SourceFirst) + cursor.RunWord;
    const std::span<const uint32_t> section = geometry.Indices.subspan(first, count);
    scratch.Indices.insert(scratch.Indices.end(), section.begin(), section.end());
    cursor.RunWord += count;
    wordsMost -= count;
    if (cursor.RunWord == run.Count) {
      ++cursor.Run;
      cursor.RunWord = 0;
    }
  }
  if (cursor.Run != runs.size()) { return false; }
  cursor.Next = RenderObjectPreparation::PackCursor::Phase::Positions;
  return false;
}

bool PackPositionSlice(const RenderGeometryView &geometry,
                       RenderObjectPreparation &scratch,
                       size_t wordsMost) {
  RenderObjectPreparation::PackCursor &cursor = scratch.Packing;
  if (cursor.Part == 0 && cursor.PartWord == 0) {
    scratch.Vertices.resize(geometry.VertexCount() * 3u);
  }
  while (cursor.Part < geometry.Parts.size() && wordsMost > 0) {
    const RenderGeometryPart &part = geometry.Parts[cursor.Part];
    const size_t partWords = part.VertexCount * 3u;
    const size_t count = std::min(wordsMost, partWords - cursor.PartWord);
    float *const into = scratch.Vertices.data() + cursor.PositionWord;
    if (part.PositionsM.size() >= partWords) {
      std::memcpy(into, part.PositionsM.data() + cursor.PartWord, count * sizeof(float));
    } else {
      std::memset(into, 0, count * sizeof(float));
    }
    cursor.PositionWord += count;
    cursor.PartWord += count;
    wordsMost -= count;
    if (cursor.PartWord == partWords) {
      ++cursor.Part;
      cursor.PartWord = 0;
    }
  }
  if (cursor.Part != geometry.Parts.size()) { return false; }
  cursor.Digest = kDigestBasis;
  cursor.Next = scratch.Digests ? RenderObjectPreparation::PackCursor::Phase::DigestIndices
                                : RenderObjectPreparation::PackCursor::Phase::Done;
  if (!scratch.Digests) { scratch.PreparedGeometry = &geometry; }
  return cursor.Next == RenderObjectPreparation::PackCursor::Phase::Done;
}

bool DigestIndexSlice(RenderObjectPreparation &scratch, size_t wordsMost) {
  RenderObjectPreparation::PackCursor &cursor = scratch.Packing;
  const size_t bytes = scratch.Indices.size() * sizeof(uint32_t);
  DigestBytes(cursor.Digest, scratch.Indices.data(), bytes, cursor.DigestByte, wordsMost);
  if (cursor.DigestByte != bytes) { return false; }
  cursor.DigestByte = 0;
  cursor.Part = 0;
  cursor.Next = RenderObjectPreparation::PackCursor::Phase::DigestChannels;
  return false;
}

bool DigestChannelSlice(const RenderGeometryView &geometry,
                        RenderObjectPreparation &scratch,
                        size_t wordsMost) {
  constexpr std::array<std::span<const float> RenderGeometryPart::*, 6> channels{
      &RenderGeometryPart::PositionsM,
      &RenderGeometryPart::Normals,
      &RenderGeometryPart::Tangents,
      &RenderGeometryPart::Uv,
      &RenderGeometryPart::Uv1,
      &RenderGeometryPart::Colours};
  RenderObjectPreparation::PackCursor &cursor = scratch.Packing;
  while (cursor.Part < geometry.Parts.size() && wordsMost > 0) {
    const std::span<const float> values =
        geometry.Parts[cursor.Part].*channels[cursor.DigestChannel];
    const size_t bytes = values.size() * sizeof(float);
    const size_t taken =
        DigestBytes(cursor.Digest, values.data(), bytes, cursor.DigestByte, wordsMost);
    wordsMost -= taken;
    if (cursor.DigestByte != bytes) { break; }
    cursor.DigestByte = 0;
    if (++cursor.DigestChannel == channels.size()) {
      cursor.DigestChannel = 0;
      ++cursor.Part;
    }
  }
  if (cursor.Part != geometry.Parts.size()) { return false; }
  scratch.Metrics.GeometryDigest = cursor.Digest;
  scratch.PreparedGeometry = &geometry;
  cursor.Next = RenderObjectPreparation::PackCursor::Phase::Done;
  return true;
}

}

bool PlanPlacement(SceneRenderer &renderer,
                   const RenderObject &proxy,
                   const PreparedView &view,
                   RenderObjectPreparation &scratch,
                   std::string &error) {
  scratch.PlannedGeometry = nullptr;
  scratch.PreparedGeometry = nullptr;
  if (proxy.GeometryView() == nullptr) {
    error = "the proxy declares no geometry";
    return false;
  }
  const RenderGeometryView &geometry = *proxy.GeometryView();
  if (geometry.TriangleCount() == 0) {
    error = "the geometry carries no triangle, so there is nothing to stand in the proxy";
    return false;
  }
  if (!Agrees(proxy, geometry, error)) { return false; }
  if (!proxy.Previous().empty() && proxy.Previous().size() / 3 != geometry.VertexCount()) {
    error = "the proxy's previous pose carries " + std::to_string(proxy.Previous().size() / 3) +
            " vertices and this one carries " + std::to_string(geometry.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, geometry, view, proxy.Anchor(), error)) { return false; }

  scratch.Metrics = {};
  scratch.Packing = {};
  scratch.Indices.clear();
  scratch.Vertices.clear();
  const auto planFrom = std::chrono::steady_clock::now();
  {
    static const Heap::Tag kInsideTag("draw-list");
    const Heap::Tagged inside(kInsideTag);
    if (!BuildDrawList(proxy, view, geometry, scratch.Draws, error)) { return false; }
  }
  scratch.Metrics.DrawPlanMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - planFrom)
          .count();
  scratch.PlannedGeometry = &geometry;
  return true;
}

bool PackPlacement(const RenderObject &proxy,
                   RenderObjectPreparation &scratch,
                   std::string &error) {
  while (true) {
    auto packed = AdvancePackPlacement(proxy, scratch, std::numeric_limits<size_t>::max());
    if (!packed) {
      error = std::move(packed.error());
      return false;
    }
    if (*packed) { return true; }
  }
}

std::expected<bool, std::string> AdvancePackPlacement(const RenderObject &proxy,
                                                      RenderObjectPreparation &scratch,
                                                      size_t wordsMost) {
  const RenderGeometryView *const source = proxy.GeometryView();
  if (source == nullptr || source != scratch.PlannedGeometry) {
    return std::unexpected("geometry packing no longer matches its draw plan");
  }
  if (wordsMost == 0) { return false; }
  static const Heap::Tag kPackingTag("index-run");
  const Heap::Tagged packing(kPackingTag);
  const auto began = std::chrono::steady_clock::now();
  const RenderObjectPreparation::PackCursor::Phase phase = scratch.Packing.Next;
  bool complete = false;
  switch (phase) {
    case RenderObjectPreparation::PackCursor::Phase::Indices:
      complete = PackIndexSlice(*source, scratch, wordsMost);
      break;
    case RenderObjectPreparation::PackCursor::Phase::Positions:
      complete = PackPositionSlice(*source, scratch, wordsMost);
      break;
    case RenderObjectPreparation::PackCursor::Phase::DigestIndices:
      complete = DigestIndexSlice(scratch, wordsMost);
      break;
    case RenderObjectPreparation::PackCursor::Phase::DigestChannels:
      complete = DigestChannelSlice(*source, scratch, wordsMost);
      break;
    case RenderObjectPreparation::PackCursor::Phase::Done: return true;
  }
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (phase == RenderObjectPreparation::PackCursor::Phase::Indices ||
      phase == RenderObjectPreparation::PackCursor::Phase::Positions) {
    scratch.Metrics.PackingMs += elapsed;
  } else {
    scratch.Metrics.DigestMs += elapsed;
  }
  return complete;
}

bool PreparePlacement(SceneRenderer &renderer,
                      const RenderObject &proxy,
                      const PreparedView &view,
                      RenderObjectPreparation &scratch,
                      std::string &error) {
  return PlanPlacement(renderer, proxy, view, scratch, error) &&
         PackPlacement(proxy, scratch, error);
}

namespace {

const RenderGeometryView *PreparedGeometry(const RenderObject &proxy,
                                           const RenderObjectPreparation &scratch) {
  const RenderGeometryView *const source = proxy.GeometryView();
  if (source == nullptr || source != scratch.PreparedGeometry ||
      scratch.Indices.size() != scratch.Draws.IndexCount() ||
      scratch.Vertices.size() != source->VertexCount() * 3u) {
    return nullptr;
  }
  return source;
}

struct PlacementMeshInput {
  ChannelPack Positions;
  ChannelPack Uv;
  ChannelPack Uv1;
  ChannelPack Normals;
  ChannelPack Tangents;
  ChannelPack Colours;
  EmitPack Emitted;
  MeshInput Mesh;

  PlacementMeshInput(const RenderGeometryView &geometry,
                     const RenderObject &proxy,
                     const RenderObjectPreparation &scratch)
      : Positions{.From = &geometry, .Channel = &RenderGeometryPart::PositionsM, .Wide = 3},
        Uv{.From = &geometry, .Channel = &RenderGeometryPart::Uv, .Wide = 2},
        Uv1{.From = &geometry, .Channel = &RenderGeometryPart::Uv1, .Wide = 2},
        Normals{.From = &geometry, .Channel = &RenderGeometryPart::Normals, .Wide = 3},
        Tangents{.From = &geometry, .Channel = &RenderGeometryPart::Tangents, .Wide = 4},
        Colours{.From = &geometry, .Channel = &RenderGeometryPart::Colours, .Wide = 4},
        Emitted{.From = &geometry, .Proxy = &proxy} {
    Mesh.Verts = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Positions};
    if (geometry.CarriesUv) {
      Mesh.Uv = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Uv};
    }
    if (geometry.CarriesUv1) {
      Mesh.Uv1 = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Uv1};
    }
    if (geometry.CarriesNormal) {
      Mesh.Normals = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Normals};
    }
    if (geometry.CarriesTangent) {
      Mesh.Tangents = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Tangents};
    }
    if (geometry.CarriesColour) {
      Mesh.Colours = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &Colours};
    }
    Mesh.Emitted = VertexSource{.From = nullptr, .Writes = PackEmitted, .Carrying = &Emitted};
    Mesh.Positions = scratch.Vertices;
    if (!proxy.Previous().empty()) {
      Mesh.PrevVerts =
          VertexSource{.From = nullptr, .Writes = PackPrevious, .Carrying = &proxy.Previous()};
    }
    Mesh.VertexCount = static_cast<uint32_t>(geometry.VertexCount());
    Mesh.Indices = scratch.Indices.data();
    Mesh.IndexCount = static_cast<uint32_t>(scratch.Indices.size());
    for (int axis = 0; axis < 3; ++axis) { Mesh.Anchor[axis] = proxy.Anchor()[axis]; }
    Mesh.Draws = &scratch.Draws;
    Mesh.Clusters = geometry.Clusters;
    Mesh.ClusterSpheres = geometry.ClusterSpheres;
  }
};

void RecordsMeshUpload(const SceneRenderer &renderer, RenderObjectPreparation &scratch) {
  const GeometryUploadMetrics stages = renderer.LastGeometryMeshUpload();
  scratch.Metrics.MeshAdmissionMs = stages.AdmissionMs;
  scratch.Metrics.IndexUploadMs = stages.IndexMs;
  scratch.Metrics.StreamUploadMs = stages.StreamsMs;
  scratch.Metrics.TableUploadMs = stages.TablesMs;
}

}

std::expected<GeometryRenderer::MeshTicket, std::string> BeginPlacementUpload(
    SceneRenderer &renderer, const RenderObject &proxy, RenderObjectPreparation &scratch) {
  const RenderGeometryView *const source = PreparedGeometry(proxy, scratch);
  if (source == nullptr) {
    return std::unexpected("geometry placement no longer matches its prepared geometry");
  }
  const PlacementMeshInput input(*source, proxy, scratch);
  static const Heap::Tag kHandingTag("geometry-mesh");
  const Heap::Tagged handing(kHandingTag);
  const auto handedFrom = std::chrono::steady_clock::now();
  auto began = renderer.BeginGeometryMesh(input.Mesh);
  scratch.Metrics.UploadMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handedFrom)
          .count();
  RecordsMeshUpload(renderer, scratch);
  return began;
}

bool FinishPlacementUpload(SceneRenderer &renderer,
                           const RenderObject &proxy,
                           RenderObjectPreparation &scratch,
                           GeometryRenderer::MeshTicket ticket,
                           std::string &error) {
  const RenderGeometryView *const source = PreparedGeometry(proxy, scratch);
  if (source == nullptr) {
    error = "geometry placement no longer matches its prepared geometry";
    return false;
  }
  const PlacementMeshInput input(*source, proxy, scratch);
  static const Heap::Tag kHandingTag("geometry-mesh");
  const Heap::Tagged handing(kHandingTag);
  const auto handedFrom = std::chrono::steady_clock::now();
  const bool uploaded = renderer.FinishGeometryMesh(ticket, input.Mesh, error);
  scratch.Metrics.UploadMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handedFrom)
          .count();
  RecordsMeshUpload(renderer, scratch);
  if (!uploaded) { return false; }
  scratch.PlannedGeometry = nullptr;
  scratch.PreparedGeometry = nullptr;
  { std::vector<uint32_t>().swap(scratch.Indices); }
  { std::vector<float>().swap(scratch.Vertices); }
  return Placed(renderer, proxy, error);
}

bool SubmitPlacement(SceneRenderer &renderer,
                     const RenderObject &proxy,
                     RenderObjectPreparation &scratch,
                     std::string &error) {
  auto began = BeginPlacementUpload(renderer, proxy, scratch);
  if (!began) {
    error = std::move(began.error());
    return false;
  }
  return FinishPlacementUpload(renderer, proxy, scratch, *began, error);
}

bool Place(SceneRenderer &renderer,
           const RenderObject &proxy,
           const PreparedView &view,
           RenderObjectPreparation &scratch,
           std::string &error) {
  return PreparePlacement(renderer, proxy, view, scratch, error) &&
         SubmitPlacement(renderer, proxy, scratch, error);
}

bool Move(SceneRenderer &renderer,
          const RenderObject &proxy,
          const PreparedView &view,
          RenderObjectPreparation &scratch,
          std::string &error) {
  if (proxy.GeometryView() == nullptr) {
    error = "the proxy declares no geometry";
    return false;
  }
  const RenderGeometryView &geometry = *proxy.GeometryView();
  if (!Agrees(proxy, geometry, error)) { return false; }
  if (!proxy.Previous().empty() && proxy.Previous().size() / 3 != geometry.VertexCount()) {
    error = "the proxy's previous pose carries " + std::to_string(proxy.Previous().size() / 3) +
            " vertices and this one carries " + std::to_string(geometry.VertexCount()) +
            ", so no vertex has a place it moved from";
    return false;
  }
  if (!Aim(renderer, geometry, view, proxy.Anchor(), error)) { return false; }

  scratch.Metrics = {};
  const auto packingFrom = std::chrono::steady_clock::now();
  VertexInput pose;
  const ChannelPack positions{
      .From = &geometry, .Channel = &RenderGeometryPart::PositionsM, .Wide = 3};
  const ChannelPack uv{.From = &geometry, .Channel = &RenderGeometryPart::Uv, .Wide = 2};
  const ChannelPack uv1{.From = &geometry, .Channel = &RenderGeometryPart::Uv1, .Wide = 2};
  const ChannelPack normals{.From = &geometry, .Channel = &RenderGeometryPart::Normals, .Wide = 3};
  const ChannelPack tangents{
      .From = &geometry, .Channel = &RenderGeometryPart::Tangents, .Wide = 4};
  const ChannelPack colours{.From = &geometry, .Channel = &RenderGeometryPart::Colours, .Wide = 4};
  const EmitPack emitted{.From = &geometry, .Proxy = &proxy};

  pose.Verts = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &positions};
  if (geometry.CarriesUv) {
    pose.Uv = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &uv};
  }
  if (geometry.CarriesUv1) {
    pose.Uv1 = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &uv1};
  }
  if (geometry.CarriesNormal) {
    pose.Normals = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &normals};
  }
  if (geometry.CarriesTangent) {
    pose.Tangents = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &tangents};
  }
  if (geometry.CarriesColour) {
    pose.Colours = VertexSource{.From = nullptr, .Writes = PackChannel, .Carrying = &colours};
  }
  pose.Emitted = VertexSource{.From = nullptr, .Writes = PackEmitted, .Carrying = &emitted};
  scratch.Vertices.resize(geometry.VertexCount() * 3u);
  PackChannel(&positions, scratch.Vertices.data(), static_cast<uint32_t>(scratch.Vertices.size()));
  scratch.Metrics.PackingMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - packingFrom)
          .count();
  if (scratch.Digests) {
    const auto digestedFrom = std::chrono::steady_clock::now();
    unsigned long long digest = kDigestBasis;
    const auto *at = reinterpret_cast<const unsigned char *>(scratch.Vertices.data());
    for (size_t one = 0; one < scratch.Vertices.size() * sizeof(float); ++one) {
      digest = (digest ^ at[one]) * kDigestPrime;
    }
    scratch.Metrics.GeometryDigest = digest;
    scratch.Metrics.DigestMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - digestedFrom)
            .count();
  } else {
    scratch.Metrics.GeometryDigest = 0;
    scratch.Metrics.DigestMs = 0.0;
  }
  pose.Positions = scratch.Vertices;
  if (!proxy.Previous().empty()) {
    pose.PrevVerts =
        VertexSource{.From = nullptr, .Writes = PackPrevious, .Carrying = &proxy.Previous()};
  }
  pose.VertexCount = static_cast<uint32_t>(geometry.VertexCount());
  for (int axis = 0; axis < 3; ++axis) { pose.Anchor[axis] = proxy.Anchor()[axis]; }
  static const Heap::Tag kHandingTag("geometry-pose");
  const Heap::Tagged handing(kHandingTag);
  const auto handedFrom = std::chrono::steady_clock::now();
  const bool uploaded = renderer.SetGeometryVertexInput(pose, error);
  scratch.Metrics.UploadMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - handedFrom)
          .count();
  return uploaded;
}

}
