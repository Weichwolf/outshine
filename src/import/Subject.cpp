#include "math/Vec4.h"
#include "math/Mat4.h"
#include "Heap.h"
#include "MaterialValidation.h"
#include "math/Units.h"
#include "math/Vec3.h"
#include "Subject.h"

#include <array>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <expected>
#include <ranges>
#include <span>

#include <scene/Geometry.h>

#include <numbers>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "Document.h"
#include "CameraFraming.h"
#include "Tangents.h"

namespace outshine::Gltf {
namespace Says {
constexpr auto NativeAttributesFailed = "native vertex attribute conversion failed";
constexpr auto NativeCapacityFailed = "native assembly index or attribute capacity exhausted";
}

constexpr uint64_t kGoldenWord = 0x9e3779b97f4a7c15ull;

static_assert(std::is_nothrow_move_assignable_v<Subject>);

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

}

void Subject::MorphDeltasFor(const MeshPrimitive &mesh,
                             const Deltas &over,
                             std::vector<double> &out) {
  const size_t vertices = over.Vertices;
  out.clear();
  if (over.Morph.Count == 0 || mesh.MorphTargets.empty()) { return; }
  for (size_t target = 0; target < over.Morph.Count && target < mesh.MorphTargets.size();
       ++target) {
    const double share = over.Morph.Weights[target];
    if (share == 0.0) { continue; }
    const MorphTargetDelta &nativeTarget = mesh.MorphTargets[target];
    const std::vector<float> *delta = nullptr;
    switch (over.Which) {
      case Deltas::Attribute::Position: delta = &nativeTarget.Positions; break;
      case Deltas::Attribute::Normal: delta = &nativeTarget.Normals; break;
      case Deltas::Attribute::Tangent: delta = &nativeTarget.Tangents; break;
    }
    if (delta->empty()) { continue; }
    assert(delta->size() == vertices * 3);
    if (out.empty()) { out.assign(vertices * 3, 0.0); }
    for (size_t at = 0; at < delta->size(); ++at) { out[at] += share * (*delta)[at]; }
  }
}

AffineTransform
Subject::JointMatrix(const Skeleton &skeleton, size_t joint, const AffineTransform &world) {
  return world * skeleton.InverseBind[joint];
}

bool Subject::BlendJoints(const Document &document,
                          std::span<const AffineTransform> joints,
                          const VertexSkinBinding &bound,
                          size_t vertices,
                          std::vector<AffineTransform> &out) {
  out.assign(vertices, AffineTransform());
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    Mat4 blended = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
    double sum = 0;
    for (size_t set = 0; set < bound.Sets; ++set) {
      const size_t base = set * vertices * 4 + vertex * 4;
      for (size_t slot = 0; slot < 4; ++slot) {
        const double share = bound.Weights[base + slot];
        if (share == 0.0) { continue; }
        const uint32_t named = bound.Joints[base + slot];
        if (static_cast<size_t>(named) >= joints.size()) {
          return Refuse(document.Path() + ": JOINTS_" + std::to_string(set) + " of vertex " +
                        std::to_string(vertex) + " names joint " + std::to_string(named) +
                        " and the skin declares " + std::to_string(joints.size()));
        }
        sum += share;
        const AffineTransform &matrix = joints[named];
        for (int at = 0; at < 16; ++at) { blended[at] += share * matrix.M[at]; }
      }
    }
    if (sum == 0.0) {
      return Refuse(document.Path() + ": the weights of vertex " + std::to_string(vertex) +
                    " sum to zero over all " + std::to_string(bound.Sets) +
                    " sets, so the vertex is bound to no joint and names no position");
    }
    for (int at = 0; at < 16; ++at) { out[vertex].M[at] = blended[at]; }
  }
  return true;
}

void Subject::SuppliedTangentsFor(const MeshPrimitive &mesh,
                                  const VertexPlacement &place,
                                  std::span<const double> morphWeights,
                                  Part &part,
                                  size_t vertices,
                                  std::vector<double> &into) {
  if (!mesh.Tangents.empty()) {
    std::vector<double> elements(mesh.Tangents.begin(), mesh.Tangents.end());
    std::vector<double> morphedTangents;
    MorphDeltasFor(mesh,
                   {.Which = Deltas::Attribute::Tangent,
                    .Morph = {.Weights = morphWeights, .Count = morphWeights.size()},
                    .Vertices = vertices},
                   morphedTangents);
    for (size_t vertex = 0; vertex < vertices && !morphedTangents.empty(); ++vertex) {
      for (size_t axis = 0; axis < 3; ++axis) {
        elements[vertex * 4 + axis] += morphedTangents[vertex * 3 + axis];
      }
    }

    into.assign(vertices * 4, 0.0);
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const AffineTransform &placed = place.At(vertex);
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
  }
}

Vec3 Subject::FaceNormalOf(std::span<const uint32_t, 3> of) const {
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
  if (!(length > 0.0)) { return Vec3{{0.0, 0.0, 0.0}}; }
  for (double &axis : face) { axis /= length; }
  return face;
}

uint32_t Subject::CloneVertex(uint32_t vertex) {
  const auto made = static_cast<uint32_t>(VertexCount());
  const auto from = static_cast<size_t>(vertex);
  const auto to = static_cast<size_t>(made);
  for (size_t axis = 0; axis < 3; ++axis) { Positions_.push_back(Positions_[from * 3 + axis]); }
  if (!Uv_.empty()) {
    Uv_.resize(to * 2 + 2, 0.0);
    Uv_[to * 2] = Uv_[from * 2];
    Uv_[to * 2 + 1] = Uv_[from * 2 + 1];
  }
  if (!Uv1_.empty()) {
    Uv1_.resize(to * 2 + 2, 0.0);
    Uv1_[to * 2] = Uv1_[from * 2];
    Uv1_[to * 2 + 1] = Uv1_[from * 2 + 1];
  }
  if (!Colours_.empty()) {
    Colours_.resize(to * 4 + 4, 0.0);
    for (size_t channel = 0; channel < 4; ++channel) {
      Colours_[to * 4 + channel] = Colours_[from * 4 + channel];
    }
  }
  return made;
}

