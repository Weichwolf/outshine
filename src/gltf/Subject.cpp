#include "Subject.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <string>

#include "Document.h"
#include "Framing.h"
#include "Tangents.h"

namespace outshine::Gltf {

namespace {

constexpr double kPi = 3.14159265358979323846;

void Cross(const double a[3], const double b[3], double out[3]) {
  out[0] = a[1] * b[2] - a[2] * b[1];
  out[1] = a[2] * b[0] - a[0] * b[2];
  out[2] = a[0] * b[1] - a[1] * b[0];
}

double Length(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

[[nodiscard]] bool Normalise(double v[3]) {
  const double length = Length(v);
  if (!(length > 0)) { return false; }
  for (int axis = 0; axis < 3; ++axis) { v[axis] /= length; }
  return true;
}

[[nodiscard]] bool RunIsStatable(Span<const float> run, size_t vertices, size_t components,
                                 const char *semantic, const std::string &where, std::string &why) {
  if (run.Empty()) { return true; }
  if (run.Size() != vertices * components) {
    why = where + " states " + std::to_string(run.Size() / components) + " " + semantic + " over " +
          std::to_string(vertices) + " vertices";
    return false;
  }
  for (size_t at = 0; at < run.Size(); ++at) {
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

void Triangulate(PrimitiveMode mode, Handedness handedness, const std::vector<uint32_t> &run,
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
  uint64_t Bits[4] = {};
  bool operator<(const BasisKey &other) const {
    return std::memcmp(Bits, other.Bits, sizeof Bits) < 0;
  }
};

BasisKey KeyOf(double x, double y, double z, double w) {
  const double basis[4] = {x, y, z, w};
  BasisKey key;
  for (size_t at = 0; at < 4; ++at) {
    const double folded = basis[at] == 0.0 ? 0.0 : basis[at];
    std::memcpy(&key.Bits[at], &folded, sizeof key.Bits[at]);
  }
  return key;
}

}

bool Subject::MorphDeltasFor(const Document &document, const Primitive &primitive,
                             const char *semantic, const double *weights, size_t count,
                             size_t components, size_t vertices, std::vector<double> &out) {
  out.clear();
  if (count == 0 || primitive.Targets.empty()) { return true; }
  std::vector<double> delta;
  for (size_t target = 0; target < count && target < primitive.Targets.size(); ++target) {
    const double share = weights[target];
    if (share == 0.0) { continue; }
    const int accessor = primitive.Targets[target].Find(semantic);
    if (accessor < 0) { continue; }
    if (!document.ReadElements(accessor, delta)) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " + semantic +
                    " does not decode: " + document.Error());
    }
    if (delta.size() != vertices * components) {
      return Refuse(document.Path() + ": morph target " + std::to_string(target) + "'s " + semantic +
                    " decodes to " + std::to_string(delta.size()) + " components over " +
                    std::to_string(vertices) + " vertices of " + std::to_string(components));
    }
    if (out.empty()) { out.assign(vertices * components, 0.0); }
    for (size_t at = 0; at < delta.size(); ++at) { out[at] += share * delta[at]; }
  }
  return true;
}

Transform Subject::JointMatrix(const Skin &skin, size_t joint, const Transform &world) {
  if (skin.InverseBind.empty()) { return world; }
  return world * Transform::FromColumnMajor(&skin.InverseBind[joint * 16]);
}

bool Subject::BlendSkinFor(const Document &document, const Skin &skin,
                           const std::vector<Transform> &joints, const Primitive &primitive,
                           size_t vertices, std::vector<Transform> &out) {
  std::vector<double> index;
  std::vector<double> weight;
  size_t sets = 0;
  for (;; ++sets) {
    const std::string which = std::to_string(sets);
    const int bones = primitive.Find(("JOINTS_" + which).c_str());
    const int weights = primitive.Find(("WEIGHTS_" + which).c_str());
    if (bones < 0 && weights < 0) { break; }
    if (bones < 0 || weights < 0) {
      return Refuse(document.Path() + ": a primitive on a skinned node carries " +
                    std::string(bones < 0 ? "no JOINTS_" : "JOINTS_") + which + " and " +
                    std::string(weights < 0 ? "no WEIGHTS_" : "WEIGHTS_") + which +
                    ", and a set without both binds no vertex to any joint");
    }
    std::vector<double> theseIndices;
    std::vector<double> theseWeights;
    if (!document.ReadElements(bones, theseIndices)) {
      return Refuse(document.Path() + ": JOINTS_" + which + " does not decode: " + document.Error());
    }
    if (!document.ReadElements(weights, theseWeights)) {
      return Refuse(document.Path() + ": WEIGHTS_" + which + " does not decode: " + document.Error());
    }
    if (theseIndices.size() != vertices * 4 || theseWeights.size() != vertices * 4) {
      return Refuse(document.Path() + ": JOINTS_" + which + " decodes to " +
                    std::to_string(theseIndices.size() / 4) + " and WEIGHTS_" + which + " to " +
                    std::to_string(theseWeights.size() / 4) + " sets over " +
                    std::to_string(vertices) + " vertices, and glTF states both are VEC4");
    }
    index.insert(index.end(), theseIndices.begin(), theseIndices.end());
    weight.insert(weight.end(), theseWeights.begin(), theseWeights.end());
  }
  if (sets == 0) {
    return Refuse(document.Path() +
                  ": a primitive on a skinned node carries no JOINTS_0 and no WEIGHTS_0, and a skin "
                  "without both binds no vertex to any joint");
  }

  out.assign(vertices, Transform());
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    double blended[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    double sum = 0;
    for (size_t set = 0; set < sets; ++set) {
      const size_t base = set * vertices * 4 + vertex * 4;
      for (size_t slot = 0; slot < 4; ++slot) {
        const double share = weight[base + slot];
        if (share == 0.0) { continue; }
        const double named = index[base + slot];
        if (!(named >= 0.0) || (size_t)named >= joints.size()) {
          return Refuse(document.Path() + ": JOINTS_" + std::to_string(set) + " of vertex " +
                        std::to_string(vertex) + " names joint " +
                        std::to_string((long long)named) + " and the skin declares " +
                        std::to_string(joints.size()));
        }
        sum += share;
        const Transform &matrix = joints[(size_t)named];
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

bool Subject::BuildTangentsFor(const Document &document, const Primitive &primitive,
                               const VertexPlacement &place, Span<const double> morphWeights,
                               Part &part, size_t vertices) {
  Tangents_.resize((Positions_.size() / 3) * 4, 0.0);

  const int supplied = primitive.Find("TANGENT");
  if (supplied >= 0) {
    std::vector<double> elements;
    if (!document.ReadElements(supplied, elements)) {
      return Refuse(document.Path() + ": TANGENT does not decode: " + document.Error());
    }
    if (elements.size() != vertices * 4) {
      return Refuse(document.Path() + ": TANGENT decodes to " + std::to_string(elements.size() / 4) +
                    " vectors over " + std::to_string(vertices) + " vertices");
    }

    std::vector<double> morphedTangents;
    if (!MorphDeltasFor(document, primitive, "TANGENT", morphWeights.Data(), morphWeights.Size(), 3,
                        vertices, morphedTangents)) {
      return false;
    }
    for (size_t vertex = 0; vertex < vertices && !morphedTangents.empty(); ++vertex) {
      for (size_t axis = 0; axis < 3; ++axis) {
        elements[vertex * 4 + axis] += morphedTangents[vertex * 3 + axis];
      }
    }

    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const Transform &placed = place.At(vertex);
      const double mirrored = placed.LinearDeterminant() < 0 ? -1.0 : 1.0;
      const double local[3] = {elements[vertex * 4], elements[vertex * 4 + 1],
                               elements[vertex * 4 + 2]};
      double global[3];
      placed.Direction(local, global);
      (void)Normalise(global);
      for (int axis = 0; axis < 3; ++axis) {
        Tangents_[(part.FirstVertex + vertex) * 4 + (size_t)axis] = global[axis];
      }
      Tangents_[(part.FirstVertex + vertex) * 4 + 3] = elements[vertex * 4 + 3] * mirrored;
    }
    part.Tangent = TangentSource::Supplied;
    return true;
  }

  const bool needed = part.Material >= 0 && (size_t)part.Material < document.Materials().size() &&
                      document.Materials()[(size_t)part.Material].Normal.Texture >= 0;
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
    return Refuse(document.Path() + ": the tangent basis the material needs cannot be generated: " +
                  error);
  }

  std::map<BasisKey, uint32_t> split;
  std::vector<char> written(VertexCount(), 0);
  for (size_t corner = 0; corner < part.IndexCount; ++corner) {
    const uint32_t vertex = Indices_[part.FirstIndex + corner];
    const double *basis = &corners[corner * 4];
    if (!written[vertex]) {
      for (size_t at = 0; at < 4; ++at) { Tangents_[vertex * 4 + at] = basis[at]; }
      written[vertex] = 1;
      continue;
    }
    if (std::memcmp(&Tangents_[vertex * 4], basis, 4 * sizeof(double)) == 0) { continue; }
    BasisKey key = KeyOf(basis[0], basis[1], basis[2], basis[3]);

    key.Bits[0] ^= (uint64_t)vertex * 0x9e3779b97f4a7c15ull;
    const auto found = split.find(key);
    if (found != split.end()) {
      Indices_[part.FirstIndex + corner] = found->second;
      continue;
    }
    const uint32_t made = (uint32_t)VertexCount();
    for (size_t axis = 0; axis < 3; ++axis) {
      Positions_.push_back(Positions_[vertex * 3 + axis]);
    }
    Uv_.resize((size_t)made * 2 + 2, 0.0);
    Uv_[(size_t)made * 2] = Uv_[vertex * 2];
    Uv_[(size_t)made * 2 + 1] = Uv_[vertex * 2 + 1];

    if (!Uv1_.empty()) {
      Uv1_.resize((size_t)made * 2 + 2, 0.0);
      Uv1_[(size_t)made * 2] = Uv1_[vertex * 2];
      Uv1_[(size_t)made * 2 + 1] = Uv1_[vertex * 2 + 1];
    }

    if (!Colours_.empty()) {
      Colours_.resize((size_t)made * 4 + 4, 0.0);
      for (size_t channel = 0; channel < 4; ++channel) {
        Colours_[(size_t)made * 4 + channel] = Colours_[vertex * 4 + channel];
      }
    }
    Normals_.resize((size_t)made * 3 + 3, 0.0);
    for (size_t axis = 0; axis < 3; ++axis) {
      Normals_[(size_t)made * 3 + axis] = Normals_[vertex * 3 + axis];
    }
    Tangents_.resize((size_t)made * 4 + 4, 0.0);
    for (size_t at = 0; at < 4; ++at) { Tangents_[(size_t)made * 4 + at] = basis[at]; }
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
    uint32_t of[3] = {Indices_[part.FirstIndex + triangle],
                      Indices_[part.FirstIndex + triangle + 1],
                      Indices_[part.FirstIndex + triangle + 2]};
    for (size_t corner = 0; corner < 3; ++corner) {
      const uint32_t vertex = of[corner];
      if (vertex < before && !owned[vertex]) {
        owned[vertex] = 1;
        continue;
      }

      const uint32_t made = (uint32_t)VertexCount();
      for (size_t axis = 0; axis < 3; ++axis) {
        Positions_.push_back(Positions_[(size_t)vertex * 3 + axis]);
      }
      Normals_.resize((size_t)made * 3 + 3, 0.0);
      if (!Uv_.empty()) {
        Uv_.resize((size_t)made * 2 + 2, 0.0);
        Uv_[(size_t)made * 2] = Uv_[(size_t)vertex * 2];
        Uv_[(size_t)made * 2 + 1] = Uv_[(size_t)vertex * 2 + 1];
      }
      if (!Uv1_.empty()) {
        Uv1_.resize((size_t)made * 2 + 2, 0.0);
        Uv1_[(size_t)made * 2] = Uv1_[(size_t)vertex * 2];
        Uv1_[(size_t)made * 2 + 1] = Uv1_[(size_t)vertex * 2 + 1];
      }
      if (!Colours_.empty()) {
        Colours_.resize((size_t)made * 4 + 4, 0.0);
        for (size_t channel = 0; channel < 4; ++channel) {
          Colours_[(size_t)made * 4 + channel] = Colours_[(size_t)vertex * 4 + channel];
        }
      }
      if (!Tangents_.empty()) { Tangents_.resize((size_t)made * 4 + 4, 0.0); }
      of[corner] = made;
      Indices_[part.FirstIndex + triangle + corner] = made;
    }

    double edge[2][3];
    for (size_t axis = 0; axis < 3; ++axis) {
      edge[0][axis] = Positions_[(size_t)of[1] * 3 + axis] - Positions_[(size_t)of[0] * 3 + axis];
      edge[1][axis] = Positions_[(size_t)of[2] * 3 + axis] - Positions_[(size_t)of[0] * 3 + axis];
    }
    double face[3] = {edge[0][1] * edge[1][2] - edge[0][2] * edge[1][1],
                      edge[0][2] * edge[1][0] - edge[0][0] * edge[1][2],
                      edge[0][0] * edge[1][1] - edge[0][1] * edge[1][0]};
    const double length = std::sqrt(face[0] * face[0] + face[1] * face[1] + face[2] * face[2]);

    if (length > 0.0) {
      for (size_t axis = 0; axis < 3; ++axis) { face[axis] /= length; }
    } else {
      face[0] = face[1] = face[2] = 0.0;
    }
    for (size_t corner = 0; corner < 3; ++corner) {
      for (size_t axis = 0; axis < 3; ++axis) { Normals_[(size_t)of[corner] * 3 + axis] = face[axis]; }
    }
  }
  part.HasNormal = true;
  part.VertexCount += VertexCount() - before;
  return true;
}

bool Placement::LookAt(const double eyeM[3], const double aimM[3], double rollRad, Placement &out) {
  double forward[3] = {aimM[0] - eyeM[0], aimM[1] - eyeM[1], aimM[2] - eyeM[2]};
  if (!Normalise(forward)) { return false; }
  const double worldUp[3] = {0, 1, 0};
  double right[3];
  Cross(forward, worldUp, right);
  if (!Normalise(right)) { return false; }
  double up[3];
  Cross(right, forward, up);

  const double turn = std::cos(rollRad);
  const double lean = std::sin(rollRad);
  for (int axis = 0; axis < 3; ++axis) {
    out.EyeM[axis] = eyeM[axis];
    out.Forward[axis] = forward[axis];
    out.Right[axis] = right[axis] * turn + up[axis] * lean;
    out.Up[axis] = up[axis] * turn - right[axis] * lean;
  }
  return true;
}

bool Placement::View(Transform &out) const {
  Transform world;
  for (int axis = 0; axis < 3; ++axis) {
    world.M[axis] = Right[axis];
    world.M[4 + axis] = Up[axis];
    world.M[8 + axis] = -Forward[axis];
    world.M[12 + axis] = EyeM[axis];
  }
  world.M[3] = world.M[7] = world.M[11] = 0;
  world.M[15] = 1;
  return world.Inverse(out);
}

bool Placement::Clip(double viewportAspect, Transform &out) const {
  Camera lens;
  lens.Kind = Kind;
  lens.YfovRad = YfovRad;
  lens.XMagM = XMagM;
  lens.YMagM = YMagM;
  lens.ZNearM = ZNearM;
  lens.ZFarM = ZFarM;
  Transform projection, view;
  if (!lens.Projection(viewportAspect, projection)) { return false; }
  if (!View(view)) { return false; }
  out = projection * view;
  return true;
}

bool Subject::Refuse(const std::string &why) {
  Error_ = why;
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

bool Subject::Build(const Document &document, Span<const Transform> pose,
                    Span<const double> weights, const VariantSelection &variant) {
  if (pose.Size() != document.Nodes().size()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(pose.Size()) +
                  " local transforms and the file carries " +
                  std::to_string(document.Nodes().size()) + " nodes");
  }

  if (weights.Size() != 0 && weights.Size() != document.MorphWeightsTotal()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(weights.Size()) +
                  " morph weights and the file's nodes carry " +
                  std::to_string(document.MorphWeightsTotal()));
  }
  return Flatten(document, pose.Data(), weights.Size() ? weights.Data() : nullptr, variant);
}

bool InstanceTransforms(const Document &document, const Node &node, const Transform &world,
                        std::vector<Transform> &out) {
  out.clear();
  const int named[3] = {node.InstanceTranslation, node.InstanceRotation, node.InstanceScale};
  bool any = false;
  for (const int accessor : named) { any = any || accessor >= 0; }
  if (!any) {
    out.push_back(world);
    return true;
  }
  std::vector<double> translation, rotation, scale;
  if (node.InstanceTranslation >= 0 &&
      !document.ReadElements(node.InstanceTranslation, translation)) {
    return false;
  }
  if (node.InstanceRotation >= 0 && !document.ReadElements(node.InstanceRotation, rotation)) {
    return false;
  }
  if (node.InstanceScale >= 0 && !document.ReadElements(node.InstanceScale, scale)) { return false; }
  size_t count = 0;
  if (!translation.empty()) { count = translation.size() / 3; }
  else if (!rotation.empty()) { count = rotation.size() / 4; }
  else if (!scale.empty()) { count = scale.size() / 3; }
  out.reserve(count);
  for (size_t at = 0; at < count; ++at) {
    const double t[3] = {translation.empty() ? 0.0 : translation[at * 3 + 0],
                         translation.empty() ? 0.0 : translation[at * 3 + 1],
                         translation.empty() ? 0.0 : translation[at * 3 + 2]};
    const double r[4] = {rotation.empty() ? 0.0 : rotation[at * 4 + 0],
                         rotation.empty() ? 0.0 : rotation[at * 4 + 1],
                         rotation.empty() ? 0.0 : rotation[at * 4 + 2],
                         rotation.empty() ? 1.0 : rotation[at * 4 + 3]};
    const double sc[3] = {scale.empty() ? 1.0 : scale[at * 3 + 0],
                          scale.empty() ? 1.0 : scale[at * 3 + 1],
                          scale.empty() ? 1.0 : scale[at * 3 + 2]};
    out.push_back(world * Transform::FromTrs(t, r, sc));
  }
  return true;
}

bool Subject::Flatten(const Document &document, const Transform *pose, const double *weights,
                      const VariantSelection &variant) {

  const auto placementOf = [&document, pose](int node, Transform &out) {
    return pose ? document.WorldTransform(node, Span<const Transform>(pose, document.Nodes().size()),
                                          out)
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
  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;

  const int sceneIndex = document.DefaultScene();
  if (sceneIndex < 0 || (size_t)sceneIndex >= document.Scenes().size()) {
    return Refuse(document.Path() + ": no default scene to draw");
  }

  int activeVariant = -1;
  {
    std::string why;
    if (!variant.Against(document, activeVariant, why)) {
      return Refuse(document.Path() + ": the declaration " + why);
    }
  }

  std::vector<int> pending(document.Scenes()[(size_t)sceneIndex].Roots.rbegin(),
                           document.Scenes()[(size_t)sceneIndex].Roots.rend());

  std::vector<double> elements;
  std::vector<uint32_t> run, indices;
  size_t primitives = 0;
  while (!pending.empty()) {
    const int nodeIndex = pending.back();
    pending.pop_back();
    if (nodeIndex < 0 || (size_t)nodeIndex >= document.Nodes().size()) {
      return Refuse(document.Path() + ": scene names node " + std::to_string(nodeIndex) +
                    ", which the file does not carry");
    }
    const Node &node = document.Nodes()[(size_t)nodeIndex];
    if (!node.Visible) { continue; }
    for (auto child = node.Children.rbegin(); child != node.Children.rend(); ++child) {
      pending.push_back(*child);
    }
    if (node.Light >= 0) {
      Transform placement;
      if (!placementOf(nodeIndex, placement)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and has no world transform: " + document.Error());
      }
      const LightRef &declared = document.Lights()[(size_t)node.Light];
      PlacedLight placed;
      placed.NodeName = node.Name;
      placed.LightName = declared.Name;
      placed.Light = declared.Light;
      const double origin[3] = {0, 0, 0};
      double position[3];
      placement.Point(origin, position);

      const double axis[3] = {0, 0, -1};
      double beam[3];
      placement.Direction(axis, beam);
      if (!Normalise(beam)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and its transform collapses the beam to zero length");
      }
      for (int component = 0; component < 3; ++component) {
        placed.Light.Position[component] = (float)position[component];
        placed.Light.Direction[component] = (float)beam[component];
      }
      Lights_.push_back(std::move(placed));
    }
    if (node.Mesh < 0) { continue; }
    if ((size_t)node.Mesh >= document.Meshes().size()) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) + " names mesh " +
                    std::to_string(node.Mesh) + ", which the file does not carry");
    }
    Transform world;
    if (!placementOf(nodeIndex, world)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " has no world transform: " + document.Error());
    }

