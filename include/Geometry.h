#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "math/Mat4.h"
#include "Material.h"
#include "Texture.h"
#include "PunctualLight.h"

namespace outshine {

class Geometry;

class TransformManager {
public:
  [[nodiscard]] bool setTransform(int part, const Mat4 &model);
  [[nodiscard]] const Mat4 &getTransform(int part) const;

private:
  friend class Geometry;

  explicit TransformManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

class LightManager {
public:
  [[nodiscard]] int count() const;
  [[nodiscard]] const PunctualLight &getLight(int lamp) const;
  [[nodiscard]] bool setLight(int lamp, const PunctualLight &light);
  [[nodiscard]] std::string_view nameOf(int lamp) const;
  [[nodiscard]] const Mat4 &getTransform(int lamp) const;

private:
  friend class Geometry;

  explicit LightManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

class RenderableManager {
public:
  [[nodiscard]] int count() const;
  [[nodiscard]] std::string_view nameOf(int part) const;
  [[nodiscard]] MaterialInstance getMaterial(int part) const;
  [[nodiscard]] bool setMaterial(int part, MaterialInstance surface);
  [[nodiscard]] size_t vertexCount(int part) const;
  [[nodiscard]] size_t triangleCount(int part) const;

private:
  friend class Geometry;

  explicit RenderableManager(Geometry &of) : Of_(&of) {}

  Geometry *Of_ = nullptr;
};

class Geometry {
public:
  Geometry();
  ~Geometry();
  Geometry(Geometry &&) noexcept;
  Geometry &operator=(Geometry &&) noexcept;
  Geometry(const Geometry &) = delete;
  Geometry &operator=(const Geometry &) = delete;

  int addPart(std::string_view named, MaterialInstance material);
  void clear();

  [[nodiscard]] TransformManager transforms();
  [[nodiscard]] LightManager lights();
  [[nodiscard]] RenderableManager renderables();

  [[nodiscard]] MaterialInstance addSurface(std::string_view named, const Material &surface);
  int addLamp(std::string_view named, const PunctualLight &light, const Mat4 &placed);

  bool setPositions(int part, std::span<const float> metres);
  bool setNormals(int part, std::span<const float> unit);
  bool setTexture(int part, std::span<const float> uv, int set = 0);
  bool setTangents(int part, std::span<const float> xyzw);
  bool setColours(int part, std::span<const float> rgba);
  bool setTriangles(int part, std::span<const uint32_t> indices);

  [[nodiscard]] int parts() const;
  [[nodiscard]] std::string_view nameOf(int part) const;
  [[nodiscard]] MaterialInstance materialOf(int part) const;

  int addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba);
  [[nodiscard]] int images() const;
  [[nodiscard]] ImageView imageAt(int image) const;

  [[nodiscard]] int surfaces() const;
  [[nodiscard]] std::string_view surfaceNameOf(int surface) const;
  [[nodiscard]] const Material &surfaceAt(MaterialInstance surface) const;

  bool setSurface(MaterialInstance surface, const Material &row);

  [[nodiscard]] int lamps() const;
  [[nodiscard]] std::string_view lampNameOf(int lamp) const;
  [[nodiscard]] const PunctualLight &lampAt(int lamp) const;
  [[nodiscard]] const Mat4 &lampPlacementOf(int lamp) const;
  [[nodiscard]] std::span<const float> positionsOf(int part) const;
  [[nodiscard]] std::span<const float> normalsOf(int part) const;
  [[nodiscard]] std::span<const float> textureOf(int part, int set = 0) const;
  [[nodiscard]] std::span<const float> tangentsOf(int part) const;
  [[nodiscard]] std::span<const float> coloursOf(int part) const;
  [[nodiscard]] std::span<const uint32_t> trianglesOf(int part) const;
  [[nodiscard]] bool wellFormed() const;

private:
  friend class TransformManager;
  friend class LightManager;
  friend class RenderableManager;
  void place(int part, const Mat4 &model);
  void relight(int lamp, const PunctualLight &light);
  void resurface(int part, MaterialInstance surface);
  [[nodiscard]] const Mat4 &placementOf(int part) const;

  struct Held;
  std::unique_ptr<Held> Held_;
};

} // namespace outshine

#endif
