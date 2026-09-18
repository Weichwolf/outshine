#include "SceneAsset.h"

#include <cstddef>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace outshine {

void SceneAsset::Adopt(std::vector<SceneNodeAsset> &&nodes,
                       std::vector<uint32_t> &&roots,
                       size_t morphWeights) {
  Nodes_ = std::move(nodes);
  Roots_ = std::move(roots);
  MorphWeights_ = morphWeights;
}

bool SceneAsset::WorldTransform(size_t node,
                                std::span<const AffineTransform> pose,
                                AffineTransform &out) const {
  if (node >= Nodes_.size() || (!pose.empty() && pose.size() != Nodes_.size())) { return false; }
  out = AffineTransform::Identity();
  std::vector<size_t> chain;
  for (int at = static_cast<int>(node), steps = 0; at >= 0; ++steps) {
    if (static_cast<size_t>(steps) > Nodes_.size()) { return false; }
    chain.push_back(static_cast<size_t>(at));
    at = Nodes_[static_cast<size_t>(at)].Parent;
  }
  for (const size_t at : std::views::reverse(chain)) {
    out = out * (pose.empty() ? Nodes_[at].RestLocal : pose[at]);
  }
  return true;
}

const SceneNodeAsset *SceneAsset::Node(std::size_t index) const noexcept {
  return index < Nodes_.size() ? &Nodes_[index] : nullptr;
}

}
