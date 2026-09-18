#include "MeshImport.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "Document.h"

namespace outshine::Gltf {

namespace {

bool ImportFloatAttribute(const Document &document,
                          int accessor,
                          const char *semantic,
                          size_t components,
                          size_t vertices,
                          std::vector<float> &out,
                          std::string &error) {
  std::vector<double> decoded;
  if (!document.ReadElements(accessor, decoded)) {
    error = document.Path() + ": " + semantic + " does not decode: " + document.Error();
    return false;
  }
  if (decoded.size() != vertices * components) {
    error = std::format("{}: {} has {} components for {} vertices instead of VEC{}",
                        document.Path(),
                        semantic,
                        decoded.size(),
                        vertices,
                        components);
    return false;
  }
  out.reserve(decoded.size());
  for (size_t component = 0; component < decoded.size(); ++component) {
    const double value = decoded[component];
    if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max()) {
      error = std::format("{}: {} component {} cannot be represented as finite float",
                          document.Path(),
                          semantic,
                          component);
      return false;
    }
    out.push_back(static_cast<float>(value));
  }
  return true;
}

struct SkinSet {
  size_t Index = 0;
  size_t Vertices = 0;
};

bool ImportSkinSet(const Document &document,
                   const Primitive &primitive,
                   SkinSet set,
                   VertexSkinBinding &out,
                   bool &found,
                   std::string &error) {
  const std::string which = std::to_string(set.Index);
  const int joints = primitive.Find(("JOINTS_" + which).c_str());
  const int weights = primitive.Find(("WEIGHTS_" + which).c_str());
  found = joints >= 0 || weights >= 0;
  if (!found) { return true; }
  if (joints < 0 || weights < 0) {
    error = std::format("{}: a primitive carries {}{} and {}{}, and a skin set requires both",
                        document.Path(),
                        joints < 0 ? "no JOINTS_" : "JOINTS_",
                        which,
                        weights < 0 ? "no WEIGHTS_" : "WEIGHTS_",
                        which);
    return false;
  }
  std::vector<double> decodedJoints;
  std::vector<double> decodedWeights;
  if (!document.ReadElements(joints, decodedJoints) ||
      !document.ReadElements(weights, decodedWeights)) {
    error =
        document.Path() + ": skin binding set " + which + " does not decode: " + document.Error();
    return false;
  }
  if (decodedJoints.size() != set.Vertices * 4 || decodedWeights.size() != set.Vertices * 4) {
    error = std::format("{}: JOINTS_{} and WEIGHTS_{} do not each bind {} vertices as VEC4",
                        document.Path(),
                        which,
                        which,
                        set.Vertices);
    return false;
  }
  for (size_t at = 0; at < decodedJoints.size(); ++at) {
    const double joint = decodedJoints[at];
    const double weight = decodedWeights[at];
    if (!std::isfinite(joint) || joint < 0 || std::trunc(joint) != joint ||
        joint > std::numeric_limits<uint32_t>::max() || !std::isfinite(weight) || weight < 0 ||
        weight > std::numeric_limits<float>::max()) {
      error = document.Path() + ": skin binding set " + which +
              " contains an invalid joint or weight at component " + std::to_string(at);
      return false;
    }
    out.Joints.push_back(static_cast<uint32_t>(joint));
    out.Weights.push_back(static_cast<float>(weight));
  }
  ++out.Sets;
  return true;
}

bool ImportSkin(const Document &document,
                const Primitive &primitive,
                size_t vertices,
                VertexSkinBinding &out,
                std::string &error) {
  for (size_t set = 0;; ++set) {
    bool found = false;
    if (!ImportSkinSet(
            document, primitive, {.Index = set, .Vertices = vertices}, out, found, error)) {
      return false;
    }
    if (!found) { break; }
  }
  out.Vertices = vertices;
  return true;
}

bool ImportMorphAttribute(const Document &document,
                          const MorphTarget &target,
                          const char *semantic,
                          size_t vertices,
                          std::vector<float> &out,
                          std::string &error) {
  const int accessor = target.Find(semantic);
  if (accessor < 0) { return true; }
  const std::string named = std::string("morph ") + semantic;
  return ImportFloatAttribute(document, accessor, named.c_str(), 3, vertices, out, error);
}

bool ImportMorphTargets(const Document &document,
                        const Primitive &primitive,
                        size_t vertices,
                        std::vector<MorphTargetDelta> &out,
                        std::string &error) {
  out.reserve(primitive.Targets.size());
  for (const MorphTarget &target : primitive.Targets) {
    MorphTargetDelta nativeTarget;
    if (!ImportMorphAttribute(
            document, target, "POSITION", vertices, nativeTarget.Positions, error) ||
        !ImportMorphAttribute(document, target, "NORMAL", vertices, nativeTarget.Normals, error) ||
        !ImportMorphAttribute(
            document, target, "TANGENT", vertices, nativeTarget.Tangents, error)) {
      return false;
    }
    out.push_back(std::move(nativeTarget));
  }
  return true;
}

bool ImportPrimitive(const Document &document,
                     const Primitive &primitive,
                     MeshPrimitive &out,
                     std::string &error) {
  const int position = primitive.Find("POSITION");
  size_t vertices = 0;
  if (position >= 0) {
    if (static_cast<size_t>(position) >= document.Accessors().size()) {
      error = document.Path() + ": primitive POSITION accessor is absent";
      return false;
    }
    vertices = document.Accessors()[static_cast<size_t>(position)].Count;
    if (!ImportFloatAttribute(document, position, "POSITION", 3, vertices, out.Positions, error)) {
      return false;
    }
  }
  const int normal = primitive.Find("NORMAL");
  if (normal >= 0 &&
      !ImportFloatAttribute(document, normal, "NORMAL", 3, vertices, out.Normals, error)) {
    return false;
  }
  return ImportSkin(document, primitive, vertices, out.Skin, error) &&
         ImportMorphTargets(document, primitive, vertices, out.MorphTargets, error);
}

bool ValidateSkinnedNodes(const Document &document,
                          const std::vector<MeshAsset> &meshes,
                          std::string &error) {
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &declared = document.Nodes()[node];
    if (declared.Skin < 0) { continue; }
    const auto mesh = static_cast<size_t>(declared.Mesh);
    for (size_t primitive = 0; primitive < meshes[mesh].Primitives.size(); ++primitive) {
      if (!meshes[mesh].Primitives[primitive].Skin.Empty()) { continue; }
      error = document.Path() + ": skinned node " + std::to_string(node) + " has primitive " +
              std::to_string(primitive) + " without joint and weight bindings";
      return false;
    }
  }
  return true;
}

}

bool ImportMeshAssets(const Document &document, MeshAssetSet &out, std::string &error) {
  std::vector<MeshAsset> meshes;
  meshes.reserve(document.Meshes().size());
  for (const Mesh &mesh : document.Meshes()) {
    MeshAsset nativeMesh;
    nativeMesh.Primitives.reserve(mesh.Primitives.size());
    for (const Primitive &primitive : mesh.Primitives) {
      MeshPrimitive nativePrimitive;
      if (!ImportPrimitive(document, primitive, nativePrimitive, error)) { return false; }
      nativeMesh.Primitives.push_back(std::move(nativePrimitive));
    }
    meshes.push_back(std::move(nativeMesh));
  }
  if (!ValidateSkinnedNodes(document, meshes, error)) { return false; }
  out.Adopt(std::move(meshes));
  error.clear();
  return true;
}

}
