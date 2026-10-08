#ifndef OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORCARDS_H
#define OUTSHINE_CONTENT_IMPOSTOR_IMPOSTORCARDS_H

#include "math/Vec3.h"
#include "scene/Geometry.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Content {
struct ImpostorCards {
  struct View {
    Vec3 Direction;
    Geometry Surface;
  };

  Vec3 Centre;
  double HalfExtentM = 0.0;
  std::vector<View> Views;

  [[nodiscard]] static std::string AssetKey(std::string_view provenance);

  [[nodiscard]] std::optional<std::vector<uint8_t>> Encode(std::string_view provenance,
                                                           size_t bytesMost) const;
  [[nodiscard]] static std::optional<ImpostorCards>
  Decode(std::span<const uint8_t> bytes, std::string_view provenance, size_t bytesMost);
};
}
#endif
