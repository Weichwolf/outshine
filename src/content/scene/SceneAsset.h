#ifndef OUTSHINE_CONTENT_SCENE_SCENEASSET_H
#define OUTSHINE_CONTENT_SCENE_SCENEASSET_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "AffineTransform.h"
#include "scene/PunctualLight.h"

namespace outshine {

struct SceneNodeAsset {
  std::string Name;
  std::vector<uint32_t> Children;
  std::vector<AffineTransform> Instances;
  std::vector<double> RestMorphWeights;
  AffineTransform RestLocal;
  int Parent = -1;
  int Mesh = -1;
  int Skin = -1;
  int Light = -1;
  size_t MorphWeightFirst = 0;
  bool Visible = true;
};

struct SceneLightAsset {
  std::string Name;
  PunctualLight Light;
};

class SceneAsset {
public:
  void Adopt(std::vector<SceneNodeAsset> &&nodes,
             std::vector<uint32_t> &&roots,
             std::vector<SceneLightAsset> &&lights,
             size_t morphWeights);
  [[nodiscard]] const SceneNodeAsset *Node(size_t index) const noexcept;
  [[nodiscard]] const SceneLightAsset *Light(size_t index) const noexcept;

  [[nodiscard]] std::span<const uint32_t> Roots() const noexcept { return Roots_; }

  [[nodiscard]] bool
  WorldTransform(size_t node, std::span<const AffineTransform> pose, AffineTransform &out) const;

  [[nodiscard]] size_t NodeCount() const noexcept { return Nodes_.size(); }

  [[nodiscard]] size_t LightCount() const noexcept { return Lights_.size(); }

  [[nodiscard]] size_t MorphWeightCount() const noexcept { return MorphWeights_; }

private:
  std::vector<SceneNodeAsset> Nodes_;
  std::vector<uint32_t> Roots_;
  std::vector<SceneLightAsset> Lights_;
  size_t MorphWeights_ = 0;
};

}
#endif