    const size_t morphCount = document.MorphWeightsCount(nodeIndex);
    std::vector<double> nodeWeights;
    if (morphCount > 0) {
      const std::vector<double> &declared = document.Meshes()[(size_t)node.Mesh].Weights;
      for (size_t at = 0; at < morphCount; ++at) {
        nodeWeights.push_back(weights ? weights[document.MorphWeightsFirst(nodeIndex) + at]
                                      : (at < declared.size() ? declared[at] : 0.0));
      }
    }

    std::vector<Transform> jointMatrices;
    if (node.Skin >= 0) {
      const Skin &skin = document.Skins()[(size_t)node.Skin];
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

    std::vector<Transform> instances;
    if (!InstanceTransforms(document, node, world, instances)) {
      return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                    " instances on an accessor this reader cannot decode: " + document.Error());
    }
    for (const Transform &world : instances) {
      for (const Primitive &primitive : document.Meshes()[(size_t)node.Mesh].Primitives) {
        ++primitives;
        Part part;
        part.NodeName = node.Name;
        part.Material = primitive.MaterialUnder(activeVariant);
        part.FirstVertex = VertexCount();
        part.FirstIndex = Indices_.size();

        if (!DrawsASurface(primitive.Mode)) {
          ++Undrawn_.Primitives;
          const size_t mode = (size_t)primitive.Mode;
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
          return Refuse(document.Path() + ": POSITION decodes to " + std::to_string(elements.size()) +
                        " components, which is not a whole number of points");
        }
        const uint32_t base = (uint32_t)(Positions_.size() / 3);
        const size_t vertices = elements.size() / 3;

        std::vector<double> morphedPositions;
        if (!MorphDeltasFor(document, primitive, "POSITION", nodeWeights.data(), morphCount, 3,
                            vertices, morphedPositions)) {
          return false;
        }
        for (size_t at = 0; at < morphedPositions.size(); ++at) { elements[at] += morphedPositions[at]; }
        std::vector<Transform> skinned;
        if (node.Skin >= 0 &&
            !BlendSkinFor(document, document.Skins()[(size_t)node.Skin], jointMatrices, primitive,
                          vertices, skinned)) {
          return false;
        }
        const VertexPlacement place{world, skinned.empty() ? nullptr : skinned.data()};
        for (size_t vertex = 0; vertex < vertices; ++vertex) {
          double local[3] = {elements[vertex * 3], elements[vertex * 3 + 1],
                             elements[vertex * 3 + 2]};
          double global[3];
          place.At(vertex).Point(local, global);
          for (int axis = 0; axis < 3; ++axis) { Positions_.push_back(global[axis]); }
        }

        const struct {
          const char *Semantic;
          bool Part::*Carried;
          bool *Any;
          std::vector<double> *Into;
        } sets[kUvSets] = {{"TEXCOORD_0", &Part::HasUv, &anyUv, &Uv_},
                           {"TEXCOORD_1", &Part::HasUv1, &anyUv1, &Uv1_}};
        for (const auto &set : sets) {
          const int uv = primitive.Find(set.Semantic);
          part.*set.Carried = uv >= 0;
          *set.Any = *set.Any || part.*set.Carried;
          set.Into->resize((Positions_.size() / 3) * 2, 0.0);
          if (uv < 0) { continue; }
          std::vector<double> coordinates;
          if (!document.ReadElements(uv, coordinates)) {
            return Refuse(document.Path() + ": " + set.Semantic + " does not decode: " +
                          document.Error());
          }
          if (coordinates.size() != vertices * 2) {
            return Refuse(document.Path() + ": " + set.Semantic + " decodes to " +
                          std::to_string(coordinates.size() / 2) + " pairs over " +
                          std::to_string(vertices) + " vertices");
          }
          std::copy(coordinates.begin(), coordinates.end(),
                    set.Into->begin() + static_cast<std::ptrdiff_t>(part.FirstVertex * 2));
        }

        const int colour = primitive.Find("COLOR_0");
        part.HasColour = colour >= 0;
        anyColour = anyColour || part.HasColour;
        Colours_.resize((Positions_.size() / 3) * 4, 0.0);
        if (colour >= 0) {
          if ((size_t)colour >= document.Accessors().size()) {
            return Refuse(document.Path() + ": COLOR_0 names accessor " + std::to_string(colour) +
                          ", which the file does not carry");
          }
          size_t components = 0;
          std::string why;
          if (!VertexColourComponents(document.Accessors()[(size_t)colour], components, why)) {
            return Refuse(document.Path() + ": COLOR_0 " + why);
          }
          std::vector<double> tints;
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
              Colours_[(part.FirstVertex + vertex) * 4 + channel] = value;
            }
          }
        }

