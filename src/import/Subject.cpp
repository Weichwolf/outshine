#include <format>
#include "math/Vec4.h"
#include "math/Mat4.h"
#include "Heap.h"
#include "Units.h"
#include "math/Vec3.h"
#include "Subject.h"

#include <array>
#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>

#include <scene/Geometry.h>

#include <numbers>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Document.h"
#include "Framing.h"
#include "Tangents.h"

namespace outshine::Gltf {

constexpr uint64_t kGoldenWord = 0x9e3779b97f4a7c15ull;

namespace {

[[nodiscard]] bool RunIsStatable(std::span<const float> run,
                                 size_t vertices,
                                 size_t components,
                                 const char *semantic,
                                 const std::string &where,
                                 std::string &why) {
  if (run.empty()) { return true; }
  if (run.size() != vertices * components) {
    why = where + " states " + std::to_string(run.size() / components) + " " + semantic + " over " +
          std::to_string(vertices) + " vertices";
    return false;
  }
  for (size_t at = 0; at < run.size(); ++at) {
    if (!std::isfinite(run[at])) {
      why = where + " states a " + semantic + " component that is not finite, at " +
            std::to_string(at);
      return false;
    }
  }
  return true;
}

const char *ModeName(PrimitiveMode mode) {
  switch (mode) {
    case PrimitiveMode::Points: return "POINTS";
    case PrimitiveMode::Lines: return "LINES";
    case PrimitiveMode::LineLoop: return "LINE_LOOP";
    case PrimitiveMode::LineStrip: return "LINE_STRIP";
    case PrimitiveMode::Triangles: return "TRIANGLES";
    case PrimitiveMode::TriangleStrip: return "TRIANGLE_STRIP";
    case PrimitiveMode::TriangleFan: return "TRIANGLE_FAN";
  }
  return "an undeclared mode";
}

bool DrawsASurface(PrimitiveMode mode) {
  return mode == PrimitiveMode::Triangles || mode == PrimitiveMode::TriangleStrip ||
         mode == PrimitiveMode::TriangleFan;
}

bool RunIsWhole(PrimitiveMode mode, size_t indices) {
  return (mode == PrimitiveMode::Triangles) ? (indices > 0 && indices % 3 == 0) : (indices >= 3);
}

enum class Handedness { Preserved, Reversed };

void Triangulate(PrimitiveMode mode,
                 Handedness handedness,
                 const std::vector<uint32_t> &run,
                 std::vector<uint32_t> &out) {
  out.clear();
  if (mode == PrimitiveMode::Triangles) {
    out = run;
  } else if (mode == PrimitiveMode::TriangleStrip) {
    for (size_t at = 0; at + 2 < run.size(); ++at) {
      const size_t flipped = (at % 2 == 0) ? 0u : 1u;
      out.push_back(run[at + flipped]);
      out.push_back(run[at + 1u - flipped]);
      out.push_back(run[at + 2u]);
    }
  } else {
    for (size_t at = 1; at + 1 < run.size(); ++at) {
      out.push_back(run[0]);
      out.push_back(run[at]);
      out.push_back(run[at + 1]);
    }
  }
  if (handedness == Handedness::Reversed) {
    for (size_t triangle = 0; triangle * 3 + 2 < out.size(); ++triangle) {
      std::swap(out[triangle * 3 + 1], out[triangle * 3 + 2]);
    }
  }
}

struct BasisKey {
  std::array<uint64_t, 4> Bits = {{}};

  bool operator<(const BasisKey &other) const {
    return std::memcmp(Bits.data(), other.Bits.data(), sizeof Bits) < 0;
  }
};

BasisKey KeyOf(double x, double y, double z, double w) {
  const Vec4 basis = {{x, y, z, w}};
  BasisKey key;
  for (size_t at = 0; at < 4; ++at) {
    const double folded = basis[at] == 0.0 ? 0.0 : basis[at];
    std::memcpy(&key.Bits[at], &folded, sizeof key.Bits[at]);
  }
  return key;
}

} // namespace

bool Subject::MorphDeltasFor(const Document &document,
                             const Primitive &primitive,
                             const char *semantic,
                             const double *weights,
                             size_t count,
                             size_t components,
                             size_t vertices,
                             std::vector<double> &out) {
  out.clear();
  if (count == 0 || primitive.Targets.empty()) { return true; }
  std::vector<double> delta;
  for (size_t target = 0; target < count && target < primitive.Targets.size(); ++target) {
    const double share = weights[target];
    if (share == 0.0) { continue; }
    const int accessor = primitive.Targets[target].Find(semantic);
    if (accessor < 0) { continue; }
    if (!document.ReadElements(accessor, delta)) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " +
                    semantic + " does not decode: " + document.Error());
    }
    if (delta.size() != vertices * components) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " +
                    semantic + " decodes to " + std::to_string(delta.size()) + " components over " +
                    std::to_string(vertices) + " vertices of " + std::to_string(components));
    }
    if (out.empty()) { out.assign(vertices * components, 0.0); }
    for (size_t at = 0; at < delta.size(); ++at) { out[at] += share * delta[at]; }
  }
  return true;
}

Transform Subject::JointMatrix(const Skin &skin, size_t joint, const Transform &world) {
  if (skin.InverseBind.empty()) { return world; }
  Mat4 bind;
  std::copy_n(skin.InverseBind.begin() + static_cast<ptrdiff_t>(joint * 16), 16, bind.begin());
  return world * Transform::FromColumnMajor(bind);
}

