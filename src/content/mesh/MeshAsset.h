#ifndef OUTSHINE_CONTENT_MESH_MESHASSET_H
#define OUTSHINE_CONTENT_MESH_MESHASSET_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct VertexSkinBinding {
  std::vector<uint32_t> Joints;
  std::vector<float> Weights;
  size_t Sets = 0;
  size_t Vertices = 0;

  [[nodiscard]] bool Empty() const { return Sets == 0; }
};

struct MorphTargetDelta {
  std::vector<float> Positions;
  std::vector<float> Normals;
  std::vector<float> Tangents;
};

struct MeshPrimitive {
  VertexSkinBinding Skin;
  std::vector<MorphTargetDelta> MorphTargets;
  std::vector<float> Positions;
  std::vector<float> Normals;
  std::vector<float> Tangents;
  std::vector<float> TextureCoordinates;
  std::vector<float> SecondaryTextureCoordinates;
  std::vector<float> Colours;
  std::vector<uint32_t> Triangles;
  int Material = -1;
  std::vector<int> VariantMaterials;

  [[nodiscard]] int MaterialFor(int variant) const noexcept;
};

struct MeshAsset {
  std::vector<MeshPrimitive> Primitives;
};

class MeshAssetSet {
public:
  void Adopt(std::vector<MeshAsset> &&meshes, std::vector<std::string> &&variantNames);
  [[nodiscard]] const MeshPrimitive *Find(size_t mesh, size_t primitive) const noexcept;
  [[nodiscard]] size_t PrimitiveCount(size_t mesh) const noexcept;

  [[nodiscard]] size_t MeshCount() const { return Meshes_.size(); }

  [[nodiscard]] std::optional<int> FindVariant(std::string_view name) const noexcept;

  [[nodiscard]] bool AcceptsVariant(int variant) const noexcept;

  [[nodiscard]] std::span<const std::string> Variants() const noexcept { return VariantNames_; }

private:
  std::vector<MeshAsset> Meshes_;
  std::vector<std::string> VariantNames_;
};

}
#endif
