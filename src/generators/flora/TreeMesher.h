#ifndef OUTSHINE_GENERATORS_FLORA_TREEMESHER_H
#define OUTSHINE_GENERATORS_FLORA_TREEMESHER_H

#include <span>
#include <cstdint>
#include <vector>

#include "TreeMesh.h"
#include "TreeSkeleton.h"
#include "math/Vec3.h"

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

  int AddVert(Vec3f p);
  int AddFace(int a, int b, int c, int d);
  [[nodiscard]] Vec3f FaceCentroid(int fi) const;

  struct Sided {
    float RadiusM = 0.0f;
    int Declared = 0;
  };

  [[nodiscard]] int SidesFor(Sided of) const;

  void RingsOf(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot, int from);
  [[nodiscard]] bool ChordHolds(const TreeSkeleton &plant, int from, int last, int stride) const;
  void Ring(const TreeSkeleton::Node &node, float radius, int sides, std::span<int> out);
  void Wall(std::span<const int> from, std::span<const int> to, int sides);

  struct Splintered {
    uint32_t Seed = 0;
    int Sides = 0;
  };

  static void BreakProfile(Splintered of, std::span<float> out);
  void Cap(const TreeSkeleton::Node &node,
           std::span<const int> ring,
           int sides,
           RingCap cap,
           uint32_t seed);

  struct Fitted {
    int Sides = 0;
    float RoomM = 0.0f;
  };

  [[nodiscard]] bool Collar(int face,
                            const TreeSkeleton::Node &anchor,
                            const TreeSkeleton::Node &first,
                            Fitted within,
                            std::span<int> out);
  [[nodiscard]] static float RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot);
  void Export(TreeMesh &out);

  float PixelGrow_ = 0.0f;
  std::vector<Vec3f> Verts_;
  std::vector<Face> Faces_;
  std::vector<uint8_t> Dead_;
  std::vector<uint8_t> Drawn_;
  std::vector<Band> Bands_;
  std::vector<int> Stations_;
  std::vector<Vec3f> Normals_;
};

} // namespace outshine::Generators
#endif