bool Subject::BlendSkinFor(const Document &document,
                           const Skin &skin,
                           std::span<const Transform> joints,
                           const Primitive &primitive,
                           size_t vertices,
                           std::vector<Transform> &out) {
  std::vector<double> index;
  std::vector<double> weight;
  size_t sets = 0;
  for (;; ++sets) {
    const std::string which = std::to_string(sets);
    const int bones = primitive.Find(("JOINTS_" + which).c_str());
    const int weights = primitive.Find(("WEIGHTS_" + which).c_str());
    if (bones < 0 && weights < 0) { break; }
    if (bones < 0 || weights < 0) {
      return Refuse(std::format(
          "{}: a primitive on a skinned node carries {}{} and {}{}, and a set without both "
          "binds no vertex to any joint",
          document.Path(),
          bones < 0 ? "no JOINTS_" : "JOINTS_",
          which,
          weights < 0 ? "no WEIGHTS_" : "WEIGHTS_",
          which));
    }
    std::vector<double> theseIndices;
    std::vector<double> theseWeights;
    if (!document.ReadElements(bones, theseIndices)) {
      return Refuse(document.Path() + ": JOINTS_" + which +
                    " does not decode: " + document.Error());
    }
    if (!document.ReadElements(weights, theseWeights)) {
      return Refuse(document.Path() + ": WEIGHTS_" + which +
                    " does not decode: " + document.Error());
    }
    if (theseIndices.size() != vertices * 4 || theseWeights.size() != vertices * 4) {
      return Refuse(std::format(
          "{}: JOINTS_{} decodes to {} and WEIGHTS_{} to {} sets over {} vertices, and glTF "
          "states both are VEC4",
          document.Path(),
          which,
          theseIndices.size() / 4,
          which,
          theseWeights.size() / 4,
          vertices));
    }
    index.insert(index.end(), theseIndices.begin(), theseIndices.end());
    weight.insert(weight.end(), theseWeights.begin(), theseWeights.end());
  }
  if (sets == 0) {
    return Refuse(
        document.Path() +
        ": a primitive on a skinned node carries no JOINTS_0 and no WEIGHTS_0, and a skin "
        "without both binds no vertex to any joint");
  }

  out.assign(vertices, Transform());
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    Mat4 blended = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    double sum = 0;
    for (size_t set = 0; set < sets; ++set) {
      const size_t base = set * vertices * 4 + vertex * 4;
      for (size_t slot = 0; slot < 4; ++slot) {
        const double share = weight[base + slot];
        if (share == 0.0) { continue; }
        const double named = index[base + slot];
        if (!(named >= 0.0) || static_cast<size_t>(named) >= joints.size()) {
          return Refuse(document.Path() + ": JOINTS_" + std::to_string(set) + " of vertex " +
                        std::to_string(vertex) + " names joint " +
                        std::to_string(static_cast<long long>(named)) + " and the skin declares " +
                        std::to_string(joints.size()));
        }
        sum += share;
        const Transform &matrix = joints[static_cast<size_t>(named)];
        for (int at = 0; at < 16; ++at) { blended[at] += share * matrix.M[at]; }
      }
    }
    if (sum == 0.0) {
      return Refuse(document.Path() + ": the weights of vertex " + std::to_string(vertex) +
                    " sum to zero over all " + std::to_string(sets) +
                    " sets, so the vertex is bound to no joint and names no position");
    }
    for (int at = 0; at < 16; ++at) { out[vertex].M[at] = blended[at]; }
  }
  (void)skin;
  return true;
}

bool Subject::SuppliedTangentsFor(const Document &document,
                                  const Primitive &primitive,
                                  const VertexPlacement &place,
                                  std::span<const double> morphWeights,
                                  Part &part,
                                  size_t vertices,
                                  std::vector<double> &into) {
  const int supplied = primitive.Find("TANGENT");
  if (supplied >= 0) {
    std::vector<double> elements;
    if (!document.ReadElements(supplied, elements)) {
      return Refuse(document.Path() + ": TANGENT does not decode: " + document.Error());
    }
    if (elements.size() != vertices * 4) {
      return Refuse(document.Path() + ": TANGENT decodes to " +
                    std::to_string(elements.size() / 4) + " vectors over " +
                    std::to_string(vertices) + " vertices");
    }

    std::vector<double> morphedTangents;
    if (!MorphDeltasFor(document,
                        primitive,
                        "TANGENT",
                        morphWeights.data(),
                        morphWeights.size(),
                        3,
                        vertices,
                        morphedTangents)) {
      return false;
    }
    for (size_t vertex = 0; vertex < vertices && !morphedTangents.empty(); ++vertex) {
      for (size_t axis = 0; axis < 3; ++axis) {
        elements[vertex * 4 + axis] += morphedTangents[vertex * 3 + axis];
      }
    }

    into.assign(vertices * 4, 0.0);
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const Transform &placed = place.At(vertex);
      const double mirrored = placed.LinearDeterminant() < 0 ? -1.0 : 1.0;
      const Vec3 local = {
          {elements[vertex * 4], elements[vertex * 4 + 1], elements[vertex * 4 + 2]}};
      Vec3 global;
      placed.Direction(local, global);
      (void)Normalise(global);
      for (int axis = 0; axis < 3; ++axis) {
        into[vertex * 4 + static_cast<size_t>(axis)] = global[axis];
      }
      into[vertex * 4 + 3] = elements[vertex * 4 + 3] * mirrored;
    }
    part.Tangent = TangentSource::Supplied;
    return true;
  }
  return true;
}

bool Subject::GeneratedTangentsFor(Part &part) {
  Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
  if (part.Tangent == TangentSource::Supplied) { return true; }
  const bool needed = part.Material >= 0 &&
                      static_cast<size_t>(part.Material) < TangentWanted_.size() &&
                      TangentWanted_[static_cast<size_t>(part.Material)] != 0;
  if (!needed || !part.HasNormal || !part.HasUv || part.IndexCount == 0) { return true; }

  TangentSubject over;
  over.PositionsM = Positions_.data();
  over.Normals = Normals_.data();
  over.Uv = Uv_.data();
  over.VertexCount = VertexCount();
  over.Indices = Indices_.data() + part.FirstIndex;
  over.IndexCount = part.IndexCount;
  std::vector<double> corners;
  std::string error;
  if (!GenerateTangents(over, corners, error)) {
    return Refuse("the tangent basis the material needs cannot be generated: " + error);
  }

  std::map<BasisKey, uint32_t> split;
  std::vector<char> written(VertexCount(), 0);
  for (size_t corner = 0; corner < part.IndexCount; ++corner) {
    const uint32_t vertex = Indices_[part.FirstIndex + corner];
    const double *basis = &corners[corner * 4];
    if (written[vertex] == 0) {
      for (size_t at = 0; at < 4; ++at) {
        Tangents_[static_cast<size_t>(vertex) * 4 + at] = basis[at];
      }
      written[vertex] = 1;
      continue;
    }
    if (std::ranges::equal(std::span(Tangents_).subspan(static_cast<size_t>(vertex) * 4, 4),
                           std::span(basis, 4))) {
      continue;
    }
    BasisKey key = KeyOf(basis[0], basis[1], basis[2], basis[3]);

    key.Bits[0] ^= static_cast<uint64_t>(vertex) * kGoldenWord;
    const auto found = split.find(key);
    if (found != split.end()) {
      Indices_[part.FirstIndex + corner] = found->second;
      continue;
    }
    const auto made = static_cast<uint32_t>(VertexCount());
    for (size_t axis = 0; axis < 3; ++axis) {
      Positions_.push_back(Positions_[static_cast<size_t>(vertex) * 3 + axis]);
    }
    Uv_.resize(static_cast<size_t>(made) * 2 + 2, 0.0);
    Uv_[static_cast<size_t>(made) * 2] = Uv_[static_cast<size_t>(vertex) * 2];
    Uv_[static_cast<size_t>(made) * 2 + 1] = Uv_[vertex * 2 + 1];

    if (!Uv1_.empty()) {
      Uv1_.resize(static_cast<size_t>(made) * 2 + 2, 0.0);
      Uv1_[static_cast<size_t>(made) * 2] = Uv1_[static_cast<size_t>(vertex) * 2];
      Uv1_[static_cast<size_t>(made) * 2 + 1] = Uv1_[vertex * 2 + 1];
    }

    if (!Colours_.empty()) {
      Colours_.resize(static_cast<size_t>(made) * 4 + 4, 0.0);
      for (size_t channel = 0; channel < 4; ++channel) {
        Colours_[static_cast<size_t>(made) * 4 + channel] =
            Colours_[static_cast<size_t>(vertex) * 4 + channel];
      }
    }
    Normals_.resize(static_cast<size_t>(made) * 3 + 3, 0.0);
    for (size_t axis = 0; axis < 3; ++axis) {
      Normals_[static_cast<size_t>(made) * 3 + axis] =
          Normals_[static_cast<size_t>(vertex) * 3 + axis];
    }
    Tangents_.resize(static_cast<size_t>(made) * 4 + 4, 0.0);
    for (size_t at = 0; at < 4; ++at) { Tangents_[static_cast<size_t>(made) * 4 + at] = basis[at]; }
    split.emplace(key, made);
    Indices_[part.FirstIndex + corner] = made;
  }
  part.Tangent = TangentSource::Generated;
  return true;
}

