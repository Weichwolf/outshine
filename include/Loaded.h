#ifndef OUTSHINE_LOADED_H
#define OUTSHINE_LOADED_H

#include <memory>
#include <span>
#include <string>
#include <string_view>

#include "Geometry.h"
#include "Scenario.h"

namespace outshine {

// AN ASSET A CLIENT CAN READ, WHICH THIS DOOR COULD NOT DO. `Engine::setGeometry` takes a
// `Geometry` and nothing handed one back from a FILE, so a client holding a glTF had exactly two
// options: declare it and never see what it contains, or reach past the door into `Gltf::`.
// Filament separates the two the same way and for the same reason -- `gltfio::AssetLoader` builds
// a `FilamentAsset` a client can walk, and `Animator` poses it -- and Cesium's tileset loader is
// the same shape. What comes back here is the door's own `Geometry`: parts with their node names,
// their surfaces, their positions, normals, texture coordinates, tangents, colours and triangles,
// and the lamps the file places.
//
// POSING REBUILDS THE GEOMETRY, which is what an animated asset MEANS: a pose is a set of node
// transforms and skin weights, and the vertices that come out of it are different vertices. A
// client that wants the rest pose asks for second zero and never poses again.
class Loaded {
public:
  Loaded();
  ~Loaded();
  Loaded(Loaded &&) noexcept;
  Loaded &operator=(Loaded &&) noexcept;
  Loaded(const Loaded &) = delete;
  Loaded &operator=(const Loaded &) = delete;

  [[nodiscard]] bool reads(std::string_view path);
  [[nodiscard]] bool wears(std::string_view variant);
  [[nodiscard]] const std::string &error(void) const;

  [[nodiscard]] const Geometry &geometry(void) const;

  // WHICH ANIMATIONS DRIVE THE POSE. Filament's `Animator::applyAnimation(index, time)` selects
  // one; a glTF may carry several that move disjoint nodes, so this takes a list and an empty one
  // means the asset stands at rest.
  [[nodiscard]] bool plays(std::span<const int> animations);
  [[nodiscard]] int animations(void) const;
  [[nodiscard]] double durationS(void) const;
  [[nodiscard]] bool poses(double seconds);

  // THE FILE'S OWN CAMERA, IF IT CARRIES ONE. glTF assets ship cameras and a client comparing a
  // render against a reference needs the one the file states rather than one it invented.
  [[nodiscard]] bool carriesCamera(void) const;
  [[nodiscard]] const Camera &camera(void) const;
  [[nodiscard]] int cameras(void) const;
  [[nodiscard]] bool camera(int index, Camera &out) const;

  // AND THE CAMERA THAT FRAMES IT, for an asset that ships none. Cesium spells this
  // `Camera.viewBoundingSphere` and Filament's sample viewer does the same arithmetic by hand;
  // a client that wants to SEE what it loaded should not have to derive the sphere itself. `fill`
  // is how much of the frame the subject takes -- the engine's own default when it is not stated.
  [[nodiscard]] bool frames(double fill, Camera &out) const;
  [[nodiscard]] bool frames(Camera &out) const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

} // namespace outshine

#endif
