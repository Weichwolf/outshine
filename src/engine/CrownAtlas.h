#ifndef OUTSHINE_ENGINE_CROWNATLAS_H
#define OUTSHINE_ENGINE_CROWNATLAS_H

#include "TreePrototype.h"
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

namespace outshine {

class CrownAtlas {
public:
  struct Texel {
    Vec3f Normal;
    float Depth = 0.0f;
    uint32_t Surface = 0;
  };

  struct View {
    Vec3 TowardEye;
    std::vector<Texel> Texels;
  };

  struct Shape {
    int Pixels = 256;
    unsigned Views = 8;
  };

  static std::optional<CrownAtlas>
  Bake(const Generators::TreePrototype &tree, Shape shape, std::string &error);

  [[nodiscard]] static std::string ProvenanceFor(std::string_view species, Shape shape);

  [[nodiscard]] std::optional<std::vector<uint8_t>> Encode(std::string_view provenance,
                                                           std::string &error) const;
  [[nodiscard]] static std::optional<CrownAtlas>
  Decode(std::span<const uint8_t> bytes, std::string_view provenance, std::string &error);

  [[nodiscard]] std::optional<Geometry> GeometryAt(size_t view) const;

  [[nodiscard]] const std::vector<View> &Views() const { return Views_; }

  [[nodiscard]] const std::vector<Material> &Surfaces() const { return Surfaces_; }

  [[nodiscard]] const Vec3 &CentreM() const { return CentreM_; }

  [[nodiscard]] double HalfExtentM() const { return HalfExtentM_; }

  [[nodiscard]] int Pixels() const { return Pixels_; }

private:
  std::vector<View> Views_;
  std::vector<Material> Surfaces_;
  Vec3 CentreM_;
  double HalfExtentM_ = 0.0;
  int Pixels_ = 0;
};

} // namespace outshine
#endif
