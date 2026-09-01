#ifndef OUTSHINE_GENERATORS_DRAW_TREESKELETON_H
#define OUTSHINE_GENERATORS_DRAW_TREESKELETON_H

#include <cstdint>
#include <vector>

#include "Vec3.h"

namespace outshine::Generators {

struct LeafPoint {
  Vec3f Pos, Dir;
};

enum class RingCap : uint8_t { Base, Point, Cut, Broken };

class TreeSkeleton {
public:
  struct Node {
    Vec3f Pos, Dir, Up;
    float Radius = 0.0f;
  };

  struct Shoot {
    int Parent = -1;
    int ParentNode = -1;
    float Roll = 0.0f;
    int First = 0, Count = 0;
    int Sides = 3;
    RingCap End = RingCap::Point;

    float Reach = 0.0f;
  };

  std::vector<Node> Nodes;
  std::vector<Shoot> Shoots;
  std::vector<LeafPoint> LeafPoints;

  Vec3f BoxMin, BoxMax;

  float FootRadius = 0.0f;
  float DbhRadius = 0.0f;

  uint32_t Seed = 1;

  void Clear() {
    Nodes.clear();
    Shoots.clear();
    LeafPoints.clear();
    BoxMin = Vec3f{};
    BoxMax = Vec3f{};
    FootRadius = 0.0f;
    DbhRadius = 0.0f;
  }
};

} // namespace outshine::Generators
#endif
