#ifndef OUTSHINE_CONTENT_SCENE_SCENEGEOMETRY_H
#define OUTSHINE_CONTENT_SCENE_SCENEGEOMETRY_H

#include <cstdint>
#include <memory>
#include <scene/Geometry.h>

namespace outshine {

class SceneGeometry {
public:
  SceneGeometry() = default;
  SceneGeometry(const SceneGeometry &) = delete;
  SceneGeometry &operator=(const SceneGeometry &) = delete;
  SceneGeometry(SceneGeometry &&) noexcept = default;
  SceneGeometry &operator=(SceneGeometry &&) noexcept = default;

  void Clear();
  void SetGeometry(Geometry &&geometry);
  void SetGeometry(std::shared_ptr<const Geometry> geometry);

  [[nodiscard]] uint64_t Revision() const noexcept { return Revision_; }

  [[nodiscard]] bool HasGeometry() const noexcept { return Geometry_ != nullptr; }

  [[nodiscard]] const Geometry &Snapshot() const noexcept;

  [[nodiscard]] std::shared_ptr<const Geometry> Retain() const noexcept { return Geometry_; }

private:
  std::shared_ptr<const Geometry> Geometry_;
  uint64_t Revision_ = 0;
};

}
#endif
