#ifndef OUTSHINE_IMPORT_TANGENTS_H
#define OUTSHINE_IMPORT_TANGENTS_H

#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Gltf {

struct TangentSubject {
  std::span<const double> PositionsM;
  std::span<const double> Normals;
  std::span<const double> Uv;
  std::span<const uint32_t> Indices;
};

[[nodiscard]] std::expected<void, std::string_view> GenerateTangents(const TangentSubject &subject,
                                                                     std::vector<double> &out);

}
#endif
