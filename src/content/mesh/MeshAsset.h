#ifndef OUTSHINE_CONTENT_MESH_MESHASSET_H
#define OUTSHINE_CONTENT_MESH_MESHASSET_H

#include <cstddef>
#include <cstdint>
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
};

struct MeshAsset {
  std::vector<MeshPrimitive> Primitives;
};

class MeshAssetSet {
public:
  void Adopt(std::vector<MeshAsset> &&meshes);
  [[nodiscard]] const MeshPrimitive *Find(size_t mesh, size_t primitive) const noexcept;

  [[nodiscard]] size_t MeshCount() const { return Meshes_.size(); }

private:
  std::vector<MeshAsset> Meshes_;
};

}
#endif