bool Subject::FlatNormalsFor(Part &part) {
  if (part.HasNormal || part.IndexCount == 0) { return true; }
  const size_t before = VertexCount();
  Normals_.resize(Positions_.size(), 0.0);

  std::vector<char> owned(before, 0);
  for (size_t triangle = 0; triangle + 2 < part.IndexCount; triangle += 3) {
    std::array<uint32_t, 3> of = {{Indices_[part.FirstIndex + triangle],
                                   Indices_[part.FirstIndex + triangle + 1],
                                   Indices_[part.FirstIndex + triangle + 2]}};
    for (size_t corner = 0; corner < 3; ++corner) {
      const uint32_t vertex = of[corner];
      if (vertex < before && (owned[vertex] == 0)) {
        owned[vertex] = 1;
        continue;
      }

      const auto made = static_cast<uint32_t>(VertexCount());
      for (size_t axis = 0; axis < 3; ++axis) {
        Positions_.push_back(Positions_[static_cast<size_t>(vertex) * 3 + axis]);
      }
      Normals_.resize(static_cast<size_t>(made) * 3 + 3, 0.0);
      if (!Uv_.empty()) {
        Uv_.resize(static_cast<size_t>(made) * 2 + 2, 0.0);
        Uv_[static_cast<size_t>(made) * 2] = Uv_[static_cast<size_t>(vertex) * 2];
        Uv_[static_cast<size_t>(made) * 2 + 1] = Uv_[static_cast<size_t>(vertex) * 2 + 1];
      }
      if (!Uv1_.empty()) {
        Uv1_.resize(static_cast<size_t>(made) * 2 + 2, 0.0);
        Uv1_[static_cast<size_t>(made) * 2] = Uv1_[static_cast<size_t>(vertex) * 2];
        Uv1_[static_cast<size_t>(made) * 2 + 1] = Uv1_[static_cast<size_t>(vertex) * 2 + 1];
      }
      if (!Colours_.empty()) {
        Colours_.resize(static_cast<size_t>(made) * 4 + 4, 0.0);
        for (size_t channel = 0; channel < 4; ++channel) {
          Colours_[static_cast<size_t>(made) * 4 + channel] =
              Colours_[static_cast<size_t>(vertex) * 4 + channel];
        }
      }
      if (!Tangents_.empty()) { Tangents_.resize(static_cast<size_t>(made) * 4 + 4, 0.0); }
      of[corner] = made;
      Indices_[part.FirstIndex + triangle + corner] = made;
    }

    std::array<std::array<double, 3>, 2> edge;
    for (size_t axis = 0; axis < 3; ++axis) {
      edge[0][axis] = Positions_[static_cast<size_t>(of[1]) * 3 + axis] -
                      Positions_[static_cast<size_t>(of[0]) * 3 + axis];
      edge[1][axis] = Positions_[static_cast<size_t>(of[2]) * 3 + axis] -
                      Positions_[static_cast<size_t>(of[0]) * 3 + axis];
    }
    Vec3 face = {{edge[0][1] * edge[1][2] - edge[0][2] * edge[1][1],
                  edge[0][2] * edge[1][0] - edge[0][0] * edge[1][2],
                  edge[0][0] * edge[1][1] - edge[0][1] * edge[1][0]}};
    const double length = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);

    if (length > 0.0) {
      for (double &axis : face) { axis /= length; }
    } else {
      face[0] = face[1] = face[2] = 0.0;
    }
    for (const unsigned int corner : of) {
      for (size_t axis = 0; axis < 3; ++axis) {
        Normals_[static_cast<size_t>(corner) * 3 + axis] = face[axis];
      }
    }
  }
  part.HasNormal = true;
  part.VertexCount += VertexCount() - before;
  return true;
}

bool ViewOf(const Viewpoint &from, Transform &out) {
  Transform world;
  for (int axis = 0; axis < 3; ++axis) {
    world.M[axis] = from.Right[axis];
    world.M[4 + axis] = from.Up[axis];
    world.M[8 + axis] = -from.Forward[axis];
    world.M[12 + axis] = from.EyeM[axis];
  }
  world.M[3] = world.M[7] = world.M[11] = 0;
  world.M[15] = 1;
  return world.Inverse(out);
}

bool ClipOf(const Viewpoint &from, double viewportAspect, Transform &out) {
  Camera lens;
  lens.Kind = from.Kind == Render::CameraKind::Orthographic ? CameraKind::Orthographic
                                                            : CameraKind::Perspective;
  lens.YfovRad = from.YfovRad;
  lens.XMagM = from.XMagM;
  lens.YMagM = from.YMagM;
  lens.ZNearM = from.ZNearM;
  lens.ZFarM = from.ZFarM;
  Transform projection;
  Transform view;
  if (!lens.Projection(viewportAspect, projection)) { return false; }
  if (!ViewOf(from, view)) { return false; }
  out = projection * view;
  return true;
}

bool Subject::Refuse(std::string why) {
  Error_ = std::move(why);
  Positions_.clear();
  Uv_.clear();
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  return false;
}

bool Subject::Build(const Document &document, const VariantSelection &variant) {
  return Flatten(document, nullptr, nullptr, variant);
}

bool Subject::Build(const Document &document,
                    std::span<const Transform> pose,
                    std::span<const double> weights,
                    const VariantSelection &variant) {
  if (pose.size() != document.Nodes().size()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(pose.size()) +
                  " local transforms and the file carries " +
                  std::to_string(document.Nodes().size()) + " nodes");
  }

  if (weights.size() != 0 && weights.size() != document.MorphWeightsTotal()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(weights.size()) +
                  " morph weights and the file's nodes carry " +
                  std::to_string(document.MorphWeightsTotal()));
  }
  return Flatten(document, pose.data(), (weights.size() != 0u) ? weights.data() : nullptr, variant);
}

