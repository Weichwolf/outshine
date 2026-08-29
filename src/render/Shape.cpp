#include "Shape.h"

namespace outshine::Render {

void Shape::BoundsOf(size_t first, double leastM[3], double mostM[3]) const {
  for (int axis = 0; axis < 3; ++axis) {
    leastM[axis] = 1.0e300;
    mostM[axis] = -1.0e300;
  }
  for (size_t part = first; part < Parts.size(); ++part) {
    const ShapePart &one = Parts[part];
    for (size_t vertex = one.FirstVertex; vertex < one.FirstVertex + one.VertexCount; ++vertex) {
      for (int axis = 0; axis < 3; ++axis) {
        const double at = PositionsM[vertex * 3 + (size_t)axis];
        leastM[axis] = at < leastM[axis] ? at : leastM[axis];
        mostM[axis] = at > mostM[axis] ? at : mostM[axis];
      }
    }
  }
}

}
