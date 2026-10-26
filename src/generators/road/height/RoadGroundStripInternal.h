#ifndef OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDSTRIPINTERNAL_H
#define OUTSHINE_GENERATORS_ROAD_HEIGHT_ROADGROUNDSTRIPINTERNAL_H

#include "RoadGroundStrip.h"
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators::RoadGround {
struct Axis {
  std::vector<EastNorth> Points;
  std::vector<double> Stations;
  std::vector<EastNorth> Tangents;
  std::vector<EastNorth> Miters;
};

[[nodiscard]] std::expected<Axis, RoadGroundError>
Frame(std::span<const LongitudeLatitude> geographic, double widthM, const std::stop_token &stop);
[[nodiscard]] std::expected<std::vector<double>, RoadGroundError>
Bands(std::span<const double> stations, size_t controlsMost);
[[nodiscard]] std::expected<void, RoadGroundError> Layout(const RoadGroundStrip &strip,
                                                          RoadGroundField &field);
[[nodiscard]] bool Valid(LongitudeLatitude at) noexcept;
}
#endif