bool Subject::GeneratedTangentsFor(Part &part) {
  Tangents_.resize((Positions_.size() / 3) * 4, 0.0);
  if (part.Tangent == TangentSource::Supplied) { return true; }
  const bool needed = part.Material >= 0 &&
                      static_cast<size_t>(part.Material) < TangentWanted_.size() &&
                      TangentWanted_[static_cast<size_t>(part.Material)] != 0;
  if (!needed || !part.HasNormal || !part.HasUv || part.IndexCount == 0) { return true; }

  TangentSubject over;
  over.PositionsM = Positions_;
  over.Normals = Normals_;
  over.Uv = Uv_;
  over.Indices = std::span(Indices_).subspan(part.FirstIndex, part.IndexCount);
  std::vector<double> corners;
  if (const auto generated = GenerateTangents(over, corners); !generated) {
    return Refuse("the tangent basis the material needs cannot be generated: " +
                  std::string(generated.error()));
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
    const uint32_t made = CloneVertex(vertex);
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

      const uint32_t made = CloneVertex(vertex);
      Normals_.resize(static_cast<size_t>(made) * 3 + 3, 0.0);
      if (!Tangents_.empty()) { Tangents_.resize(static_cast<size_t>(made) * 4 + 4, 0.0); }
      of[corner] = made;
      Indices_[part.FirstIndex + triangle + corner] = made;
    }

    const Vec3 face = FaceNormalOf(of);
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

bool Subject::Refuse(std::string why) {
  Error_ = std::move(why);
  Images_.clear();
  Positions_.clear();
  Uv_.clear();
  Uv1_.clear();
  Normals_.clear();
  Tangents_.clear();
  Colours_.clear();
  Indices_.clear();
  Parts_.clear();
  Surfaces_.clear();
  SurfaceNames_.clear();
  return false;
}

bool Subject::Build(const Document &document,
                    std::span<const Skeleton> skeletons,
                    const MeshAssetSet &meshes,
                    const MaterialAssetSet &materials,
                    const SceneAsset &scene,
                    const VariantSelection &variant) {
  return Flatten(document, skeletons, meshes, materials, scene, nullptr, nullptr, variant);
}

bool Subject::Build(const Document &document,
                    std::span<const Skeleton> skeletons,
                    const MeshAssetSet &meshes,
                    const MaterialAssetSet &materials,
                    const SceneAsset &scene,
                    std::span<const AffineTransform> pose,
                    std::span<const double> weights,
                    const VariantSelection &variant) {
  if (pose.size() != scene.NodeCount()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(pose.size()) +
                  " local transforms and the file carries " + std::to_string(scene.NodeCount()) +
                  " nodes");
  }

  if (!weights.empty() && weights.size() != scene.MorphWeightCount()) {
    return Refuse(document.Path() + ": the pose states " + std::to_string(weights.size()) +
                  " morph weights and the file's nodes carry " +
                  std::to_string(scene.MorphWeightCount()));
  }
  return Flatten(document,
                 skeletons,
                 meshes,
                 materials,
                 scene,
                 pose.data(),
                 (!weights.empty()) ? weights.data() : nullptr,
                 variant);
}

namespace {

void InstanceTransforms(const SceneAsset &scene,
                        size_t node,
                        const AffineTransform &world,
                        std::vector<AffineTransform> &out) {
  out.clear();
  const SceneNodeAsset *asset = scene.Node(node);
  assert(asset != nullptr);
  if (asset->Instances.empty()) {
    out.push_back(world);
    return;
  }
  out.reserve(asset->Instances.size());
  for (const AffineTransform &local : asset->Instances) { out.push_back(world * local); }
}
}

bool Subject::ReadTriangleRun(const Document &document,
                              const MeshPrimitive &mesh,
                              const AffineTransform &world,
                              std::span<const AffineTransform> skinned) {
  bool mirrored = false;
  if (skinned.empty()) {
    mirrored = world.LinearDeterminant() < 0;
  } else {
    mirrored = skinned[0].LinearDeterminant() < 0;
    for (size_t vertex = 1; vertex < skinned.size(); ++vertex) {
      if ((skinned[vertex].LinearDeterminant() < 0) != mirrored) {
        return Refuse(document.Path() + ": vertex " + std::to_string(vertex) +
                      " of a skinned primitive blends to a transform whose determinant has the "
                      "opposite sign to vertex 0's, so the primitive would need two windings");
      }
    }
  }
  Scratch_.Idx = mesh.Triangles;
  if (mirrored) {
    for (size_t triangle = 0; triangle * 3 + 2 < Scratch_.Idx.size(); ++triangle) {
      std::swap(Scratch_.Idx[triangle * 3 + 1], Scratch_.Idx[triangle * 3 + 2]);
    }
  }
  return true;
}

