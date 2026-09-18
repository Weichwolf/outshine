#include "SceneImport.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Document.h"

namespace outshine::Gltf {
namespace {

struct InstanceChannel {
  int Accessor = -1;
  size_t Components = 0;
  std::vector<double> Values;
};

bool ReadChannel(const Document &document,
                 InstanceChannel &channel,
                 size_t &instances,
                 std::string &error) {
  if (channel.Accessor < 0) { return true; }
  if (!document.ReadElements(channel.Accessor, channel.Values)) {
    error = document.Path() + ": instance attribute does not decode: " + document.Error();
    return false;
  }
  if (channel.Values.size() % channel.Components != 0) {
    error = document.Path() + ": instance attribute has an incomplete tuple";
    return false;
  }
  for (size_t component = 0; component < channel.Values.size(); ++component) {
    if (!std::isfinite(channel.Values[component])) {
      error = document.Path() + ": instance attribute component " + std::to_string(component) +
              " is not finite";
      return false;
    }
  }
  const size_t count = channel.Values.size() / channel.Components;
  if (instances != 0 && count != instances) {
    error = document.Path() + ": instance attributes have different lengths";
    return false;
  }
  instances = count;
  return true;
}

bool ImportInstances(const Document &document,
                     const Node &node,
                     std::vector<AffineTransform> &out,
                     std::string &error) {
  std::array<InstanceChannel, 3> channels = {
      {{.Accessor = node.InstanceTranslation, .Components = 3, .Values = {}},
       {.Accessor = node.InstanceRotation, .Components = 4, .Values = {}},
       {.Accessor = node.InstanceScale, .Components = 3, .Values = {}}}};
  size_t instances = 0;
  for (InstanceChannel &channel : channels) {
    if (!ReadChannel(document, channel, instances, error)) { return false; }
  }
  if (instances == 0) { return true; }
  const auto held =
      [](const InstanceChannel &channel, size_t instance, size_t component, double fallback) {
        return channel.Values.empty() ? fallback
                                      : channel.Values[instance * channel.Components + component];
      };
  out.reserve(instances);
  for (size_t instance = 0; instance < instances; ++instance) {
    const Vec3 translation = {{held(channels[0], instance, 0, 0.0),
                               held(channels[0], instance, 1, 0.0),
                               held(channels[0], instance, 2, 0.0)}};
    const Quat rotation = {.X = held(channels[1], instance, 0, 0.0),
                           .Y = held(channels[1], instance, 1, 0.0),
                           .Z = held(channels[1], instance, 2, 0.0),
                           .W = held(channels[1], instance, 3, 1.0)};
    const Vec3 scale = {{held(channels[2], instance, 0, 1.0),
                         held(channels[2], instance, 1, 1.0),
                         held(channels[2], instance, 2, 1.0)}};
    out.push_back(AffineTransform::FromTrs(translation, rotation, scale));
  }
  return true;
}

}

bool ImportSceneAsset(const Document &document, SceneAsset &out, std::string &error) {
  std::vector<SceneNodeAsset> nodes;
  nodes.reserve(document.Nodes().size());
  for (size_t nodeIndex = 0; nodeIndex < document.Nodes().size(); ++nodeIndex) {
    const Node &node = document.Nodes()[nodeIndex];
    SceneNodeAsset native;
    native.Name = node.Name;
    native.Mesh = node.Mesh;
    native.Skin = node.Skin;
    native.Light = node.Light;
    native.Visible = node.Visible;
    native.RestLocal = node.HasMatrix
                           ? AffineTransform::FromColumnMajor(node.Matrix)
                           : AffineTransform::FromTrs(node.Translation, node.Rotation, node.Scale);
    native.MorphWeightFirst = document.MorphWeightsFirst(nodeIndex);
    const size_t morphCount = document.MorphWeightsCount(nodeIndex);
    if (morphCount > 0) {
      const std::vector<double> &declared =
          document.Meshes()[static_cast<size_t>(node.Mesh)].Weights;
      native.RestMorphWeights.reserve(morphCount);
      for (size_t at = 0; at < morphCount; ++at) {
        native.RestMorphWeights.push_back(at < declared.size() ? declared[at] : 0.0);
      }
    }
    native.Children.reserve(node.Children.size());
    for (const int child : node.Children) {
      native.Children.push_back(static_cast<uint32_t>(child));
    }
    if (!ImportInstances(document, node, native.Instances, error)) { return false; }
    nodes.push_back(std::move(native));
  }
  for (size_t parent = 0; parent < nodes.size(); ++parent) {
    for (const uint32_t child : nodes[parent].Children) {
      nodes[child].Parent = static_cast<int>(parent);
    }
  }
  const int defaultScene = document.DefaultScene();
  if (defaultScene < 0 || static_cast<size_t>(defaultScene) >= document.Scenes().size()) {
    error = document.Path() + ": no default scene to import";
    return false;
  }
  std::vector<uint32_t> roots;
  for (const int root : document.Scenes()[static_cast<size_t>(defaultScene)].Roots) {
    roots.push_back(static_cast<uint32_t>(root));
  }
  out.Adopt(std::move(nodes), std::move(roots), document.MorphWeightsTotal());
  error.clear();
  return true;
}

}
