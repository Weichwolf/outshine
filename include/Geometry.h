#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "Material.h"
#include "PunctualLight.h"

namespace outshine {

class Geometry;

class TransformManager {
public:
  [[nodiscard]] bool setTransform(int part, const double modelM16[16]);
  [[nodiscard]] const double *getTransform(int part) const;

private:
  friend class Geometry;
  explicit TransformManager(Geometry &of) : Of_(&of) {}
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

  [[nodiscard]] TransformManager transforms(void);

  [[nodiscard]] MaterialInstance addSurface(std::string_view named, const Material &surface);
  int addLamp(std::string_view named, const PunctualLight &light, const double placedM16[16]);

  bool setPositions(int part, std::span<const float> metres);
  bool setNormals(int part, std::span<const float> unit);
  bool setTexture(int part, std::span<const float> uv, int set = 0);
  bool setTangents(int part, std::span<const float> xyzw);
  bool setColours(int part, std::span<const float> rgba);
  bool setTriangles(int part, std::span<const uint32_t> indices);

  [[nodiscard]] int parts() const;
  [[nodiscard]] std::string_view nameOf(int part) const;
  [[nodiscard]] MaterialInstance materialOf(int part) const;

  [[nodiscard]] int surfaces() const;
  [[nodiscard]] std::string_view surfaceNameOf(int surface) const;
  [[nodiscard]] const Material &surfaceAt(MaterialInstance surface) const;

  [[nodiscard]] int lamps() const;
  [[nodiscard]] std::string_view lampNameOf(int lamp) const;
  [[nodiscard]] const PunctualLight &lampAt(int lamp) const;
  [[nodiscard]] const double *lampPlacementOf(int lamp) const;
  [[nodiscard]] std::span<const float> positionsOf(int part) const;
  [[nodiscard]] std::span<const float> normalsOf(int part) const;
  [[nodiscard]] std::span<const float> textureOf(int part, int set = 0) const;
  [[nodiscard]] std::span<const float> tangentsOf(int part) const;
  [[nodiscard]] std::span<const float> coloursOf(int part) const;
  [[nodiscard]] std::span<const uint32_t> trianglesOf(int part) const;
  [[nodiscard]] bool wellFormed() const;

private:
  friend class TransformManager;
  void place(int part, const double modelM16[16]);
  [[nodiscard]] const double *placementOf(int part) const;

  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
