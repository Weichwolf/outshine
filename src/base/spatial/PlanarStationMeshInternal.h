#ifndef OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESHINTERNAL_H
#define OUTSHINE_BASE_SPATIAL_PLANARSTATIONMESHINTERNAL_H

#include "PlanarStationMesh.h"
#include <cstddef>
#include <expected>
#include <stop_token>

namespace outshine::PlanarStations {
using Error = PlanarMeshError;
template <typename T> using Checked = std::expected<T, Error>;

[[nodiscard]] double Area(PlanarPoint a, PlanarPoint b, PlanarPoint c);
[[nodiscard]] Checked<void> Cut(PlanarStationMesh &output,
                                size_t axis,
                                double station,
                                PlanarStationBudget budget,
                                const std::stop_token &stop);
}
#endif
