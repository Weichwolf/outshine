#ifndef OUTSHINE_CONTENT_SCENE_SCENEGEOMETRY_H
#define OUTSHINE_CONTENT_SCENE_SCENEGEOMETRY_H

#include <cstdint>
#include <scene/Geometry.h>

namespace outshine {

class SceneGeometry {
public:
  void Clear();
  void SetGeometry(Geometry &&geometry);

  [[nodiscard]] uint64_t Revision() const noexcept { return Revision_; }

  [[nodiscard]] bool HasGeometry() const noexcept { return HasGeometry_; }

  [[nodiscard]] const Geometry &Snapshot() const noexcept { return Geometry_; }

private:
  Geometry Geometry_;
  uint64_t Revision_ = 0;
  bool HasGeometry_ = false;
};

}
#endif
