#ifndef OUTSHINE_GENERATORS_DRAW_TREEMESHER_H
#define OUTSHINE_GENERATORS_DRAW_TREEMESHER_H

#include <cstdint>
#include <vector>

#include "TreeMesh.h"
#include "TreeSkeleton.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeMesher {
public:
  void Draw(const TreeSkeleton &plant, float pixelHeightFrac, TreeMesh &out);

private:
  static constexpr int kMaxSides = 16;

  struct Face {
    int A, B, C, D;
  };

  struct Band {
    int First = -1;
    int Sides = 0;
  };

  int AddVert(TreeVec3 p);
  int AddFace(int a, int b, int c, int d);
  [[nodiscard]] TreeVec3 FaceCentroid(int fi) const;

  [[nodiscard]] int SidesFor(float radius, int declared) const;

  void RingsOf(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot, int from);
  [[nodiscard]] bool ChordHolds(const TreeSkeleton &plant, int from, int last, int stride) const;
  void Ring(const TreeSkeleton::Node &node, float radius, int sides, int *out);
  void Wall(const int *from, const int *to, int sides);
  static void BreakProfile(uint32_t seed, int sides, float *out);
  void Cap(const TreeSkeleton::Node &node, const int *ring, int sides, RingCap cap, uint32_t seed);

  [[nodiscard]] bool Collar(int face,
                            const TreeSkeleton::Node &anchor,
                            const TreeSkeleton::Node &first,
                            int sides,
                            float room,
                            int *out);
  [[nodiscard]] static float RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot);
  void Export(TreeMesh &out);

  float PixelGrow_ = 0.0f;
  std::vector<TreeVec3> Verts_;
  std::vector<Face> Faces_;
  std::vector<uint8_t> Dead_;
  std::vector<uint8_t> Drawn_;
  std::vector<Band> Bands_;
  std::vector<int> Stations_;
  std::vector<TreeVec3> Normals_;
};

} // namespace outshine::Generators
#endif
