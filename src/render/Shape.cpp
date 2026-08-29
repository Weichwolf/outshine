#include "Shape.h"

namespace outshine::Render {

void Shape::BoundsOf(size_t parts, double leastM[3], double mostM[3]) const {
  const auto fold = [this](size_t upTo, double least[3], double most[3]) {
    bool any = false;
    for (size_t part = 0; part < upTo && part < Parts.size(); ++part) {
      const ShapePart &one = Parts[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        for (int axis = 0; axis < 3; ++axis) {
          const double at = (double)one.PositionsM[vertex * 3 + (size_t)axis];
          if (!any || at < least[axis]) { least[axis] = at; }
          if (!any || at > most[axis]) { most[axis] = at; }
        }
        any = true;
      }
    }
    return any;
  };
  for (int axis = 0; axis < 3; ++axis) {
    leastM[axis] = 0.0;
    mostM[axis] = 0.0;
  }
  (void)fold(Parts.size(), leastM, mostM);
  if (parts == 0 || parts >= Parts.size()) { return; }
  double least[3], most[3];
  if (!fold(parts, least, most)) { return; }
  for (int axis = 0; axis < 3; ++axis) {
    leastM[axis] = least[axis];
    mostM[axis] = most[axis];
  }
}

}