        const int normal = primitive.Find("NORMAL");
        part.HasNormal = normal >= 0;
        anyNormal = anyNormal || part.HasNormal;
        Normals_.resize(Positions_.size(), 0.0);
        if (normal >= 0) {
          std::vector<double> directions;
          if (!document.ReadElements(normal, directions)) {
            return Refuse(document.Path() + ": NORMAL does not decode: " + document.Error());
          }
          if (directions.size() != vertices * 3) {
            return Refuse(document.Path() + ": NORMAL decodes to " +
                          std::to_string(directions.size() / 3) + " vectors over " +
                          std::to_string(vertices) + " vertices");
          }
          std::vector<double> morphedNormals;
          if (!MorphDeltasFor(document, primitive, "NORMAL", nodeWeights.data(), morphCount, 3,
                              vertices, morphedNormals)) {
            return false;
          }
          for (size_t at = 0; at < morphedNormals.size(); ++at) { directions[at] += morphedNormals[at]; }
          for (size_t vertex = 0; vertex < vertices; ++vertex) {
            double local[3] = {directions[vertex * 3], directions[vertex * 3 + 1],
                               directions[vertex * 3 + 2]};
            double global[3];

            if (!place.At(vertex).Normal(local, global)) {
              global[0] = global[1] = global[2] = 0.0;
            }

            (void)Normalise(global);
            for (int axis = 0; axis < 3; ++axis) {
              Normals_[(part.FirstVertex + vertex) * 3 + (size_t)axis] = global[axis];
            }
          }
        }