bool Subject::EmitPart(outshine::Geometry &made, const Part &part) {
  std::vector<float> &narrowed = Scratch_.Narrowed;
  const auto asFloat = [&narrowed](const std::vector<double> &from) {
    narrowed.resize(from.size());
    for (size_t at = 0; at < from.size(); ++at) { narrowed[at] = static_cast<float>(from[at]); }
    return std::span<const float>(narrowed.data(), narrowed.size());
  };
  const auto createdPart = made.addPart(part.NodeName, MaterialInstance(part.Material));
  if (!createdPart) { return false; }
  const int emitted = *createdPart;
  if (!made.setPositions(emitted, asFloat(Scratch_.Pos))) { return false; }
  if (part.HasNormal && !made.setNormals(emitted, asFloat(Scratch_.Nor))) { return false; }
  if (part.HasUv && !made.setTexture(emitted, asFloat(Scratch_.Uv), 0)) { return false; }
  if (part.HasUv1 && !made.setTexture(emitted, asFloat(Scratch_.Uv1), 1)) { return false; }
  if (part.HasTangent() && !made.setTangents(emitted, asFloat(Scratch_.Tan))) { return false; }
  if (part.HasColour && !made.setColours(emitted, asFloat(Scratch_.Col))) { return false; }
  return made
      .setTriangles(emitted, std::span<const uint32_t>(Scratch_.Idx.data(), Scratch_.Idx.size()))
      .has_value();
}

bool Subject::FlattenLight(const Document &document,
                           size_t nodeIndex,
                           const SceneNodeAsset &node,
                           const SceneLightAsset &light,
                           const AffineTransform &placement) {
  PlacedLight placed;
  placed.NodeName = node.Name;
  placed.LightName = light.Name;
  placed.Light = light.Light;

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
  return true;
}

void Subject::ReadUvSets(const MeshPrimitive &mesh, size_t vertices, Part &part) {
  struct UvSet {
    const std::vector<float> *Source;
    bool Part::*Carried;
    std::vector<double> *Into;
  };

  const std::array<UvSet, kUvSets> sets = {
      {{.Source = &mesh.TextureCoordinates, .Carried = &Part::HasUv, .Into = &Scratch_.Uv},
       {.Source = &mesh.SecondaryTextureCoordinates,
        .Carried = &Part::HasUv1,
        .Into = &Scratch_.Uv1}}};

  for (const UvSet &set : sets) {
    part.*set.Carried = !set.Source->empty();
    set.Into->assign(set.Source->begin(), set.Source->end());
    if (set.Source->empty()) { set.Into->assign(vertices * 2, 0.0); }
  }
}

void Subject::ReadVertexColours(const MeshPrimitive &mesh, size_t vertices, Part &part) {
  part.HasColour = !mesh.Colours.empty();
  Scratch_.Col.assign(mesh.Colours.begin(), mesh.Colours.end());
  if (mesh.Colours.empty()) { Scratch_.Col.assign(vertices * 4, 0.0); }
}

void Subject::ReadVertexNormals(const MeshPrimitive &mesh,
                                const VertexPlacement &place,
                                Morphing morph,
                                size_t vertices,
                                Part &part) {
  part.HasNormal = !mesh.Normals.empty();
  Scratch_.Nor.assign(vertices * 3, 0.0);
  if (part.HasNormal) {
    std::vector<double> &directions = Scratch_.Directions;
    directions.assign(mesh.Normals.begin(), mesh.Normals.end());
    std::vector<double> &morphedNormals = Scratch_.MorphedNormals;
    morphedNormals.clear();
    MorphDeltasFor(mesh,
                   {.Which = Deltas::Attribute::Normal, .Morph = morph, .Vertices = vertices},
                   morphedNormals);
    for (size_t at = 0; at < morphedNormals.size(); ++at) { directions[at] += morphedNormals[at]; }
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      const Vec3 local = {
          {directions[vertex * 3], directions[vertex * 3 + 1], directions[vertex * 3 + 2]}};
      Vec3 global;

      if (!place.At(vertex).Normal(local, global)) { global[0] = global[1] = global[2] = 0.0; }

      (void)Normalise(global);
      for (int axis = 0; axis < 3; ++axis) {
        Scratch_.Nor[vertex * 3 + static_cast<size_t>(axis)] = global[axis];
      }
    }
  }
}

bool Subject::PlacementOf(const Posing &posed, size_t node, AffineTransform &out) {
  const std::span<const AffineTransform> pose =
      posed.Pose == nullptr
          ? std::span<const AffineTransform>()
          : std::span<const AffineTransform>(posed.Pose, posed.Scene->NodeCount());
  return posed.Scene->WorldTransform(node, pose, out);
}

bool Subject::ResolveJointMatrices(const Document &document,
                                   const Posing &posed,
                                   size_t nodeIndex,
                                   const SceneNodeAsset &node,
                                   std::vector<AffineTransform> &out) {
  out.clear();
  if (node.Skin < 0) { return true; }
  if (static_cast<size_t>(node.Skin) >= posed.Skeletons.size()) {
    return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                  " names a native skeleton the imported asset does not carry");
  }
  const Skeleton &skeleton = posed.Skeletons[static_cast<size_t>(node.Skin)];
  out.assign(skeleton.JointNodes.size(), AffineTransform());
  for (size_t joint = 0; joint < skeleton.JointNodes.size(); ++joint) {
    AffineTransform placed;
    const uint32_t jointNode = skeleton.JointNodes[joint];
    if (!PlacementOf(posed, static_cast<size_t>(jointNode), placed)) {
      return Refuse(document.Path() + ": joint node " + std::to_string(jointNode) +
                    " has no world transform: " + document.Error());
    }
    out[joint] = JointMatrix(skeleton, joint, placed);
  }
  return true;
}

