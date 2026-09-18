#include "MeshAsset.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {

int MeshPrimitive::MaterialFor(int variant) const noexcept {
  if (variant < 0 || static_cast<size_t>(variant) >= VariantMaterials.size()) { return Material; }
  const int selected = VariantMaterials[static_cast<size_t>(variant)];
  return selected < 0 ? Material : selected;
}

void MeshAssetSet::Adopt(std::vector<MeshAsset> &&meshes, std::vector<std::string> &&variantNames) {
  Meshes_ = std::move(meshes);
  VariantNames_ = std::move(variantNames);
}

std::optional<int> MeshAssetSet::FindVariant(std::string_view name) const noexcept {
  for (size_t index = 0; index < VariantNames_.size(); ++index) {
    if (VariantNames_[index] == name) { return static_cast<int>(index); }
  }
  return std::nullopt;
}

bool MeshAssetSet::AcceptsVariant(int variant) const noexcept {
  return variant == -1 || (variant >= 0 && static_cast<size_t>(variant) < VariantNames_.size());
}

const MeshPrimitive *MeshAssetSet::Find(size_t mesh, size_t primitive) const noexcept {
  if (mesh >= Meshes_.size() || primitive >= Meshes_[mesh].Primitives.size()) { return nullptr; }
  return &Meshes_[mesh].Primitives[primitive];
}

size_t MeshAssetSet::PrimitiveCount(size_t mesh) const noexcept {
  return mesh < Meshes_.size() ? Meshes_[mesh].Primitives.size() : 0;
}

}
