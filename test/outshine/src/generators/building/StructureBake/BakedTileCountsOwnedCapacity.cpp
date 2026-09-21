#include "StructureBake.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Generators::BakedTile baked;
  baked.Built.WallCorners.reserve(2);
  baked.Walls.Clusters.reserve(3);
  baked.Roofs.Index.reserve(5);
  baked.Prints.reserve(7);
  baked.SeatSpreadM.reserve(11);
  baked.AcrossM.reserve(13);
  const size_t expected =
      baked.Built.HeapBytes() + baked.Walls.HeapBytes() + baked.Roofs.HeapBytes() +
      baked.Prints.capacity() * sizeof(Ground::BuildingField::Footprint) +
      baked.SeatSpreadM.capacity() * sizeof(double) + baked.AcrossM.capacity() * sizeof(double);
  CHECK(baked.HeapBytes() == expected, "baked structure products count each owned capacity once");
  return Report();
}