bool Subject::FlattenMesh(const Document &document,
                          const Posing &posed,
                          int nodeIndex,
                          outshine::Geometry &made,
                          size_t &primitives) {
  const SceneNodeAsset *nativeNode = posed.Scene->Node(static_cast<size_t>(nodeIndex));
  if (nativeNode == nullptr) {
    return Refuse(document.Path() + ": native scene has no node " + std::to_string(nodeIndex));
  }
  const SceneNodeAsset &node = *nativeNode;
  if (node.Mesh < 0) { return true; }
  if (static_cast<size_t>(node.Mesh) >= posed.Meshes->MeshCount()) {
    return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) + " names mesh " +
                  std::to_string(node.Mesh) + ", which the file does not carry");
  }
  AffineTransform world;
  if (!PlacementOf(posed, static_cast<size_t>(nodeIndex), world)) {
    return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                  " has no world transform: " + document.Error());
  }

  const size_t morphCount = node.RestMorphWeights.size();
  std::vector<double> &nodeWeights = Scratch_.NodeWeights;
  nodeWeights.clear();
  if (morphCount > 0) {
    for (size_t at = 0; at < morphCount; ++at) {
      nodeWeights.push_back((posed.Weights != nullptr) ? posed.Weights[node.MorphWeightFirst + at]
                                                       : node.RestMorphWeights[at]);
    }
  }

  std::vector<AffineTransform> &jointMatrices = Scratch_.Joints;
  if (!ResolveJointMatrices(document, posed, nodeIndex, node, jointMatrices)) { return false; }

  std::vector<AffineTransform> &instances = Scratch_.Instances;
  instances.clear();
  InstanceTransforms(*posed.Scene, static_cast<size_t>(nodeIndex), world, instances);
  for (const AffineTransform &placedWorld : instances) {
    const size_t primitiveCount = posed.Meshes->PrimitiveCount(static_cast<size_t>(node.Mesh));
    for (size_t primitiveIndex = 0; primitiveIndex < primitiveCount; ++primitiveIndex) {
      const MeshPrimitive *meshAsset =
          posed.Meshes->Find(static_cast<size_t>(node.Mesh), primitiveIndex);
      if (meshAsset == nullptr) {
        return Refuse(document.Path() + ": native mesh asset has no mesh " +
                      std::to_string(node.Mesh) + " primitive " + std::to_string(primitiveIndex));
      }
      ++primitives;
      const Placing under{
          .Node = node,
          .Materials = *posed.Materials,
          .World = world,
          .Placed = placedWorld,
          .Joints = jointMatrices,
          .Primitive = *meshAsset,
          .Morph = {.Weights = std::span<const double>(nodeWeights.data(), morphCount),
                    .Count = morphCount},
          .Variant = posed.Variant};
      if (!FlattenPrimitive(document, under, made)) { return false; }
    }
  }
  return true;
}

bool Subject::FlattenPrimitive(const Document &document,
                               const Placing &under,
                               outshine::Geometry &made) {
  std::vector<double> &elements = Scratch_.Elements;
  Part part;
  part.NodeName = under.Node.Name;
  part.Material = under.Primitive.MaterialFor(under.Variant);
  if (part.Material >= 0 && static_cast<size_t>(part.Material) >= under.Materials.MaterialCount()) {
    return Refuse(document.Path() + ": native primitive names absent material " +
                  std::to_string(part.Material));
  }
  const std::string_view materialError = under.Materials.ErrorAt(part.Material);
  if (!materialError.empty()) {
    return Refuse(document.Path() + ": " + std::string(materialError));
  }
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

  if (under.Primitive.Triangles.empty()) {
    ++Undrawn_.Primitives;
    return true;
  }
  elements.assign(under.Primitive.Positions.begin(), under.Primitive.Positions.end());
  if (elements.empty()) {
    return Refuse(document.Path() + ": primitive of mesh " + std::to_string(under.Node.Mesh) +
                  " carries no native positions, and nothing here invents them");
  }
  const size_t vertices = elements.size() / 3;

  std::vector<double> &morphedPositions = Scratch_.Morphed;
  morphedPositions.clear();
  MorphDeltasFor(under.Primitive,
                 {.Which = Deltas::Attribute::Position, .Morph = under.Morph, .Vertices = vertices},
                 morphedPositions);
  for (size_t at = 0; at < morphedPositions.size(); ++at) { elements[at] += morphedPositions[at]; }
  std::vector<AffineTransform> &skinned = Scratch_.Skinned;
  skinned.clear();
  if (under.Node.Skin >= 0) {
    const VertexSkinBinding &binding = under.Primitive.Skin;
    if (binding.Vertices != vertices) {
      return Refuse(document.Path() + ": native skin binding has " +
                    std::to_string(binding.Vertices) + " vertices for a primitive carrying " +
                    std::to_string(vertices));
    }
    if (!BlendJoints(document, under.Joints, binding, vertices, skinned)) { return false; }
  }
  const VertexPlacement place{.Node = under.Placed,
                              .Skinned = skinned.empty() ? nullptr : skinned.data()};
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    const Vec3 local = {{elements[vertex * 3], elements[vertex * 3 + 1], elements[vertex * 3 + 2]}};
    Vec3 global;
    place.At(vertex).Point(local, global);
    for (const double axis : global) { atPos.push_back(axis); }
  }

  ReadUvSets(under.Primitive, vertices, part);

  ReadVertexColours(under.Primitive, vertices, part);

  ReadVertexNormals(under.Primitive, place, under.Morph, vertices, part);

  if (!ReadTriangleRun(document, under.Primitive, under.World, skinned)) { return false; }
  part.IndexCount = atIdx.size();
  SuppliedTangentsFor(under.Primitive, place, under.Morph.Weights, part, vertices, atTan);
  part.VertexCount = atPos.size() / 3;

  if (part.IndexCount == 0) { return true; }
  if (!EmitPart(made, part)) { return Refuse(Says::NativeAttributesFailed); }
  return true;
}

