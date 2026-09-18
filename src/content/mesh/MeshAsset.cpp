#include "MeshAsset.h"

#include <cstddef>
#include <utility>
#include <vector>

namespace outshine {

int MeshPrimitive::MaterialFor(int variant) const noexcept {
  if (variant < 0 || static_cast<size_t>(variant) >= VariantMaterials.size()) { return Material; }
  const int selected = VariantMaterials[static_cast<size_t>(variant)];
  return selected < 0 ? Material : selected;
}

void MeshAssetSet::Adopt(std::vector<MeshAsset> &&meshes) {
  Meshes_ = std::move(meshes);
}

const MeshPrimitive *MeshAssetSet::Find(size_t mesh, size_t primitive) const noexcept {
  if (mesh >= Meshes_.size() || primitive >= Meshes_[mesh].Primitives.size()) { return nullptr; }
  return &Meshes_[mesh].Primitives[primitive];
}

size_t MeshAssetSet::PrimitiveCount(size_t mesh) const noexcept {
  return mesh < Meshes_.size() ? Meshes_[mesh].Primitives.size() : 0;
}

}