namespace {

bool InstanceTransforms(const Document &document,
                        const Node &node,
                        const Transform &world,
                        std::vector<Transform> &out) {
  out.clear();
  const std::array<int, 3> named = {
      {node.InstanceTranslation, node.InstanceRotation, node.InstanceScale}};
  bool any = false;
  for (const int accessor : named) { any = any || accessor >= 0; }
  if (!any) {
    out.push_back(world);
    return true;
  }
  std::vector<double> translation;
  std::vector<double> rotation;
  std::vector<double> scale;
  if (node.InstanceTranslation >= 0 &&
      !document.ReadElements(node.InstanceTranslation, translation)) {
    return false;
  }
  if (node.InstanceRotation >= 0 && !document.ReadElements(node.InstanceRotation, rotation)) {
    return false;
  }
  if (node.InstanceScale >= 0 && !document.ReadElements(node.InstanceScale, scale)) {
    return false;
  }
  size_t count = 0;
  if (!translation.empty()) {
    count = translation.size() / 3;
  } else if (!rotation.empty()) {
    count = rotation.size() / 4;
  } else if (!scale.empty()) {
    count = scale.size() / 3;
  }
  out.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const Vec3 t = {{translation.empty() ? 0.0 : translation[at * 3 + 0],
                     translation.empty() ? 0.0 : translation[at * 3 + 1],
                     translation.empty() ? 0.0 : translation[at * 3 + 2]}};
    const Quat r = {.X = rotation.empty() ? 0.0 : rotation[at * 4 + 0],
                    .Y = rotation.empty() ? 0.0 : rotation[at * 4 + 1],
                    .Z = rotation.empty() ? 0.0 : rotation[at * 4 + 2],
                    .W = rotation.empty() ? 1.0 : rotation[at * 4 + 3]};
    const Vec3 sc = {{scale.empty() ? 1.0 : scale[at * 3 + 0],
                      scale.empty() ? 1.0 : scale[at * 3 + 1],
                      scale.empty() ? 1.0 : scale[at * 3 + 2]}};
    out.push_back(world * Transform::FromTrs(t, r, sc));
  }
  return true;
}
} // namespace

