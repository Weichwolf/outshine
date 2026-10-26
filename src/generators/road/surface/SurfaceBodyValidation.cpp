#include "SurfaceBodyInternal.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>

namespace outshine::Generators::SurfaceBodies {
Checked<void> Validate(const State &state) {
  const auto &request = state.Request;
  const auto &topology = request.Topology;
  if (state.Stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (topology.Points.size() > state.Budget.Points || request.Sources.size() > 64 ||
      request.Flights.size() > state.Budget.Flights ||
      topology.Points.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (state.Heights.size() != topology.Points.size() ||
      topology.Sources.size() != request.Sources.size() ||
      !std::isfinite(state.Budget.MaximumRiseM) || state.Budget.MaximumRiseM <= 0 ||
      !std::isfinite(state.Budget.ThicknessM) || state.Budget.ThicknessM <= 0 ||
      state.Budget.Risers == 0 || state.Budget.Risers >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error::InvalidInput);
  }
  for (size_t at = 0; at < topology.Points.size(); ++at) {
    const auto xy = topology.Points[at];
    if (!std::isfinite(xy[0]) || !std::isfinite(xy[1]) || !std::isfinite(state.Heights[at])) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  for (const auto &flight : request.Flights) {
    if (flight.Source >= request.Sources.size() || flight.Start >= request.Ports.size() ||
        flight.End >= request.Ports.size() || flight.Start == flight.End ||
        (flight.Direction != 1 && flight.Direction != -1) || !std::isfinite(flight.CutStart) ||
        !std::isfinite(flight.CutEnd) || flight.CutStart >= flight.CutEnd) {
      return std::unexpected(Error::InvalidInput);
    }
  }
  return {};
}
}
