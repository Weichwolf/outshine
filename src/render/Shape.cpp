#include <algorithm>
#include "Shape.h"
#include "math/Vec3.h"

#include <cstddef>

namespace outshine::Render {

Box Shape::BoundsOf(size_t parts) const {
  const auto fold = [this](size_t upTo) {
    Box over;
    for (size_t part = 0; part < upTo && part < Parts.size(); ++part) {
      const ShapePart &one = Parts[part];
      for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
           ++vertex) {
        over.Cover(Vec3{{static_cast<double>(one.PositionsM[vertex * 3]),
                         static_cast<double>(one.PositionsM[vertex * 3 + 1]),
                         static_cast<double>(one.PositionsM[vertex * 3 + 2])}});
      }
    }
    return over;
  };
  Box whole = fold(Parts.size());
  if (whole.Empty()) { whole = Box{.Min = Vec3{}, .Max = Vec3{}}; }
  if (parts == 0 || parts >= Parts.size()) { return whole; }
  const Box some = fold(parts);
  return some.Empty() ? whole : some;
}

}