bool Subject::Flatten(const Document &document,
                      const Transform *pose,
                      const double *weights,
                      const VariantSelection &variant) {
  const auto placementOf = [&document, pose](int node, Transform &out) {
    return pose ? document.WorldTransform(
                      node, std::span<const Transform>(pose, document.Nodes().size()), out)
                : document.WorldTransform(node, out);
  };
  Error_.clear();
  Positions_.clear();
  Uv_.clear();
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  Undrawn_ = Undrawn();
  outshine::Geometry &made = Scratch_.Made;
  made.clear();
  for (const MaterialRef &declared : document.Materials()) {
    Material row = declared.Surface;
    row.NeedsTangents = declared.Normal.Texture >= 0;
    (void)made.addSurface("", row);
  }
  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || static_cast<size_t>(sceneIndex) >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  int activeVariant = -1;
  {
    std::string why;
    if (!variant.Against(document, activeVariant, why)) {
      return Refuse(document.Path() + ": the declaration " + why);
    }
  }

  std::vector<int> pending(document.Scenes()[static_cast<size_t>(sceneIndex)].Roots.rbegin(),
                           document.Scenes()[static_cast<size_t>(sceneIndex)].Roots.rend());

  std::vector<double> &elements = Scratch_.Elements;
  std::vector<uint32_t> &run = Scratch_.Run;
  std::vector<uint32_t> &indices = Scratch_.Loop;
  size_t primitives = 0;
  while (!pending.empty()) {
    const int nodeIndex = pending.back();
    pending.pop_back();
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= document.Nodes().size()) {
      return Refuse(document.Path() + ": scene names node " + std::to_string(nodeIndex) +
                    ", which the file does not carry");
    }
    const Node &node = document.Nodes()[static_cast<size_t>(nodeIndex)];
    if (!node.Visible) { continue; }
    for (const int child : std::views::reverse(node.Children)) { pending.push_back(child); }
    if (node.Light >= 0) {
      Transform placement;
      if (!placementOf(nodeIndex, placement)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and has no world transform: " + document.Error());
      }
      const LightRef &declared = document.Lights()[static_cast<size_t>(node.Light)];
      PlacedLight placed;
      placed.NodeName = node.Name;
      placed.LightName = declared.Name;
      placed.Light = declared.Light;
      const Vec3 origin = {{0, 0, 0}};
      Vec3 position;
      placement.Point(origin, position);

      const Vec3 axis = {{0, 0, -1}};
      Vec3 beam;
      placement.Direction(axis, beam);
      if (!Normalise(beam)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and its transform collapses the beam to zero length");
      }
      for (int component = 0; component < 3; ++component) {
        placed.Light.Position[component] = static_cast<float>(position[component]);
        placed.Light.Direction[component] = static_cast<float>(beam[component]);
      }
      Lights_.push_back(std::move(placed));
    }
    if (node.Mesh < 0) { continue; }
    if (static_cast<size_t>(node.Mesh) >= document.Meshes().size()) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) + " names mesh " +
                    std::to_string(node.Mesh) + ", which the file does not carry");
    }
    Transform world;
    if (!placementOf(nodeIndex, world)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " has no world transform: " + document.Error());
    }

    const size_t morphCount = document.MorphWeightsCount(nodeIndex);
    std::vector<double> &nodeWeights = Scratch_.NodeWeights;
    nodeWeights.clear();
    if (morphCount > 0) {
      const std::vector<double> &declared =
          document.Meshes()[static_cast<size_t>(node.Mesh)].Weights;
      for (size_t at = 0; at < morphCount; ++at) {
        const double asDeclared = at < declared.size() ? declared[at] : 0.0;
        nodeWeights.push_back((weights != nullptr)
                                  ? weights[document.MorphWeightsFirst(nodeIndex) + at]
                                  : asDeclared);
      }
    }

    std::vector<Transform> &jointMatrices = Scratch_.Joints;
    jointMatrices.clear();
    if (node.Skin >= 0) {
      const Skin &skin = document.Skins()[static_cast<size_t>(node.Skin)];
      jointMatrices.assign(skin.Joints.size(), Transform());
      for (size_t joint = 0; joint < skin.Joints.size(); ++joint) {
        Transform placed;
        if (!placementOf(skin.Joints[joint], placed)) {
          return Refuse(document.Path() + ": joint node " + std::to_string(skin.Joints[joint]) +
                        " has no world transform: " + document.Error());
        }
        jointMatrices[joint] = JointMatrix(skin, joint, placed);
      }
    }

    std::vector<Transform> &instances = Scratch_.Instances;
    instances.clear();
    if (!InstanceTransforms(document, node, world, instances)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " instances on an accessor this reader cannot decode: " + document.Error());
    }
    for (const Transform &placedWorld : instances) {
      for (const Primitive &primitive :
           document.Meshes()[static_cast<size_t>(node.Mesh)].Primitives) {
        ++primitives;
        Part part;
        part.NodeName = node.Name;
        part.Material = primitive.MaterialUnder(activeVariant);
        part.FirstVertex = 0;
        part.FirstIndex = 0;
        std::vector<double> &atPos = Scratch_.Pos;
        std::vector<double> &atNor = Scratch_.Nor;
        std::vector<double> &atUv = Scratch_.Uv;
        std::vector<double> &atUv1 = Scratch_.Uv1;
        std::vector<double> &atCol = Scratch_.Col;
        std::vector<double> &atTan = Scratch_.Tan;
        std::vector<uint32_t> &atIdx = Scratch_.Idx;
        atPos.clear();
        atNor.clear();
        atUv.clear();
        atUv1.clear();
        atCol.clear();
        atTan.clear();
        atIdx.clear();

        if (!DrawsASurface(primitive.Mode)) {
          ++Undrawn_.Primitives;
          const auto mode = static_cast<size_t>(primitive.Mode);
          if (mode < 7) { ++Undrawn_.ByMode[mode]; }

          continue;
        }
        const int position = primitive.Find("POSITION");
        if (position < 0) {
          return Refuse(document.Path() + ": primitive of mesh " + std::to_string(node.Mesh) +
                        " carries no POSITION, and nothing here invents one");
        }
        if (!document.ReadElements(position, elements)) {
          return Refuse(document.Path() + ": POSITION does not decode: " + document.Error());
        }
        if (elements.size() % 3 != 0) {
          return Refuse(document.Path() + ": POSITION decodes to " +
                        std::to_string(elements.size()) +
                        " components, which is not a whole number of points");
        }
        const size_t vertices = elements.size() / 3;

        std::vector<double> &morphedPositions = Scratch_.Morphed;
        morphedPositions.clear();
        if (!MorphDeltasFor(document,
                            primitive,
                            "POSITION",
                            nodeWeights.data(),
                            morphCount,
                            3,
                            vertices,
                            morphedPositions)) {
          return false;
        }
        for (size_t at = 0; at < morphedPositions.size(); ++at) {
          elements[at] += morphedPositions[at];
        }
        std::vector<Transform> &skinned = Scratch_.Skinned;
        skinned.clear();
        if (node.Skin >= 0 && !BlendSkinFor(document,
                                            document.Skins()[static_cast<size_t>(node.Skin)],
                                            jointMatrices,
                                            primitive,
                                            vertices,
                                            skinned)) {
          return false;
        }
        const VertexPlacement place{.Node = placedWorld,
                                    .Skinned = skinned.empty() ? nullptr : skinned.data()};
        for (size_t vertex = 0; vertex < vertices; ++vertex) {
          const Vec3 local = {
              {elements[vertex * 3], elements[vertex * 3 + 1], elements[vertex * 3 + 2]}};
          Vec3 global;
          place.At(vertex).Point(local, global);
          for (const double axis : global) { atPos.push_back(axis); }
        }

        struct UvSetRow {
          const char *Semantic;
          bool Part::*Carried;
          bool *Any;
          std::vector<double> *Into;
        };

        const std::array<UvSetRow, kUvSets> sets = {
            {{.Semantic = "TEXCOORD_0", .Carried = &Part::HasUv, .Any = &anyUv, .Into = &atUv},
             {.Semantic = "TEXCOORD_1", .Carried = &Part::HasUv1, .Any = &anyUv1, .Into = &atUv1}}};

        for (const auto &set : sets) {
          const int uv = primitive.Find(set.Semantic);
          part.*set.Carried = uv >= 0;
          *set.Any = *set.Any || part.*set.Carried;
          set.Into->assign(vertices * 2, 0.0);
          if (uv < 0) { continue; }
          std::vector<double> &coordinates = Scratch_.Coordinates;
          coordinates.clear();
          if (!document.ReadElements(uv, coordinates)) {
            return Refuse(document.Path() + ": " + set.Semantic +
                          " does not decode: " + document.Error());
          }
          if (coordinates.size() != vertices * 2) {
            return Refuse(document.Path() + ": " + set.Semantic + " decodes to " +
                          std::to_string(coordinates.size() / 2) + " pairs over " +
                          std::to_string(vertices) + " vertices");
          }
          std::ranges::copy(coordinates, set.Into->begin());
        }

        const int colour = primitive.Find("COLOR_0");
        part.HasColour = colour >= 0;
        anyColour = anyColour || part.HasColour;
        atCol.assign(vertices * 4, 0.0);
        if (colour >= 0) {
          if (static_cast<size_t>(colour) >= document.Accessors().size()) {
            return Refuse(document.Path() + ": COLOR_0 names accessor " + std::to_string(colour) +
                          ", which the file does not carry");
          }
          size_t components = 0;
          std::string why;
          if (!VertexColourComponents(
                  document.Accessors()[static_cast<size_t>(colour)], components, why)) {
            return Refuse(document.Path() + ": COLOR_0 " + why);
          }
          std::vector<double> &tints = Scratch_.Tints;
          tints.clear();
          if (!document.ReadElements(colour, tints)) {
            return Refuse(document.Path() + ": COLOR_0 does not decode: " + document.Error());
          }
          if (tints.size() != vertices * components) {
            return Refuse(document.Path() + ": COLOR_0 decodes to " +
                          std::to_string(tints.size() / components) + " colours over " +
                          std::to_string(vertices) + " vertices");
          }
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            for (size_t channel = 0; channel < 4; ++channel) {
              const double value =
                  channel < components ? tints[vertex * components + channel] : 1.0;
              if (!(value >= 0.0) || !(value <= 1.0)) {
                return Refuse(document.Path() + ": COLOR_0 of vertex " + std::to_string(vertex) +
                              " carries " + std::to_string(value) + " in channel " +
                              std::to_string(channel) +
                              ", and the format requires every component in [0, 1]");
              }
              atCol[vertex * 4 + channel] = value;
            }
          }
        }

        const int normal = primitive.Find("NORMAL");
        part.HasNormal = normal >= 0;
        anyNormal = anyNormal || part.HasNormal;
        atNor.assign(vertices * 3, 0.0);
        if (normal >= 0) {
          std::vector<double> &directions = Scratch_.Directions;
          directions.clear();
          if (!document.ReadElements(normal, directions)) {
            return Refuse(document.Path() + ": NORMAL does not decode: " + document.Error());
          }
          if (directions.size() != vertices * 3) {
            return Refuse(document.Path() + ": NORMAL decodes to " +
                          std::to_string(directions.size() / 3) + " vectors over " +
                          std::to_string(vertices) + " vertices");
          }
          std::vector<double> &morphedNormals = Scratch_.MorphedNormals;
          morphedNormals.clear();
          if (!MorphDeltasFor(document,
                              primitive,
                              "NORMAL",
                              nodeWeights.data(),
                              morphCount,
                              3,
                              vertices,
                              morphedNormals)) {
            return false;
          }
          for (size_t at = 0; at < morphedNormals.size(); ++at) {
            directions[at] += morphedNormals[at];
          }
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            const Vec3 local = {
                {directions[vertex * 3], directions[vertex * 3 + 1], directions[vertex * 3 + 2]}};
            Vec3 global;

            if (!place.At(vertex).Normal(local, global)) {
              global[0] = global[1] = global[2] = 0.0;
            }

            (void)Normalise(global);
            for (int axis = 0; axis < 3; ++axis) {
              atNor[vertex * 3 + static_cast<size_t>(axis)] = global[axis];
            }
          }
        }

        if (primitive.Indices >= 0) {
          if (!document.ReadIndices(primitive.Indices, run)) {
            return Refuse(document.Path() +
                          ": the index accessor does not decode: " + document.Error());
          }
        } else {
          run.resize(vertices);
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            run[vertex] = static_cast<uint32_t>(vertex);
          }
        }
        if (!RunIsWhole(primitive.Mode, run.size())) {
          return Refuse(document.Path() + ": " + std::to_string(run.size()) +
                        " indices do not make a whole run of " + ModeName(primitive.Mode));
        }

        Handedness handedness = Handedness::Preserved;
        if (skinned.empty()) {
          handedness = world.LinearDeterminant() < 0 ? Handedness::Reversed : Handedness::Preserved;
        } else {
          const bool mirrored = skinned[0].LinearDeterminant() < 0;
          for (size_t vertex = 1; vertex < skinned.size(); ++vertex) {
            if ((skinned[vertex].LinearDeterminant() < 0) != mirrored) {
              return Refuse(
                  document.Path() + ": vertex " + std::to_string(vertex) +
                  " of a skinned primitive blends to a transform whose determinant has the "
                  "opposite sign to vertex 0's, so the primitive would need two windings");
            }
          }
          handedness = mirrored ? Handedness::Reversed : Handedness::Preserved;
        }
        Triangulate(primitive.Mode, handedness, run, indices);
        for (const uint32_t index : indices) {
          if (index >= vertices) {
            return Refuse(document.Path() + ": index " + std::to_string(index) +
                          " addresses past the " + std::to_string(vertices) +
                          " vertices of its own primitive");
          }
          atIdx.push_back(index);
        }
        part.IndexCount = atIdx.size();
        if (!SuppliedTangentsFor(document,
                                 primitive,
                                 place,
                                 std::span<const double>(nodeWeights.data(), morphCount),
                                 part,
                                 vertices,
                                 atTan)) {
          return false;
        }
        anyTangent = anyTangent || part.HasTangent();
        anyNormal = anyNormal || part.HasNormal;
        part.VertexCount = atPos.size() / 3;

        if (part.IndexCount == 0) { continue; }
        std::vector<float> &narrowed = Scratch_.Narrowed;
        const auto asFloat = [&narrowed](const std::vector<double> &from) {
          narrowed.resize(from.size());
          for (size_t at = 0; at < from.size(); ++at) {
            narrowed[at] = static_cast<float>(from[at]);
          }
          return std::span<const float>(narrowed.data(), narrowed.size());
        };
        const int emitted = made.addPart(part.NodeName, MaterialInstance(part.Material));
        (void)made.setPositions(emitted, asFloat(atPos));
        if (part.HasNormal) { (void)made.setNormals(emitted, asFloat(atNor)); }
        if (part.HasUv) { (void)made.setTexture(emitted, asFloat(atUv), 0); }
        if (part.HasUv1) { (void)made.setTexture(emitted, asFloat(atUv1), 1); }
        if (part.HasTangent()) { (void)made.setTangents(emitted, asFloat(atTan)); }
        if (part.HasColour) { (void)made.setColours(emitted, asFloat(atCol)); }
        (void)made.setTriangles(emitted, std::span<const uint32_t>(atIdx.data(), atIdx.size()));
      }
    }
  }

  if (made.parts() == 0) {
    return Refuse(document.Path() + ": the default scene draws no triangle over " +
                  std::to_string(primitives) + " primitive(s), so there is nothing to render");
  }

  const std::vector<PlacedLight> lit = Lights_;
  const Undrawn missed = Undrawn_;
  {
    const Heap::Tagged assembling("pose-assemble");
    if (!Assemble(made)) { return false; }
  }
  Lights_ = lit;
  Undrawn_ = missed;
  return true;
}