        if (primitive.Indices >= 0) {
          if (!document.ReadIndices(primitive.Indices, run)) {
            return Refuse(document.Path() + ": the index accessor does not decode: " +
                          document.Error());
          }
        } else {
          run.resize(vertices);
          for (size_t vertex = 0; vertex < vertices; ++vertex) { run[vertex] = (uint32_t)vertex; }
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
              return Refuse(document.Path() + ": vertex " + std::to_string(vertex) +
                            " of a skinned primitive blends to a transform whose determinant has the "
                            "opposite sign to vertex 0's, so the primitive would need two windings");
            }
          }
          handedness = mirrored ? Handedness::Reversed : Handedness::Preserved;
        }
        Triangulate(primitive.Mode, handedness, run, indices);
        for (uint32_t index : indices) {
          if (index >= vertices) {
            return Refuse(document.Path() + ": index " + std::to_string(index) + " addresses past the " +
                          std::to_string(vertices) + " vertices of its own primitive");
          }
          Indices_.push_back(base + index);
        }
        part.IndexCount = Indices_.size() - part.FirstIndex;

        if (!FlatNormalsFor(part)) { return false; }
        if (!BuildTangentsFor(document, primitive, place,
                              Span<const double>(nodeWeights.data(), morphCount), part, vertices)) {
          return false;
        }
        anyTangent = anyTangent || part.HasTangent();
        anyNormal = anyNormal || part.HasNormal;
        part.VertexCount = VertexCount() - part.FirstVertex;

        if (part.IndexCount > 0) { Parts_.push_back(part); }
      }
    }

  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyUv1) { Uv1_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  if (!anyColour) { Colours_.clear(); }
  if (Indices_.empty()) {
    return Refuse(document.Path() + ": the default scene draws no triangle over " +
                  std::to_string(primitives) + " primitive(s), so there is nothing to render");
  }

  Bound();
  return true;
}

