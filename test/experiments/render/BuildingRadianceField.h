#ifndef OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#define OUTSHINE_TEST_BUILDING_RADIANCE_FIELD_H
#include "ImpostorAtlas.h"
#include "ImpostorCards.h"
#include <nlohmann/json_fwd.hpp>
#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace outshine::Test {
struct BuildingRadianceField {
  ImpostorCards Cards;
  double PrepareMs = 0;
  double GpuMs = 0;
  double SetupGpuMs = 0;
  std::vector<float> SourceLinear;
};

[[nodiscard]] std::expected<BuildingRadianceField, std::string>
PrepareBuildingRadiance(const ImpostorAtlas &atlas,
                        size_t view,
                        const nlohmann::json &query,
                        const Geometry *original = nullptr);
}
#endif