void Subject::Bound() {
  for (int axis = 0; axis < 3; ++axis) {
    Min_[axis] = Max_[axis] = Positions_[static_cast<size_t>(axis)];
  }
  for (size_t vertex = 1; vertex < VertexCount(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const double value = Positions_[vertex * 3 + static_cast<size_t>(axis)];
      Min_[axis] = std::min(value, Min_[axis]);
      Max_[axis] = std::max(value, Max_[axis]);
    }
  }
}

outshine::Geometry Subject::Handed() const {
  return Handed(nullptr);
}

outshine::Geometry Subject::Handed(const Document &naming) const {
  return Handed(&naming);
}

outshine::Geometry Subject::Handed(const Document *naming) const {
  outshine::Geometry out;
  for (size_t at = 0; at < Surfaces_.size(); ++at) {
    const bool named = naming != nullptr && at < naming->Materials().size();
    (void)out.addSurface(named ? naming->Materials()[at].Name : std::string(), Surfaces_[at]);
  }
  for (const PlacedLight &lit : Lights_) {
    Mat4 placed;
    for (int axis = 0; axis < 3; ++axis) { placed[12 + axis] = lit.Light.Position[axis]; }
    (void)out.addLamp(lit.NodeName, lit.Light, placed);
  }
  const auto floats = [](const std::vector<double> &from, size_t first, size_t many) {
    std::vector<float> made(many);
    for (size_t at = 0; at < many; ++at) { made[at] = static_cast<float>(from[first + at]); }
    return made;
  };
  for (const Part &one : Parts_) {
    const int made = out.addPart(one.NodeName, MaterialInstance(one.Material));
    const std::vector<float> positions =
        floats(Positions_, one.FirstVertex * 3, one.VertexCount * 3);
    (void)out.setPositions(made, std::span<const float>(positions.data(), positions.size()));
    if (one.HasNormal && !Normals_.empty()) {
      const std::vector<float> held = floats(Normals_, one.FirstVertex * 3, one.VertexCount * 3);
      (void)out.setNormals(made, std::span<const float>(held.data(), held.size()));
    }
    if (one.HasUv && !Uv_.empty()) {
      const std::vector<float> held = floats(Uv_, one.FirstVertex * 2, one.VertexCount * 2);
      (void)out.setTexture(made, std::span<const float>(held.data(), held.size()), 0);
    }
    if (one.HasUv1 && !Uv1_.empty()) {
      const std::vector<float> held = floats(Uv1_, one.FirstVertex * 2, one.VertexCount * 2);
      (void)out.setTexture(made, std::span<const float>(held.data(), held.size()), 1);
    }
    if (one.HasTangent() && !Tangents_.empty()) {
      const std::vector<float> held = floats(Tangents_, one.FirstVertex * 4, one.VertexCount * 4);
      (void)out.setTangents(made, std::span<const float>(held.data(), held.size()));
    }
    if (one.HasColour && !Colours_.empty()) {
      const std::vector<float> held = floats(Colours_, one.FirstVertex * 4, one.VertexCount * 4);
      (void)out.setColours(made, std::span<const float>(held.data(), held.size()));
    }
    std::vector<uint32_t> run(one.IndexCount);
    for (size_t at = 0; at < one.IndexCount; ++at) {
      run[at] = static_cast<uint32_t>(Indices_[one.FirstIndex + at] - one.FirstVertex);
    }
    (void)out.setTriangles(made, std::span<const uint32_t>(run.data(), run.size()));
  }
  return out;
}