void Subject::Bound() {
  for (int axis = 0; axis < 3; ++axis) {
    Min_[axis] = Max_[axis] = Positions_[(size_t)axis];
  }
  for (size_t vertex = 1; vertex < VertexCount(); ++vertex) {
    for (int axis = 0; axis < 3; ++axis) {
      const double value = Positions_[vertex * 3 + (size_t)axis];
      if (value < Min_[axis]) { Min_[axis] = value; }
      if (value > Max_[axis]) { Max_[axis] = value; }
    }
  }
}

bool Subject::Assemble(const Assembly &what) {
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
  if (what.Pieces.Empty()) {
    return Refuse("an assembly of no piece draws nothing, and a subject with no triangle is not one");
  }

  bool anyUv = false;
  bool anyUv1 = false;
  bool anyNormal = false;
  bool anyTangent = false;
  bool anyColour = false;
  for (size_t index = 0; index < what.Pieces.Size(); ++index) {
    const Piece &piece = what.Pieces[index];
    const std::string where = "assembled piece " + std::to_string(index);
    if (piece.PositionsM.Empty() || (piece.PositionsM.Size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(piece.PositionsM.Size()) +
                    " position components, which is not a whole run of points");
    }
    const size_t vertices = piece.PositionsM.Size() / 3;
    if (piece.Indices.Empty() || (piece.Indices.Size() % 3) != 0) {
      return Refuse(where + " states " + std::to_string(piece.Indices.Size()) +
                    " indices, which is not a whole run of triangles");
    }
    if (piece.Material < -1) {
      return Refuse(where + " names material " + std::to_string(piece.Material) +
                    ", and -1 is the only spelling of naming none");
    }
    std::string why;
    if (!RunIsStatable(piece.PositionsM, vertices, 3, "positions", where, why) ||
        !RunIsStatable(piece.Normals, vertices, 3, "normals", where, why) ||
        !RunIsStatable(piece.Uv, vertices, 2, "uv pairs", where, why) ||
        !RunIsStatable(piece.Uv1, vertices, 2, "second-set uv pairs", where, why) ||
        !RunIsStatable(piece.Tangents, vertices, 4, "tangents", where, why) ||
        !RunIsStatable(piece.Colours, vertices, 4, "vertex colours", where, why)) {
      return Refuse(why);
    }

    for (size_t at = 0; at < piece.Colours.Size(); ++at) {
      if (piece.Colours[at] >= 0.0f && piece.Colours[at] <= 1.0f) { continue; }
      return Refuse(where + " states a vertex colour component of " +
                    std::to_string(piece.Colours[at]) + " at " + std::to_string(at) +
                    ", and the format requires every component in [0, 1]");
    }

    Part part;
    part.NodeName = piece.NodeName;
    part.Material = piece.Material;
    part.FirstVertex = VertexCount();
    part.FirstIndex = Indices_.size();
    part.VertexCount = vertices;
    part.IndexCount = piece.Indices.Size();
    part.HasUv = !piece.Uv.Empty();
    part.HasUv1 = !piece.Uv1.Empty();
    part.HasNormal = !piece.Normals.Empty();
    part.HasColour = !piece.Colours.Empty();

    part.Tangent = piece.Tangents.Empty() ? TangentSource::None : TangentSource::Supplied;
    anyUv = anyUv || part.HasUv;
    anyUv1 = anyUv1 || part.HasUv1;
    anyNormal = anyNormal || part.HasNormal;
    anyTangent = anyTangent || part.HasTangent();
    anyColour = anyColour || part.HasColour;

    for (const float component : piece.PositionsM) { Positions_.push_back((double)component); }

    Uv_.resize((Positions_.size() / 3) * 2, 0.0);
    Uv1_.resize((Positions_.size() / 3) * 2, 0.0);
    Normals_.resize(Positions_.size(), 0.0);
    Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
    Colours_.resize((Positions_.size() / 3) * 4, 0.0);
    for (size_t at = 0; at < piece.Uv.Size(); ++at) {
      Uv_[part.FirstVertex * 2 + at] = (double)piece.Uv[at];
    }
    for (size_t at = 0; at < piece.Uv1.Size(); ++at) {
      Uv1_[part.FirstVertex * 2 + at] = (double)piece.Uv1[at];
    }
    for (size_t at = 0; at < piece.Normals.Size(); ++at) {
      Normals_[part.FirstVertex * 3 + at] = (double)piece.Normals[at];
    }
    for (size_t at = 0; at < piece.Tangents.Size(); ++at) {
      Tangents_[part.FirstVertex * 4 + at] = (double)piece.Tangents[at];
    }
    for (size_t at = 0; at < piece.Colours.Size(); ++at) {
      Colours_[part.FirstVertex * 4 + at] = (double)piece.Colours[at];
    }

    for (const uint32_t local : piece.Indices) {
      if (local >= vertices) {
        return Refuse(where + " addresses vertex " + std::to_string(local) + " of its own " +
                      std::to_string(vertices));
      }
      Indices_.push_back((uint32_t)part.FirstVertex + local);
    }
    Parts_.push_back(part);
  }

  if (!anyUv) { Uv_.clear(); }
  if (!anyUv1) { Uv1_.clear(); }
  if (!anyNormal) { Normals_.clear(); }
  if (!anyTangent) { Tangents_.clear(); }
  if (!anyColour) { Colours_.clear(); }
  Bound();
  return true;
}

