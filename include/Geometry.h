#ifndef OUTSHINE_GEOMETRY_H
#define OUTSHINE_GEOMETRY_H

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace outshine {

class Geometry {
public:
  Geometry();
  ~Geometry();
  Geometry(Geometry &&) noexcept;
  Geometry &operator=(Geometry &&) noexcept;
  Geometry(const Geometry &) = delete;
  Geometry &operator=(const Geometry &) = delete;

  int Part(std::string_view named, int material);

  bool Positions(int part, std::span<const float> metres);
  bool Normals(int part, std::span<const float> unit);
  bool Texture(int part, std::span<const float> uv, int set = 0);
  bool Tangents(int part, std::span<const float> xyzw);
  bool Colours(int part, std::span<const float> rgba);
  bool Triangles(int part, std::span<const uint32_t> indices);

  [[nodiscard]] int Parts() const;
  [[nodiscard]] std::string_view NameOf(int part) const;
  [[nodiscard]] int MaterialOf(int part) const;
  [[nodiscard]] std::span<const float> PositionsOf(int part) const;
  [[nodiscard]] std::span<const float> NormalsOf(int part) const;
  [[nodiscard]] std::span<const float> TextureOf(int part, int set = 0) const;
  [[nodiscard]] std::span<const float> TangentsOf(int part) const;
  [[nodiscard]] std::span<const float> ColoursOf(int part) const;
  [[nodiscard]] std::span<const uint32_t> TrianglesOf(int part) const;
  [[nodiscard]] bool Whole() const;

private:
  struct Held;
  std::unique_ptr<Held> Held_;
};

}

#endif