bool Subject::Assemble(const outshine::Geometry &what) {
  Error_.clear();
  Positions_.clear();
  Uv_.clear();
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  Lights_.clear();
  Surfaces_.clear();
  TangentWanted_.clear();

  size_t wholeFloats = 0;
  for (int counting = 0; counting < what.parts(); ++counting) {
    wholeFloats += what.positionsOf(counting).size();
  }
  for (int surface = 0; surface < what.surfaces(); ++surface) {
    Surfaces_.push_back(what.surfaceAt(MaterialInstance(surface)));
    TangentWanted_.push_back(Surfaces_.back().NeedsTangents ? 1u : 0u);
  }
  for (int lamp = 0; lamp < what.lamps(); ++lamp) {
    PlacedLight placed;
    placed.NodeName = std::string(what.lampNameOf(lamp));
    placed.LightName = placed.NodeName;
    placed.Light = what.lampAt(lamp);
    const Mat4 &at = what.lampPlacementOf(lamp);
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Position[axis] = static_cast<float>(at[12 + axis]);
    }
    Lights_.push_back(std::move(placed));
  }
  if (what.parts() == 0) {
    return Refuse(
        "an assembly of no piece draws nothing, and a subject with no triangle is not one");
  }

  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;
  for (size_t index = 0; std::cmp_less(index, what.parts()); ++index) {
    const int slot = static_cast<int>(index);
    const std::span<const float> pPos = what.positionsOf(slot);
    const std::span<const float> pNormals = what.normalsOf(slot);
    const std::span<const float> pUv = what.textureOf(slot, 0);
    const std::span<const float> pUv1 = what.textureOf(slot, 1);
    const std::span<const float> pTangents = what.tangentsOf(slot);
    const std::span<const float> pColours = what.coloursOf(slot);
    const std::span<const uint32_t> pIndices = what.trianglesOf(slot);
    const std::string where = "assembled piece " + std::to_string(index);
    if (pPos.empty() || (pPos.size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(pPos.size()) +
                    " position components, which is not a whole run of points");
    }
    const size_t vertices = pPos.size() / 3;
    if (pIndices.empty() || (pIndices.size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(pIndices.size()) +
                    " indices, which is not a whole run of triangles");
    }
    if (what.materialOf(slot).index() < -1) {
      return Refuse(where + " names material " + std::to_string(what.materialOf(slot).index()) +
                    ", and -1 is the only spelling of naming none");
    }
    std::string why;
    if (!RunIsStatable(pPos, vertices, 3, "positions", where, why) ||
        !RunIsStatable(pNormals, vertices, 3, "normals", where, why) ||
        !RunIsStatable(pUv, vertices, 2, "uv pairs", where, why) ||
        !RunIsStatable(pUv1, vertices, 2, "second-set uv pairs", where, why) ||
        !RunIsStatable(pTangents, vertices, 4, "tangents", where, why) ||
        !RunIsStatable(pColours, vertices, 4, "vertex colours", where, why)) {
      return Refuse(why);
    }

    for (size_t at = 0; at < pColours.size(); ++at) {
      if (pColours[at] >= 0.0f && pColours[at] <= 1.0f) { continue; }
      return Refuse(where + " states a vertex colour component of " + std::to_string(pColours[at]) +
                    " at " + std::to_string(at) +
                    ", and the format requires every component in [0, 1]");
    }

    Part part;
    part.NodeName = std::string(what.nameOf(slot));
    part.Material = what.materialOf(slot).index();
    part.FirstVertex = VertexCount();
    part.FirstIndex = Indices_.size();
    part.VertexCount = vertices;
    part.IndexCount = pIndices.size();
    part.HasUv = !pUv.empty();
    part.HasUv1 = !pUv1.empty();
    part.HasNormal = !pNormals.empty();
    part.HasColour = !pColours.empty();

    part.Tangent = pTangents.empty() ? TangentSource::None : TangentSource::Supplied;
    anyUv = anyUv || part.HasUv;
    anyUv1 = anyUv1 || part.HasUv1;
    anyNormal = anyNormal || part.HasNormal;
    anyTangent = anyTangent || part.HasTangent();
    anyColour = anyColour || part.HasColour;

    if (Positions_.capacity() < Positions_.size() + pPos.size()) {
      Positions_.reserve(wholeFloats);
      Uv_.reserve((wholeFloats / 3) * 2);
      Uv1_.reserve((wholeFloats / 3) * 2);
      Normals_.reserve(wholeFloats);
      Tangents_.reserve((wholeFloats / 3) * 4);
      Colours_.reserve((wholeFloats / 3) * 4);
    }
    for (const float component : pPos) { Positions_.push_back(static_cast<double>(component)); }

    Uv_.resize((Positions_.size() / 3) * 2, 0.0);
    Uv1_.resize((Positions_.size() / 3) * 2, 0.0);
    Normals_.resize(Positions_.size(), 0.0);
    Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
    Colours_.resize((Positions_.size() / 3) * 4, 0.0);
    for (size_t at = 0; at < pUv.size(); ++at) {
      Uv_[part.FirstVertex * 2 + at] = static_cast<double>(pUv[at]);
    }
    for (size_t at = 0; at < pUv1.size(); ++at) {
      Uv1_[part.FirstVertex * 2 + at] = static_cast<double>(pUv1[at]);
    }
    for (size_t at = 0; at < pNormals.size(); ++at) {
      Normals_[part.FirstVertex * 3 + at] = static_cast<double>(pNormals[at]);
    }
    for (size_t at = 0; at < pTangents.size(); ++at) {
      Tangents_[part.FirstVertex * 4 + at] = static_cast<double>(pTangents[at]);
    }
    for (size_t at = 0; at < pColours.size(); ++at) {
      Colours_[part.FirstVertex * 4 + at] = static_cast<double>(pColours[at]);
    }

    for (const uint32_t local : pIndices) {
      if (local >= vertices) {
        return Refuse(where + " addresses vertex " + std::to_string(local) + " of its own " +
                      std::to_string(vertices));
      }
      Indices_.push_back(static_cast<uint32_t>(part.FirstVertex) + local);
    }
    part.IndexCount = Indices_.size() - part.FirstIndex;
    if (!FlatNormalsFor(part) || !GeneratedTangentsFor(part)) { return false; }
    part.VertexCount = VertexCount() - part.FirstVertex;
    anyNormal = anyNormal || part.HasNormal;
    Parts_.push_back(part);
  }

  Bound();
  return true;
}

