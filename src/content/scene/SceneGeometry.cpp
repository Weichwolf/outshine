#include "SceneGeometry.h"
#include <utility>

namespace outshine {

void SceneGeometry::Clear() {
  Geometry_.clear();
  HasGeometry_ = false;
  ++Revision_;
}

void SceneGeometry::SetGeometry(Geometry &&geometry) {
  Geometry_ = std::move(geometry);
  HasGeometry_ = true;
  ++Revision_;
}

}
