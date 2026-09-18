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

bool ImportOptionalFloatAttribute(const Document &document,
                                  const Primitive &primitive,
                                  const char *semantic,
                                  size_t components,
                                  size_t vertices,
                                  std::vector<float> &out,
                                  std::string &error) {
  const int accessor = primitive.Find(semantic);
  return accessor < 0 ||
         ImportFloatAttribute(document, accessor, semantic, components, vertices, out, error);
}

bool DrawsSurface(PrimitiveMode mode) {
  return mode == PrimitiveMode::Triangles || mode == PrimitiveMode::TriangleStrip ||
         mode == PrimitiveMode::TriangleFan;
}

bool ImportTriangles(const Document &document,
                     const Primitive &primitive,
                     size_t vertices,
                     std::vector<uint32_t> &out,
                     std::string &error) {
  if (!DrawsSurface(primitive.Mode)) { return true; }
  std::vector<uint32_t> run;
  if (primitive.Indices >= 0) {
    if (!document.ReadIndices(primitive.Indices, run)) {
      error = document.Path() + ": primitive indices do not decode: " + document.Error();
      return false;
    }
  } else {
    run.resize(vertices);
    for (size_t vertex = 0; vertex < vertices; ++vertex) {
      run[vertex] = static_cast<uint32_t>(vertex);
    }
  }
  const bool whole = primitive.Mode == PrimitiveMode::Triangles
                         ? !run.empty() && run.size() % 3 == 0
                         : run.size() >= 3;
  if (!whole) {
    error = document.Path() + ": primitive index run cannot form complete triangles";
    return false;
  }
  for (const uint32_t index : run) {
    if (index >= vertices) {
      error = document.Path() + ": primitive index " + std::to_string(index) +
              " exceeds its vertex count " + std::to_string(vertices);
      return false;
    }
  }
  if (primitive.Mode == PrimitiveMode::Triangles) {
    out = std::move(run);
  } else if (primitive.Mode == PrimitiveMode::TriangleStrip) {
    out.reserve((run.size() - 2) * 3);
    for (size_t at = 0; at + 2 < run.size(); ++at) {
      const size_t flipped = at % 2;
      out.push_back(run[at + flipped]);
      out.push_back(run[at + 1 - flipped]);
      out.push_back(run[at + 2]);
    }
  } else {
    out.reserve((run.size() - 2) * 3);
    for (size_t at = 1; at + 1 < run.size(); ++at) {
      out.push_back(run[0]);
      out.push_back(run[at]);
      out.push_back(run[at + 1]);
    }
  }
  return true;
}

bool ImportColours(const Document &document,
                   const Primitive &primitive,
                   size_t vertices,
                   std::vector<float> &out,
                   std::string &error) {
  const int accessor = primitive.Find("COLOR_0");
  if (accessor < 0) { return true; }
  if (static_cast<size_t>(accessor) >= document.Accessors().size()) {
    error = document.Path() + ": COLOR_0 accessor is absent";
    return false;
  }
  size_t components = 0;
  std::string why;
  if (!VertexColourComponents(
          document.Accessors()[static_cast<size_t>(accessor)], components, why)) {
    error = document.Path() + ": COLOR_0 " + why;
    return false;
  }
  std::vector<float> decoded;
  if (!ImportFloatAttribute(document, accessor, "COLOR_0", components, vertices, decoded, error)) {
    return false;
  }
  out.reserve(vertices * 4);
  for (size_t vertex = 0; vertex < vertices; ++vertex) {
    for (size_t channel = 0; channel < 4; ++channel) {
      const float value = channel < components ? decoded[vertex * components + channel] : 1.0F;
      if (value < 0.0F || value > 1.0F) {
        error = document.Path() + ": COLOR_0 component lies outside [0, 1]";
        return false;
      }
      out.push_back(value);
    }
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
  out.Material = primitive.Material;
  out.VariantMaterials = primitive.VariantMaterials;
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
  if (!ImportOptionalFloatAttribute(
          document, primitive, "NORMAL", 3, vertices, out.Normals, error) ||
      !ImportOptionalFloatAttribute(
          document, primitive, "TANGENT", 4, vertices, out.Tangents, error) ||
      !ImportOptionalFloatAttribute(
          document, primitive, "TEXCOORD_0", 2, vertices, out.TextureCoordinates, error) ||
      !ImportOptionalFloatAttribute(
          document, primitive, "TEXCOORD_1", 2, vertices, out.SecondaryTextureCoordinates, error) ||
      !ImportColours(document, primitive, vertices, out.Colours, error) ||
      !ImportTriangles(document, primitive, vertices, out.Triangles, error)) {
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
  std::vector<std::string> variantNames = document.Variants();
  out.Adopt(std::move(meshes), std::move(variantNames));
  error.clear();
  return true;
}

}