bool Subject::Append(const Subject &other) {
  if (other.Parts_.empty()) {
    return Refuse(
        "a subject with no part appends nothing, and an empty append is a caller's mistake "
        "rather than a shape this can carry");
  }
  const size_t vertexBase = VertexCount();
  const size_t indexBase = Indices_.size();
  const size_t vertexTotal = vertexBase + other.VertexCount();

  const auto join = [vertexBase, vertexTotal](std::vector<double> &mine,
                                              const std::vector<double> &theirs,
                                              size_t stride) {
    if (mine.empty() && theirs.empty()) { return; }
    mine.resize(vertexTotal * stride, 0.0);
    for (size_t at = 0; at < theirs.size(); ++at) { mine[vertexBase * stride + at] = theirs[at]; }
  };
  Positions_.insert(Positions_.end(), other.Positions_.begin(), other.Positions_.end());
  join(Uv_, other.Uv_, 2);
  join(Uv1_, other.Uv1_, 2);
  join(Normals_, other.Normals_, 3);
  join(Tangents_, other.Tangents_, 4);
  join(Colours_, other.Colours_, 4);

  Indices_.reserve(Indices_.size() + other.Indices_.size());
  for (const uint32_t index : other.Indices_) {
    Indices_.push_back(static_cast<uint32_t>(vertexBase) + index);
  }
  int beyond = 0;
  for (const Part &part : Parts_) {
    if (part.Material >= beyond) { beyond = part.Material + 1; }
  }
  Parts_.reserve(Parts_.size() + other.Parts_.size());
  for (Part part : other.Parts_) {
    part.FirstVertex += vertexBase;
    part.FirstIndex += indexBase;
    if (part.Material >= 0) { part.Material += beyond; }
    Parts_.push_back(std::move(part));
  }
  Bound();
  return true;
}

void Subject::BoundsOf(size_t parts, Vec3 &least, Vec3 &most) const {
  for (int axis = 0; axis < 3; ++axis) {
    least[axis] = Min_[axis];
    most[axis] = Max_[axis];
  }
  if (parts == 0 || parts >= Parts_.size()) { return; }
  bool any = false;
  for (size_t at = 0; at < parts; ++at) {
    const Part &part = Parts_[at];
    for (size_t vertex = part.FirstVertex; vertex < part.FirstVertex + part.VertexCount; ++vertex) {
      for (int axis = 0; axis < 3; ++axis) {
        const double held = Positions_[vertex * 3 + static_cast<size_t>(axis)];
        if (!any || held < least[axis]) { least[axis] = held; }
        if (!any || held > most[axis]) { most[axis] = held; }
      }
      any = true;
    }
  }
}

double Subject::RadiusM() const {
  const Vec3 span = Max_ - Min_;
  return 0.5 * Length(span);
}

void Subject::CentreM(Vec3 &out) const {
  out = (Min_ + Max_) * 0.5;
}

bool Subject::Frame(Viewpoint &out, double fill) const {
  return FramingFor(Min_, Max_, out, fill);
}

bool FramingFor(const Vec3 &minM, const Vec3 &maxM, Viewpoint &out, double fill) {
  const Vec3 span = {{maxM[0] - minM[0], maxM[1] - minM[1], maxM[2] - minM[2]}};
  const double radius = 0.5 * Length(span);
  if (!(radius > 0)) { return false; }
  Vec3 centre;
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (minM[axis] + maxM[axis]); }

  const double azimuth = Render::kFramingAzimuthDeg * kDeg2Rad;
  const double elevation = Render::kFramingElevationDeg * kDeg2Rad;

  const Vec3 toEye = {{std::cos(elevation) * std::cos(azimuth),
                       std::sin(elevation),
                       std::cos(elevation) * std::sin(azimuth)}};

  const double yfov =
      2.0 * std::atan(Render::kFramingSensorHalfHeightMm / Render::kFramingFocalLengthMm);
  const double distance = radius / std::sin(0.5 * yfov) / (fill > 0 ? fill : Render::kFramingFill);
  Vec3 eye;
  for (int axis = 0; axis < 3; ++axis) { eye[axis] = centre[axis] + toEye[axis] * distance; }

  if (!Viewpoint::LookAt(eye, centre, 0.0, out)) { return false; }
  out.YfovRad = yfov;
  const double floor = radius * Render::kFramingNearFloorFraction;
  out.ZNearM = (distance - radius > floor) ? distance - radius : floor;
  out.ZFarM = distance + radius;
  return true;
}

bool DeclaredPlacement(const Document &document,
                       int cameraIndex,
                       Viewpoint &out,
                       std::string &error) {
  if (cameraIndex < 0 || static_cast<size_t>(cameraIndex) >= document.Cameras().size()) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is asked for and the " +
            "document declares " + std::to_string(document.Cameras().size());
    return false;
  }
  size_t holder = 0;
  size_t holders = 0;
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    if (document.Nodes()[node].Camera != cameraIndex) { continue; }
    holder = node;
    ++holders;
  }
  if (holders != 1) {
    error = document.Path() + ": camera " + std::to_string(cameraIndex) + " is referenced by " +
            std::to_string(holders) + " nodes, and a placement is what exactly one node states";
    return false;
  }

  const Camera &lens = document.Cameras()[static_cast<size_t>(cameraIndex)];
  Transform world;
  if (!document.WorldTransform(static_cast<int>(holder), world)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its world transform does not resolve";
    return false;
  }
  for (int axis = 0; axis < 3; ++axis) {
    out.Right[axis] = world.M[axis];
    out.Up[axis] = world.M[4 + axis];
    out.Forward[axis] = -world.M[8 + axis];
    out.EyeM[axis] = world.M[12 + axis];
  }
  if (!Normalise(out.Right) || !Normalise(out.Up) || !Normalise(out.Forward)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its basis has collapsed";
    return false;
  }
  out.Kind = lens.Kind == CameraKind::Orthographic ? Render::CameraKind::Orthographic
                                                   : Render::CameraKind::Perspective;
  out.YfovRad = lens.YfovRad;
  out.XMagM = lens.XMagM;
  out.YMagM = lens.YMagM;
  out.ZNearM = lens.ZNearM;
  out.ZFarM = lens.ZFarM;
  return true;
}

double Subject::ProjectedAreaPx(const Transform &clip, const Viewport &viewport) const {
  double total = 0;
  for (size_t triangle = 0; triangle * 3 + 2 < Indices_.size(); ++triangle) {
    std::array<Vec2, 3> raster;
    for (int corner = 0; corner < 3; ++corner) {
      const size_t vertex = Indices_[triangle * 3 + static_cast<size_t>(corner)];
      const Vec3 point = {
          {Positions_[vertex * 3], Positions_[vertex * 3 + 1], Positions_[vertex * 3 + 2]}};
      Vec3 ndc;
      clip.Point(point, ndc);
      viewport.Raster(ndc, raster[corner]);
    }
    total += 0.5 * std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                             (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1]));
  }
  return total;
}

} // namespace outshine::Gltf
