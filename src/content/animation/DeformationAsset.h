#ifndef OUTSHINE_CONTENT_ANIMATION_DEFORMATIONASSET_H
#define OUTSHINE_CONTENT_ANIMATION_DEFORMATIONASSET_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace outshine {

struct VertexSkinBinding {
  std::vector<uint32_t> Joints;
  std::vector<double> Weights;
  size_t Sets = 0;
  size_t Vertices = 0;

  [[nodiscard]] bool Empty() const { return Sets == 0; }
};

struct MorphTargetDelta {
  std::vector<double> Positions;
  std::vector<double> Normals;
  std::vector<double> Tangents;
};

struct PrimitiveDeformation {
  VertexSkinBinding Skin;
  std::vector<MorphTargetDelta> MorphTargets;
};

struct MeshDeformation {
  std::vector<PrimitiveDeformation> Primitives;
};

class DeformationAsset {
public:
  void Adopt(std::vector<MeshDeformation> &&meshes);
  [[nodiscard]] const PrimitiveDeformation *Find(size_t mesh, size_t primitive) const noexcept;

  [[nodiscard]] size_t MeshCount() const { return Meshes_.size(); }

private:
  std::vector<MeshDeformation> Meshes_;
};

}
#endif
