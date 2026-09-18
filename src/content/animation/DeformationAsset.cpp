#include "DeformationAsset.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace outshine {

void DeformationAsset::Adopt(std::vector<MeshDeformation> &&meshes) {
  Meshes_ = std::move(meshes);
}

const PrimitiveDeformation *DeformationAsset::Find(size_t mesh, size_t primitive) const noexcept {
  if (mesh >= Meshes_.size() || primitive >= Meshes_[mesh].Primitives.size()) { return nullptr; }
  return &Meshes_[mesh].Primitives[primitive];
}

}
