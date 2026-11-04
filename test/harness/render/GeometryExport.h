#ifndef OUTSHINE_TEST_GEOMETRYEXPORT_H
#define OUTSHINE_TEST_GEOMETRYEXPORT_H
#include "Geometry.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace outshine::Test {
[[nodiscard]] inline std::expected<void, std::string>
SaveGeometry(const Geometry &geometry, const std::filesystem::path &path) {
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) { return std::unexpected("cannot create geometry output directory"); }
  const auto write = [](const auto &file, auto bytes) {
    std::ofstream stream(file, std::ios::binary);
    stream.write(reinterpret_cast<const char *>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size_bytes()));
    return static_cast<bool>(stream);
  };
  nlohmann::json parts = nlohmann::json::array();
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto directory = path / std::to_string(part);
    std::filesystem::create_directories(directory, error);
    if (error) { return std::unexpected("cannot create geometry part directory"); }
    const size_t vertices = geometry.positionsOf(part).size() / 3;
    const auto uv1 = geometry.textureOf(part, UvSet::Uv1);
    const auto colours = geometry.coloursOf(part);
    const std::vector<float> defaultUv(uv1.empty() ? vertices * 2 : 0);
    const std::vector<float> defaultColour(colours.empty() ? vertices * 4 : 0, 1.0f);
    if (!write(directory / "positions.bin", geometry.positionsOf(part)) ||
        !write(directory / "normals.bin", geometry.normalsOf(part)) ||
        !write(directory / "uv.bin", geometry.textureOf(part)) ||
        !write(directory / "indices.bin", geometry.trianglesOf(part)) ||
        !write(directory / "uv1.bin", uv1.empty() ? std::span<const float>(defaultUv) : uv1) ||
        !write(directory / "colours.bin",
               colours.empty() ? std::span<const float>(defaultColour) : colours)) {
      return std::unexpected("geometry part output failed");
    }
    parts.push_back({{"vertices", vertices},
                     {"triangles", geometry.trianglesOf(part).size() / 3},
                     {"material", geometry.materialOf(part).index()}});
  }
  std::ofstream description(path / "input.json");
  description << nlohmann::json{{"parts", parts}}.dump(2) << '\n';
  if (!description) { return std::unexpected("geometry description output failed"); }
  return {};
}
}
#endif
