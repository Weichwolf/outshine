#include "SceneGeometry.h"
#include <memory>
#include <utility>

namespace outshine {

void SceneGeometry::Clear() {
  Geometry_.reset();
  ++Revision_;
}

void SceneGeometry::SetGeometry(Geometry &&geometry) {
  SetGeometry(std::make_shared<const Geometry>(std::move(geometry)));
}

void SceneGeometry::SetGeometry(std::shared_ptr<const Geometry> geometry) {
  Geometry_ = std::move(geometry);
  ++Revision_;
}

const Geometry &SceneGeometry::Snapshot() const noexcept {
  static const Geometry empty;
  return Geometry_ ? *Geometry_ : empty;
}

}
