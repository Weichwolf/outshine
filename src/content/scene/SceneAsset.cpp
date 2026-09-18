#include "SceneAsset.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace outshine {

void SceneAsset::Adopt(std::vector<SceneNodeAsset> &&nodes) {
  Nodes_ = std::move(nodes);
}

const SceneNodeAsset *SceneAsset::Node(std::size_t index) const noexcept {
  return index < Nodes_.size() ? &Nodes_[index] : nullptr;
}

}
