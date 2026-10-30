#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYSLICEINTERNAL_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTBODYSLICEINTERNAL_H

#include "GroundPortBodies.h"
#include "PlanarStationMesh.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators::GroundBodySlices {
using Error = GroundPortBodyError;
template <class T> using Checked = std::expected<T, Error>;

struct Surface {
  std::vector<PlanarPoint> Points;
  std::vector<PlanarFace> Faces;
  std::array<std::vector<double>, 4> Values;
  std::vector<std::array<uint32_t, 2>> Interfaces;
  std::array<double, 2> Cuts{};
  std::array<bool, 4> Above{};
  std::array<bool, 2> HasCut{};

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Points.capacity() * sizeof(PlanarPoint) + Faces.capacity() * sizeof(PlanarFace) +
                   Interfaces.capacity() * sizeof(std::array<uint32_t, 2>);
    for (const auto &values : Values) { bytes += values.capacity() * sizeof(double); }
    return bytes;
  }
};

[[nodiscard]] Checked<Surface> Prepare(const GroundPortField &field,
                                       const GroundPortBody &body,
                                       const TransportCoreSegment &interval,
                                       GroundPortBodyBudget budget,
                                       GroundPortBodyWork &work,
                                       const std::stop_token &stop);
[[nodiscard]] Checked<GroundPortBody> Build(const GroundPortField &field,
                                            const GroundPortBody &source,
                                            const TransportCoreSegment &interval,
                                            GroundPortBodyBudget budget,
                                            GroundPortBodyWork &work,
                                            const std::stop_token &stop);
}
#endif
