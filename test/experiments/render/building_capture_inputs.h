#ifndef OUTSHINE_TEST_BUILDING_CAPTURE_INPUTS_H
#define OUTSHINE_TEST_BUILDING_CAPTURE_INPUTS_H

#include "BuildingSurfaceCapture.h"
#include <nlohmann/json.hpp>
#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace outshine::Test {
[[nodiscard]] std::expected<nlohmann::json, std::string>
BuildingCaptureInputs(std::span<const StructurePlan> plans,
                      const Vec3 &eye,
                      ProjectedErrorBudget projection,
                      const Generators::BuildingSurfaceCaptureRecord &record,
                      const Raised &captured,
                      size_t triangleLimit);
}

#endif
