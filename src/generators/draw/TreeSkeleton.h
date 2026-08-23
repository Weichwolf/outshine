#ifndef OUTSHINE_GENERATORS_DRAW_TREESKELETON_H
#define OUTSHINE_GENERATORS_DRAW_TREESKELETON_H

#include <cstdint>
#include <vector>

#include "TreeVec3.h"

namespace outshine::Generators {

struct LeafPoint {
  TreeVec3 Pos, Dir;
};

enum class RingCap : uint8_t { Base, Point, Cut, Broken };

class TreeSkeleton {
public:

  struct Node {
    TreeVec3 Pos, Dir, Up;
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

  TreeVec3 BoxMin, BoxMax;

  float FootRadius = 0.0f;
  float DbhRadius = 0.0f;

  uint32_t Seed = 1;

  void Clear() {
    Nodes.clear();
    Shoots.clear();
    LeafPoints.clear();
    BoxMin = TreeVec3{};
    BoxMax = TreeVec3{};
    FootRadius = 0.0f;
    DbhRadius = 0.0f;
  }
};

}
#endif
