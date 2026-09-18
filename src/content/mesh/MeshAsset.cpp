#include "MeshAsset.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace outshine {

void MeshAssetSet::Adopt(std::vector<MeshAsset> &&meshes) {
  Meshes_ = std::move(meshes);
}

const MeshPrimitive *MeshAssetSet::Find(size_t mesh, size_t primitive) const noexcept {
  if (mesh >= Meshes_.size() || primitive >= Meshes_[mesh].Primitives.size()) { return nullptr; }
  return &Meshes_[mesh].Primitives[primitive];
}

}