double Subject::RadiusM() const {
  const double span[3] = {Max_[0] - Min_[0], Max_[1] - Min_[1], Max_[2] - Min_[2]};
  return 0.5 * Length(span);
}

void Subject::CentreM(double out[3]) const {
  for (int axis = 0; axis < 3; ++axis) { out[axis] = 0.5 * (Min_[axis] + Max_[axis]); }
}

bool Subject::Frame(Placement &out, double fill) const {
  return FramingFor(Min_, Max_, out, fill);
}

bool FramingFor(const double minM[3], const double maxM[3], Placement &out, double fill) {
  const double span[3] = {maxM[0] - minM[0], maxM[1] - minM[1], maxM[2] - minM[2]};
  const double radius = 0.5 * Length(span);
  if (!(radius > 0)) { return false; }
  double centre[3];
  for (int axis = 0; axis < 3; ++axis) { centre[axis] = 0.5 * (minM[axis] + maxM[axis]); }

  const double azimuth = kFramingAzimuthDeg * kPi / 180.0;
  const double elevation = kFramingElevationDeg * kPi / 180.0;

  double toEye[3] = {std::cos(elevation) * std::cos(azimuth), std::sin(elevation),
                     std::cos(elevation) * std::sin(azimuth)};

  const double yfov = 2.0 * std::atan(kFramingSensorHalfHeightMm / kFramingFocalLengthMm);
  const double distance = radius / std::sin(0.5 * yfov) / (fill > 0 ? fill : kFramingFill);
  double eye[3];
  for (int axis = 0; axis < 3; ++axis) { eye[axis] = centre[axis] + toEye[axis] * distance; }

  if (!Placement::LookAt(eye, centre, 0.0, out)) { return false; }
  out.YfovRad = yfov;
  const double floor = radius * kFramingNearFloorFraction;
  out.ZNearM = (distance - radius > floor) ? distance - radius : floor;
  out.ZFarM = distance + radius;
  return true;
}

bool DeclaredPlacement(const Document &document, int cameraIndex, Placement &out,
                       std::string &error) {
  if (cameraIndex < 0 || (size_t)cameraIndex >= document.Cameras().size()) {
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

  const Camera &lens = document.Cameras()[(size_t)cameraIndex];
  Transform world;
  if (!document.WorldTransform((int)holder, world)) {
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
  out.Kind = lens.Kind;
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
    double raster[3][2];
    for (int corner = 0; corner < 3; ++corner) {
      const size_t vertex = Indices_[triangle * 3 + (size_t)corner];
      const double point[3] = {Positions_[vertex * 3], Positions_[vertex * 3 + 1],
                               Positions_[vertex * 3 + 2]};
      double ndc[3];
      clip.Point(point, ndc);
      viewport.Raster(ndc, raster[corner]);
    }
    total += 0.5 * std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                             (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1]));
  }
  return total;
}

}
