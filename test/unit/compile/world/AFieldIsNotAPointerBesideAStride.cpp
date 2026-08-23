#include "TerrainGrid.h"
// REFUSED: too many arguments to function call, expected 3, have 5
// board:1769: a field is two extents that travel together. Handing the data one way and the
// shape another is how a stride goes stale in one caller and not the other -- the tree's own
// "no second spelling of any truth", applied to the shape of a grid.
int main(void) {
  outshine::Ground::TerrainField field(3, 4);
  return (int)outshine::Ground::Bilinear(field.Data(), 4, 3, 1.0, 1.0);
}
