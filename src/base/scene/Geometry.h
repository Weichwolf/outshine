#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "Mat4.h"
#include "Material.h"
#include "Texture.h"
#include "PunctualLight.h"

namespace outshine {

enum class MaterialError { CapacityExceeded, MissingMaterial, InvalidMaterial };

enum class GeometryPartError { CapacityExceeded };

enum class PlacementError { MissingPart, InvalidTransform };

enum class LightMutationError { MissingLight, CapacityExceeded };

enum class MaterialBindingError { MissingPart, MissingMaterial };

enum class GeometryAttributeError {
  MissingPart,
  IncompleteTuple,
  NonFiniteValue,
  NonUnitNormal,
  InvalidTextureSet
};

enum class GeometryAppendError { MalformedSource, CapacityExceeded };

enum class GeometryImageError {
  InvalidDimensions,
  ByteCountMismatch,
  InvalidMipChain,
  CapacityExceeded
};

[[nodiscard]] constexpr std::string_view Describe(GeometryPartError error) noexcept {
  if (error == GeometryPartError::CapacityExceeded) { return "part capacity exceeded"; }
  return "unknown part error";
}

[[nodiscard]] constexpr std::string_view Describe(GeometryAttributeError error) noexcept {
  switch (error) {
    case GeometryAttributeError::MissingPart: return "part is absent";
    case GeometryAttributeError::IncompleteTuple: return "attribute tuple count is incomplete";
    case GeometryAttributeError::NonFiniteValue: return "attribute contains a nonfinite value";
    case GeometryAttributeError::NonUnitNormal: return "normal is not unit length";
    case GeometryAttributeError::InvalidTextureSet: return "texture coordinate set is invalid";
  }
  return "unknown attribute error";
}

[[nodiscard]] constexpr std::string_view Describe(LightMutationError error) noexcept {
  switch (error) {
    case LightMutationError::MissingLight: return "light is absent";
    case LightMutationError::CapacityExceeded: return "light capacity exceeded";
  }
  return "unknown light error";
}

[[nodiscard]] constexpr std::string_view Describe(MaterialError error) noexcept {
  switch (error) {
    case MaterialError::CapacityExceeded: return "material capacity exceeded";
    case MaterialError::MissingMaterial: return "material is absent";
    case MaterialError::InvalidMaterial: return "material values or image bindings are invalid";
  }
  return "unknown material error";
}

[[nodiscard]] constexpr std::string_view Describe(GeometryImageError error) noexcept {
  switch (error) {
    case GeometryImageError::InvalidDimensions: return "image dimensions are invalid";
    case GeometryImageError::ByteCountMismatch: return "image byte count does not match dimensions";
    case GeometryImageError::InvalidMipChain: return "prepared image mip chain is incomplete";
    case GeometryImageError::CapacityExceeded: return "image capacity exceeded";
  }
  return "unknown image error";
}

class Geometry {
public:
  struct Mesh {
    std::vector<float> PositionsM;
    std::vector<float> Normals;
    std::vector<float> Uv;
    std::vector<float> Uv1;
    std::vector<float> Tangents;
    std::vector<float> Colours;
    std::vector<uint32_t> Indices;
  };

  Geometry();

  ~Geometry();

  Geometry(Geometry &&other) noexcept;

  Geometry &operator=(Geometry &&other) noexcept;

  Geometry(const Geometry &) = delete;

  Geometry &operator=(const Geometry &) = delete;

  [[nodiscard]] Geometry clone() const;

  [[nodiscard]] std::expected<void, GeometryAppendError> append(const Geometry &source);

  [[nodiscard]] std::expected<int, GeometryPartError> addPart(std::string_view named,
                                                              MaterialInstance material);

  void clear();

  [[nodiscard]] std::expected<void, PlacementError> setPlacement(int part,
                                                                 const Mat4 &model) noexcept;

  [[nodiscard]] std::expected<void, LightMutationError>
  setLight(int lamp, const PunctualLight &light) noexcept;

  [[nodiscard]] std::expected<void, MaterialBindingError>
  setMaterial(int part, MaterialInstance surface) noexcept;

  [[nodiscard]] std::expected<MaterialInstance, MaterialError> addSurface(std::string_view named,
                                                                          const Material &surface);

  [[nodiscard]] std::expected<int, LightMutationError>
  addLamp(std::string_view named, const PunctualLight &light, const Mat4 &placed);

  [[nodiscard]] std::expected<void, GeometryAttributeError> adoptMesh(int part,
                                                                      Mesh &mesh) noexcept;
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  adoptPositions(int part, std::vector<float> &metres) noexcept;
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  adoptNormals(int part, std::vector<float> &unit) noexcept;
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  adoptColours(int part, std::vector<float> &rgba) noexcept;
  [[nodiscard]] std::expected<void, GeometryAttributeError>
  adoptTriangles(int part, std::vector<uint32_t> &indices) noexcept;

  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setPositions(int part, std::span<const float> metres);

  [[nodiscard]] std::expected<void, GeometryAttributeError> setNormals(int part,
                                                                       std::span<const float> unit);

  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTexture(int part, std::span<const float> uv, int set = 0);

  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTangents(int part, std::span<const float> xyzw);

  [[nodiscard]] std::expected<void, GeometryAttributeError> setColours(int part,
                                                                       std::span<const float> rgba);

  [[nodiscard]] std::expected<void, GeometryAttributeError>
  setTriangles(int part, std::span<const uint32_t> indices);

  [[nodiscard]] int windingAgainstNormals(int part) const;

  [[nodiscard]] int parts() const;

  [[nodiscard]] size_t storageBytes() const noexcept;

  [[nodiscard]] std::string_view nameOf(int part) const;

  [[nodiscard]] MaterialInstance materialOf(int part) const;

  [[nodiscard]] const Mat4 &placementOf(int part) const;

  [[nodiscard]] std::expected<int, GeometryImageError>
  addImage(int widthPx, int heightPx, std::span<const uint8_t> rgba);

  [[nodiscard]] std::expected<int, GeometryImageError> addImage(ImageView image);

  [[nodiscard]] int images() const;

  [[nodiscard]] ImageView imageAt(int image) const;

  [[nodiscard]] int surfaces() const;

  [[nodiscard]] std::string_view surfaceNameOf(int surface) const;

  [[nodiscard]] const Material &surfaceAt(MaterialInstance surface) const;

  [[nodiscard]] std::expected<void, MaterialError> setSurface(MaterialInstance surface,
                                                              const Material &row) noexcept;

  [[nodiscard]] int lamps() const;

  [[nodiscard]] std::string_view lampNameOf(int lamp) const;

  [[nodiscard]] const PunctualLight &lampAt(int lamp) const;

  [[nodiscard]] const Mat4 &lampPlacementOf(int lamp) const;

  [[nodiscard]] std::span<const float> positionsOf(int part) const;

  [[nodiscard]] std::span<const float> normalsOf(int part) const;

  using UvSet = outshine::UvSet;

  [[nodiscard]] std::span<const float> textureOf(int part, UvSet set = UvSet::Uv0) const;

  [[nodiscard]] std::span<const float> tangentsOf(int part) const;

  [[nodiscard]] std::span<const float> coloursOf(int part) const;

  [[nodiscard]] std::span<const uint32_t> trianglesOf(int part) const;

  [[nodiscard]] bool wellFormed() const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