bool Subject::Flatten(const Document &document,
                      std::span<const Skeleton> skeletons,
                      const MeshAssetSet &meshes,
                      const MaterialAssetSet &materials,
                      const SceneAsset &scene,
                      const AffineTransform *pose,
                      const double *weights,
                      const VariantSelection &variant) {
  Error_.clear();
  Images_.clear();
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
  const auto copiedMaterials = materials.CopyTo(made);
  if (!copiedMaterials) { return Refuse(copiedMaterials.error()); }
  if (scene.Roots().empty()) { return Refuse(document.Path() + ": no default scene to draw"); }

  int activeVariant = -1;
  {
    std::string why;
    if (!variant.Against(document, activeVariant, why)) {
      return Refuse(document.Path() + ": the declaration " + why);
    }
  }
  const Posing posed{.Skeletons = skeletons,
                     .Meshes = &meshes,
                     .Materials = &materials,
                     .Scene = &scene,
                     .Pose = pose,
                     .Weights = weights,
                     .Variant = activeVariant};

  std::vector<uint32_t> pending(scene.Roots().rbegin(), scene.Roots().rend());

  size_t primitives = 0;
  while (!pending.empty()) {
    const uint32_t nodeIndex = pending.back();
    pending.pop_back();
    const SceneNodeAsset *nodeAsset = scene.Node(static_cast<size_t>(nodeIndex));
    if (nodeAsset == nullptr) {
      return Refuse(document.Path() + ": scene names node " + std::to_string(nodeIndex) +
                    ", which the file does not carry");
    }
    const SceneNodeAsset &node = *nodeAsset;
    if (!node.Visible) { continue; }
    for (const uint32_t child : std::views::reverse(node.Children)) { pending.push_back(child); }
    if (node.Light >= 0) {
      const SceneLightAsset *light = scene.Light(static_cast<size_t>(node.Light));
      if (light == nullptr) {
        return Refuse(document.Path() + ": native scene has no light " +
                      std::to_string(node.Light));
      }
      AffineTransform placement;
      if (!PlacementOf(posed, static_cast<size_t>(nodeIndex), placement)) {
        return Refuse(document.Path() + ": node " + std::to_string(nodeIndex) +
                      " carries a light and has no world transform: " + document.Error());
      }
      if (!FlattenLight(document, static_cast<size_t>(nodeIndex), node, *light, placement)) {
        return false;
      }
    }
    if (!FlattenMesh(document, posed, static_cast<int>(nodeIndex), made, primitives)) {
      return false;
    }
  }

  if (made.parts() == 0) {
    return Refuse(document.Path() + ": the default scene draws no triangle over " +
                  std::to_string(primitives) + " primitive(s), so there is nothing to render");
  }

  const std::vector<PlacedLight> lit = Lights_;
  const Undrawn missed = Undrawn_;
  {
    static const Heap::Tag kAssemblingTag("pose-assemble");
    const Heap::Tagged assembling(kAssemblingTag);
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

std::vector<ImageView> Subject::Images() const {
  std::vector<ImageView> images;
  images.reserve(Images_.size());
  for (const Core::Raster &image : Images_) {
    images.push_back({.WidthPx = image.Width, .HeightPx = image.Height, .Rgba = image.Rgba});
  }
  return images;
}

std::expected<void, std::string> Subject::CopyNativeAssets(outshine::Geometry &out) const {
  for (const Core::Raster &image : Images_) {
    const auto added = out.addImage(image.Width, image.Height, image.Rgba);
    if (!added) { return std::unexpected(std::string(Describe(added.error()))); }
  }
  for (size_t at = 0; at < Surfaces_.size(); ++at) {
    if (!MaterialIsValid(Surfaces_[at], Images_.size())) {
      return std::unexpected(std::string(Describe(MaterialError::InvalidMaterial)));
    }
    const std::string_view name =
        at < SurfaceNames_.size() ? SurfaceNames_[at] : std::string_view();
    const auto added = out.addSurface(name, Surfaces_[at]);
    if (!added) { return std::unexpected(std::string(Describe(added.error()))); }
  }
  for (const PlacedLight &lit : Lights_) {
    Mat4 placed;
    placed.SetTranslation({{lit.Light.Position[0], lit.Light.Position[1], lit.Light.Position[2]}});
    PunctualLight local = lit.Light;
    local.Position = {};
    const auto added = out.addLamp(lit.NodeName, local, placed);
    if (!added) { return std::unexpected(std::string(Describe(added.error()))); }
  }
  return {};
}

std::expected<outshine::Geometry, std::string> Subject::Handed() const {
  outshine::Geometry out;
  const auto assets = CopyNativeAssets(out);
  if (!assets) { return std::unexpected(assets.error()); }
  const auto floats = [](const std::vector<double> &from, size_t first, size_t many) {
    std::vector<float> made(many);
    for (size_t at = 0; at < many; ++at) { made[at] = static_cast<float>(from[first + at]); }
    return made;
  };
  for (const Part &one : Parts_) {
    const auto createdPart = out.addPart(one.NodeName, MaterialInstance(one.Material));
    if (!createdPart) { return std::unexpected(std::string(Describe(createdPart.error()))); }
    const int made = *createdPart;
    const std::vector<float> positions =
        floats(Positions_, one.FirstVertex * 3, one.VertexCount * 3);
    const auto positioned = out.setPositions(made, positions);
    if (!positioned) { return std::unexpected(std::string(Describe(positioned.error()))); }

    struct ChannelRow {
      bool Carried;
      const std::vector<double> *From;
      size_t Stride;
      std::expected<void, GeometryAttributeError> (*Set)(outshine::Geometry &,
                                                         int,
                                                         std::span<const float>);
    };

    const std::array<ChannelRow, 5> channels = {
        {{.Carried = one.HasNormal,
          .From = &Normals_,
          .Stride = 3,
          .Set = [](outshine::Geometry &into,
                    int at,
                    std::span<const float> held) { return into.setNormals(at, held); }},
         {.Carried = one.HasUv,
          .From = &Uv_,
          .Stride = 2,
          .Set = [](outshine::Geometry &into,
                    int at,
                    std::span<const float> held) { return into.setTexture(at, held, 0); }},
         {.Carried = one.HasUv1,
          .From = &Uv1_,
          .Stride = 2,
          .Set = [](outshine::Geometry &into,
                    int at,
                    std::span<const float> held) { return into.setTexture(at, held, 1); }},
         {.Carried = one.HasTangent(),
          .From = &Tangents_,
          .Stride = 4,
          .Set = [](outshine::Geometry &into,
                    int at,
                    std::span<const float> held) { return into.setTangents(at, held); }},
         {.Carried = one.HasColour,
          .From = &Colours_,
          .Stride = 4,
          .Set = [](outshine::Geometry &into, int at, std::span<const float> held) {
            return into.setColours(at, held);
          }}}};

    for (const ChannelRow &channel : channels) {
      if (!channel.Carried || channel.From->empty()) { continue; }
      const std::vector<float> held =
          floats(*channel.From, one.FirstVertex * channel.Stride, one.VertexCount * channel.Stride);
      const auto set = channel.Set(out, made, held);
      if (!set) { return std::unexpected(std::string(Describe(set.error()))); }
    }
    std::vector<uint32_t> run(one.IndexCount);
    for (size_t at = 0; at < one.IndexCount; ++at) {
      run[at] = static_cast<uint32_t>(Indices_[one.FirstIndex + at] - one.FirstVertex);
    }
    const auto triangulated = out.setTriangles(made, run);
    if (!triangulated) { return std::unexpected(std::string(Describe(triangulated.error()))); }
  }
  return out;
}

void Subject::AssembleLights(const outshine::Geometry &what) {
  for (int lamp = 0; lamp < what.lamps(); ++lamp) {
    PlacedLight placed;
    placed.NodeName = std::string(what.lampNameOf(lamp));
    placed.LightName = placed.NodeName;
    placed.Light = what.lampAt(lamp);
    const Mat4 &placement = what.lampPlacementOf(lamp);
    const Vec3f &position = placed.Light.Position;
    const Vec3f &direction = placed.Light.Direction;
    const Vec3 stood = placement.TransformPoint({{position[0], position[1], position[2]}});
    Vec3 beam = placement.TransformDirection({{direction[0], direction[1], direction[2]}});
    (void)Normalise(beam);
    for (int axis = 0; axis < 3; ++axis) {
      placed.Light.Position[axis] = static_cast<float>(stood[axis]);
      placed.Light.Direction[axis] = static_cast<float>(beam[axis]);
    }
    Lights_.push_back(std::move(placed));
  }
}

std::expected<size_t, std::string> Subject::ValidatePart(const outshine::Geometry &what, int slot) {
  const std::span<const float> pPos = what.positionsOf(slot);
  const std::span<const uint32_t> pIndices = what.trianglesOf(slot);
  const std::span<const float> pNormals = what.normalsOf(slot);
  const std::span<const float> pUv = what.textureOf(slot, Geometry::UvSet::Uv0);
  const std::span<const float> pUv1 = what.textureOf(slot, Geometry::UvSet::Uv1);
  const std::span<const float> pTangents = what.tangentsOf(slot);
  const std::span<const float> pColours = what.coloursOf(slot);
  const std::string where = "assembled piece " + std::to_string(slot);
  if (pPos.empty() || (pPos.size() % 3) != 0) {
    return std::unexpected(where + " states " + std::to_string(pPos.size()) +
                           " position components, which is not a whole run of points");
  }
  const size_t vertices = pPos.size() / 3;
  if (pIndices.empty() || (pIndices.size() % 3) != 0) {
    return std::unexpected(where + " states " + std::to_string(pIndices.size()) +
                           " indices, which is not a whole run of triangles");
  }
  if (what.materialOf(slot).index() < -1) {
    return std::unexpected(where + " names material " +
                           std::to_string(what.materialOf(slot).index()) +
                           ", and -1 is the only spelling of naming none");
  }
  std::string why;
  if (!RunIsStatable(pPos, vertices, 3, "positions", where, why) ||
      !RunIsStatable(pNormals, vertices, 3, "normals", where, why) ||
      !RunIsStatable(pUv, vertices, 2, "uv pairs", where, why) ||
      !RunIsStatable(pUv1, vertices, 2, "second-set uv pairs", where, why) ||
      !RunIsStatable(pTangents, vertices, 4, "tangents", where, why) ||
      !RunIsStatable(pColours, vertices, 4, "vertex colours", where, why)) {
    return std::unexpected(why);
  }

  for (size_t at = 0; at < pColours.size(); ++at) {
    if (pColours[at] >= 0.0f && pColours[at] <= 1.0f) { continue; }
    return std::unexpected(where + " states a vertex colour component of " +
                           std::to_string(pColours[at]) + " at " + std::to_string(at) +
                           ", and the format requires every component in [0, 1]");
  }

  for (const uint32_t local : pIndices) {
    if (local >= vertices) {
      return std::unexpected(where + " addresses vertex " + std::to_string(local) + " of its own " +
                             std::to_string(vertices));
    }
  }
  return vertices;
}

void Subject::AssemblePartInto(const outshine::Geometry &what,
                               int slot,
                               const Part &part,
                               size_t wholeFloats) {
  const std::span<const float> pPos = what.positionsOf(slot);
  const std::span<const float> pNormals = what.normalsOf(slot);
  const std::span<const float> pUv = what.textureOf(slot, Geometry::UvSet::Uv0);
  const std::span<const float> pUv1 = what.textureOf(slot, Geometry::UvSet::Uv1);
  const std::span<const float> pTangents = what.tangentsOf(slot);
  const std::span<const float> pColours = what.coloursOf(slot);
  const std::span<const uint32_t> pIndices = what.trianglesOf(slot);
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
    Indices_.push_back(static_cast<uint32_t>(part.FirstVertex) + local);
  }
  ApplyPartPlacement(what.placementOf(slot), part);
}

void Subject::ApplyPartPlacement(const Mat4 &placement, const Part &part) {
  if (placement == Mat4{}) { return; }
  const Vec3 x = {{placement[0], placement[1], placement[2]}};
  const Vec3 y = {{placement[4], placement[5], placement[6]}};
  const Vec3 z = {{placement[8], placement[9], placement[10]}};
  const Vec3 nx = Cross(y, z);
  const Vec3 ny = Cross(z, x);
  const Vec3 nz = Cross(x, y);
  const double sign = Dot(x, nx) < 0 ? -1.0 : 1.0;
  for (size_t vertex = part.FirstVertex; vertex < part.FirstVertex + part.VertexCount; ++vertex) {
    const size_t at = vertex * 3;
    const Vec3 local = {{Positions_[at], Positions_[at + 1], Positions_[at + 2]}};
    const Vec3 placed = placement.TransformPoint(local);
    for (size_t axis = 0; axis < 3; ++axis) { Positions_[at + axis] = placed[axis]; }
    if (part.HasNormal) {
      Vec3 normal = (nx * Normals_[at] + ny * Normals_[at + 1] + nz * Normals_[at + 2]) * sign;
      (void)Normalise(normal);
      for (size_t axis = 0; axis < 3; ++axis) { Normals_[at + axis] = normal[axis]; }
    }
    if (part.HasTangent()) {
      const size_t along = vertex * 4;
      Vec3 tangent = placement.TransformDirection(
          {{Tangents_[along], Tangents_[along + 1], Tangents_[along + 2]}});
      (void)Normalise(tangent);
      for (size_t axis = 0; axis < 3; ++axis) { Tangents_[along + axis] = tangent[axis]; }
      Tangents_[along + 3] *= sign;
    }
  }
  if (sign < 0) {
    for (size_t at = part.FirstIndex; at < Indices_.size(); at += 3) {
      std::swap(Indices_[at + 1], Indices_[at + 2]);
    }
  }
}

std::expected<size_t, std::string> Subject::ValidateAssembly(const outshine::Geometry &what) const {
  if (what.parts() == 0) { return std::unexpected("an assembly of no piece draws nothing"); }
  const size_t maximumVertices = std::min(Positions_.max_size() / 3, Tangents_.max_size() / 4);
  constexpr size_t kMaximumClonesPerCorner = 2;
  size_t projectedVertices = 0;
  const auto accountVertices = [&](size_t count) {
    if (count > maximumVertices - projectedVertices) { return false; }
    if (count > 0 && (projectedVertices > std::numeric_limits<uint32_t>::max() ||
                      count - 1 > std::numeric_limits<uint32_t>::max() - projectedVertices)) {
      return false;
    }
    projectedVertices += count;
    return true;
  };
  size_t wholeFloats = 0;
  size_t totalIndices = 0;
  for (int slot = 0; slot < what.parts(); ++slot) {
    const auto vertices = ValidatePart(what, slot);
    if (!vertices) { return std::unexpected(vertices.error()); }
    const size_t indices = what.trianglesOf(slot).size();
    if (indices > Indices_.max_size() - totalIndices ||
        indices > maximumVertices / kMaximumClonesPerCorner || !accountVertices(*vertices) ||
        !accountVertices(indices * kMaximumClonesPerCorner)) {
      return std::unexpected(Says::NativeCapacityFailed);
    }
    totalIndices += indices;
    wholeFloats += *vertices * 3;
  }
  return wholeFloats;
}

bool Subject::Assemble(const outshine::Geometry &what) {
  Subject candidate;
  if (!candidate.AssembleUnchecked(what)) {
    Error_ = std::move(candidate.Error_);
    return false;
  }
  PublishAssembly(std::move(candidate));
  return true;
}

bool Subject::AssembleUnchecked(const outshine::Geometry &what) {
  Error_.clear();
  const auto validated = ValidateAssembly(what);
  if (!validated) {
    Error_ = validated.error();
    return false;
  }
  const size_t wholeFloats = *validated;
  Images_.clear();
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
  SurfaceNames_.clear();
  TangentWanted_.clear();

  Images_.reserve(static_cast<size_t>(what.images()));
  for (int at = 0; at < what.images(); ++at) {
    const ImageView image = what.imageAt(at);
    Images_.push_back({.Width = image.WidthPx,
                       .Height = image.HeightPx,
                       .Rgba = {image.Rgba.begin(), image.Rgba.end()}});
  }
  for (int surface = 0; surface < what.surfaces(); ++surface) {
    Surfaces_.push_back(what.surfaceAt(MaterialInstance(surface)));
    SurfaceNames_.emplace_back(what.surfaceNameOf(surface));
    TangentWanted_.push_back(Surfaces_.back().NeedsTangents ? 1u : 0u);
  }
  AssembleLights(what);
  for (size_t index = 0; std::cmp_less(index, what.parts()); ++index) {
    const int slot = static_cast<int>(index);
    const std::span<const float> pNormals = what.normalsOf(slot);
    const std::span<const float> pUv = what.textureOf(slot, Geometry::UvSet::Uv0);
    const std::span<const float> pUv1 = what.textureOf(slot, Geometry::UvSet::Uv1);
    const std::span<const float> pTangents = what.tangentsOf(slot);
    const std::span<const float> pColours = what.coloursOf(slot);
    const std::span<const uint32_t> pIndices = what.trianglesOf(slot);
    const size_t vertices = what.positionsOf(slot).size() / 3;
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

    AssemblePartInto(what, slot, part, wholeFloats);
    part.IndexCount = Indices_.size() - part.FirstIndex;
    if (!FlatNormalsFor(part) || !GeneratedTangentsFor(part)) { return false; }
    part.VertexCount = VertexCount() - part.FirstVertex;
    Parts_.push_back(part);
  }

  Bound();
  return true;
}

void Subject::PublishAssembly(Subject &&candidate) noexcept {
  *this = std::move(candidate);
}

bool Subject::Append(const Subject &other) {
  if (other.Parts_.empty()) {
    Error_ = "a subject with no part appends nothing, and an empty append is a caller's mistake "
             "rather than a shape this can carry";
    return false;
  }
  Error_.clear();
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
  int beyond = static_cast<int>(Surfaces_.size());
  for (const Part &part : Parts_) {
    if (part.Material >= beyond) { beyond = part.Material + 1; }
  }
  const int imageBase = static_cast<int>(Images_.size());
  Images_.insert(Images_.end(), other.Images_.begin(), other.Images_.end());
  Surfaces_.resize(static_cast<size_t>(beyond));
  SurfaceNames_.resize(static_cast<size_t>(beyond));
  for (outshine::Material surface : other.Surfaces_) {
    for (SurfaceMap *map : {&surface.BaseColourMap,
                            &surface.NormalMap,
                            &surface.MetalRoughMap,
                            &surface.EmissiveMap,
                            &surface.OcclusionMap,
                            &surface.SpecularStrengthMap,
                            &surface.SpecularTintMap}) {
      if (map->bound()) { map->Image += imageBase; }
    }
    Surfaces_.push_back(surface);
  }
  SurfaceNames_.insert(SurfaceNames_.end(), other.SurfaceNames_.begin(), other.SurfaceNames_.end());
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

Box Subject::BoundsOf(size_t parts) const {
  const Box whole{.Min = Min_, .Max = Max_};
  if (parts == 0 || parts >= Parts_.size()) { return whole; }
  Box over;
  for (size_t at = 0; at < parts; ++at) {
    const Part &part = Parts_[at];
    for (size_t vertex = part.FirstVertex; vertex < part.FirstVertex + part.VertexCount; ++vertex) {
      over.Cover(
          Vec3{{Positions_[vertex * 3], Positions_[vertex * 3 + 1], Positions_[vertex * 3 + 2]}});
    }
  }
  return over.Empty() ? whole : over;
}

double Subject::RadiusM() const {
  const Vec3 span = Max_ - Min_;
  return 0.5 * Length(span);
}

void Subject::CentreM(Vec3 &out) const {
  out = (Min_ + Max_) * 0.5;
}

bool Subject::Frame(outshine::Camera &out, double fill, double aspect) const {
  const auto framed = FrameCamera({.Min = Min_, .Max = Max_}, {.Fill = fill, .Aspect = aspect});
  if (!framed) { return false; }
  out = *framed;
  return true;
}

bool DeclaredPlacement(const Document &document,
                       int cameraIndex,
                       outshine::Camera &out,
                       std::string &error,
                       std::span<const AffineTransform> locals) {
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
  AffineTransform world;
  const bool transformed = locals.empty()
                               ? document.WorldTransform(static_cast<int>(holder), world)
                               : document.WorldTransform(static_cast<int>(holder), locals, world);
  if (!transformed) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its world transform does not resolve";
    return false;
  }
  Vec3 up;
  Vec3 forward;
  for (int axis = 0; axis < 3; ++axis) {
    up[axis] = world.M[4 + axis];
    forward[axis] = -world.M[8 + axis];
    out.PositionM[axis] = world.M[12 + axis];
  }
  if (!Normalise(up) || !Normalise(forward)) {
    error = document.Path() + ": node " + std::to_string(holder) + " carries camera " +
            std::to_string(cameraIndex) + " and its basis has collapsed";
    return false;
  }
  out.LooksAt = true;
  out.LookAtM = out.PositionM + forward;
  out.UpM = up;
  if (lens.Kind == CameraKind::Orthographic) {
    out.setProjection(outshine::Camera::Ortho{
        .XMagM = lens.XMagM, .YMagM = lens.YMagM, .NearM = lens.ZNearM, .FarM = lens.ZFarM});
  } else {
    out.setProjection(outshine::Camera::Perspective{
        .FovDeg = lens.YfovRad * kRad2Deg, .NearM = lens.ZNearM, .FarM = lens.ZFarM});
  }
  return true;
}

double Subject::ProjectedAreaPx(const Mat4 &clip, const Viewport &viewport) const {
  double total = 0;
  for (size_t triangle = 0; triangle * 3 + 2 < Indices_.size(); ++triangle) {
    std::array<Vec2, 3> raster;
    for (int corner = 0; corner < 3; ++corner) {
      const size_t vertex = Indices_[triangle * 3 + static_cast<size_t>(corner)];
      const Vec3 point = {
          {Positions_[vertex * 3], Positions_[vertex * 3 + 1], Positions_[vertex * 3 + 2]}};
      const double w = clip[3] * point[0] + clip[7] * point[1] + clip[11] * point[2] + clip[15];
      const double reciprocalW = (w != 0.0) ? 1.0 / w : 1.0;
      Vec3 ndc;
      for (size_t axis = 0; axis < 3; ++axis) {
        ndc[axis] = (clip[axis] * point[0] + clip[4 + axis] * point[1] + clip[8 + axis] * point[2] +
                     clip[12 + axis]) *
                    reciprocalW;
      }
      viewport.Raster(ndc, raster[corner]);
    }
    total += 0.5 * std::fabs((raster[1][0] - raster[0][0]) * (raster[2][1] - raster[0][1]) -
                             (raster[2][0] - raster[0][0]) * (raster[1][1] - raster[0][1]));
  }
  return total;
}

}
