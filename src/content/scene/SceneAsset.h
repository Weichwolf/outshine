#ifndef OUTSHINE_CONTENT_SCENE_SCENEASSET_H
#define OUTSHINE_CONTENT_SCENE_SCENEASSET_H

#include <cstddef>
#include <vector>

#include "AffineTransform.h"

namespace outshine {

struct SceneNodeAsset {
  std::vector<AffineTransform> Instances;
};

class SceneAsset {
public:
  void Adopt(std::vector<SceneNodeAsset> &&nodes);
  [[nodiscard]] const SceneNodeAsset *Node(size_t index) const noexcept;

  [[nodiscard]] size_t NodeCount() const noexcept { return Nodes_.size(); }

private:
  std::vector<SceneNodeAsset> Nodes_;
};

}
#endif
