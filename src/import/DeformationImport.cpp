#include "DeformationImport.h"

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

bool ImportSkin(const Document &document,
                const Primitive &primitive,
                size_t vertices,
                VertexSkinBinding &out,
                std::string &error) {
  for (size_t set = 0;; ++set) {
    const std::string which = std::to_string(set);
    const int joints = primitive.Find(("JOINTS_" + which).c_str());
    const int weights = primitive.Find(("WEIGHTS_" + which).c_str());
    if (joints < 0 && weights < 0) { break; }
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
    if (decodedJoints.size() != vertices * 4 || decodedWeights.size() != vertices * 4) {
      error = std::format("{}: JOINTS_{} and WEIGHTS_{} do not each bind {} vertices as VEC4",
                          document.Path(),
                          which,
                          which,
                          vertices);
      return false;
    }
    for (size_t at = 0; at < decodedJoints.size(); ++at) {
      const double joint = decodedJoints[at];
      const double weight = decodedWeights[at];
      if (!std::isfinite(joint) || joint < 0 || std::trunc(joint) != joint ||
          joint > std::numeric_limits<uint32_t>::max() || !std::isfinite(weight) || weight < 0) {
        error = document.Path() + ": skin binding set " + which +
                " contains an invalid joint or weight at component " + std::to_string(at);
        return false;
      }
      out.Joints.push_back(static_cast<uint32_t>(joint));
      out.Weights.push_back(weight);
    }
    ++out.Sets;
  }
  out.Vertices = vertices;
  return true;
}

bool ImportMorphAttribute(const Document &document,
                          const MorphTarget &target,
                          const char *semantic,
                          size_t vertices,
                          std::vector<double> &out,
                          std::string &error) {
  const int accessor = target.Find(semantic);
  if (accessor < 0) { return true; }
  if (!document.ReadElements(accessor, out)) {
    error = document.Path() + ": morph " + semantic + " does not decode: " + document.Error();
    return false;
  }
  if (out.size() != vertices * 3) {
    error = std::format("{}: morph {} has {} components for {} vertices instead of VEC3",
                        document.Path(),
                        semantic,
                        out.size(),
                        vertices);
    return false;
  }
  for (size_t component = 0; component < out.size(); ++component) {
    if (!std::isfinite(out[component])) {
      error = std::format(
          "{}: morph {} component {} is not finite", document.Path(), semantic, component);
      return false;
    }
  }
  return true;
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

}

bool ImportDeformations(const Document &document, DeformationAsset &out, std::string &error) {
  std::vector<MeshDeformation> meshes;
  meshes.reserve(document.Meshes().size());
  for (const Mesh &mesh : document.Meshes()) {
    MeshDeformation nativeMesh;
    nativeMesh.Primitives.reserve(mesh.Primitives.size());
    for (const Primitive &primitive : mesh.Primitives) {
      PrimitiveDeformation nativePrimitive;
      const int position = primitive.Find("POSITION");
      size_t vertices = 0;
      if (position >= 0) {
        std::vector<double> positions;
        if (!document.ReadElements(position, positions) || positions.size() % 3 != 0) {
          error = document.Path() + ": primitive POSITION does not decode as VEC3";
          return false;
        }
        vertices = positions.size() / 3;
      }
      if (!ImportSkin(document, primitive, vertices, nativePrimitive.Skin, error)) { return false; }
      if (!ImportMorphTargets(document, primitive, vertices, nativePrimitive.MorphTargets, error)) {
        return false;
      }
      nativeMesh.Primitives.push_back(std::move(nativePrimitive));
    }
    meshes.push_back(std::move(nativeMesh));
  }
  for (size_t node = 0; node < document.Nodes().size(); ++node) {
    const Node &declared = document.Nodes()[node];
    if (declared.Skin < 0) { continue; }
    const auto mesh = static_cast<size_t>(declared.Mesh);
    for (size_t primitive = 0; primitive < document.Meshes()[mesh].Primitives.size(); ++primitive) {
      if (meshes[mesh].Primitives[primitive].Skin.Empty()) {
        error = document.Path() + ": skinned node " + std::to_string(node) + " has primitive " +
                std::to_string(primitive) + " without joint and weight bindings";
        return false;
      }
    }
  }
  out.Adopt(std::move(meshes));
  error.clear();
  return true;
}

}
