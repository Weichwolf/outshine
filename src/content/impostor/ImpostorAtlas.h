#ifndef OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORATLAS_H
#define OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORATLAS_H

#include "math/Vec3.h"
#include "scene/Material.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Content {

class ImpostorAtlas {
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

  [[nodiscard]] static std::optional<ImpostorAtlas> Create(int pixels,
                                                           Vec3 centreM,
                                                           double halfExtentM,
                                                           std::vector<Material> surfaces,
                                                           std::vector<View> views,
                                                           std::string &error);

  [[nodiscard]] std::optional<std::vector<uint8_t>> Encode(std::string_view provenance,
                                                           std::string &error) const;
  [[nodiscard]] static std::optional<ImpostorAtlas>
  Decode(std::span<const uint8_t> bytes, std::string_view provenance, std::string &error);

  [[nodiscard]] const std::vector<View> &Views() const noexcept { return Views_; }

  [[nodiscard]] const std::vector<Material> &Surfaces() const noexcept { return Surfaces_; }

  [[nodiscard]] const Vec3 &CentreM() const noexcept { return CentreM_; }

  [[nodiscard]] double HalfExtentM() const noexcept { return HalfExtentM_; }

  [[nodiscard]] int Pixels() const noexcept { return Pixels_; }

private:
  std::vector<View> Views_;
  std::vector<Material> Surfaces_;
  Vec3 CentreM_;
  double HalfExtentM_ = 0.0;
  int Pixels_ = 0;
};

}
#endif
