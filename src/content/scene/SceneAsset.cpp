#include "SceneAsset.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace outshine {

void SceneAsset::Adopt(std::vector<SceneNodeAsset> &&nodes,
                       std::vector<uint32_t> &&roots,
                       std::vector<SceneLightAsset> &&lights,
                       std::vector<SceneCameraAsset> &&cameras,
                       size_t morphWeights) {
  Nodes_ = std::move(nodes);
  Roots_ = std::move(roots);
  Lights_ = std::move(lights);
  Cameras_ = std::move(cameras);
  MorphWeights_ = morphWeights;
}

const SceneLightAsset *SceneAsset::Light(size_t index) const noexcept {
  return index < Lights_.size() ? &Lights_[index] : nullptr;
}

std::expected<Camera, SceneCameraError>
SceneAsset::PlacedCamera(size_t index, std::span<const AffineTransform> pose) const {
  if (index >= Cameras_.size()) { return std::unexpected(SceneCameraError::MissingCamera); }
  size_t holder = 0;
  size_t holders = 0;
  for (size_t node = 0; node < Nodes_.size(); ++node) {
    if (!std::cmp_equal(Nodes_[node].Camera, index)) { continue; }
    holder = node;
    ++holders;
  }
  if (holders == 0) { return std::unexpected(SceneCameraError::UnplacedCamera); }
  if (holders != 1) { return std::unexpected(SceneCameraError::MultiplyPlacedCamera); }
  AffineTransform world;
  if (!WorldTransform(holder, pose, world)) {
    return std::unexpected(SceneCameraError::InvalidPose);
  }
  Camera camera = Cameras_[index].Lens;
  Vec3 up;
  Vec3 forward;
  for (int axis = 0; axis < 3; ++axis) {
    up[axis] = world.M[4 + axis];
    forward[axis] = -world.M[8 + axis];
    camera.PositionM[axis] = world.M[12 + axis];
  }
  if (!Normalise(up) || !Normalise(forward)) {
    return std::unexpected(SceneCameraError::InvalidPose);
  }
  camera.LooksAt = true;
  camera.LookAtM = camera.PositionM + forward;
  camera.UpM = up;
  return camera;
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
