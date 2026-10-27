#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTSINTERNAL_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTGROUNDPORTINPUTSINTERNAL_H

#include "MvtGroundPortInputs.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <vector>

namespace outshine::Generators::Osm {
struct MvtGroundPortInputNormalization {
  MvtGroundPortInputs Ports;
  std::vector<uint8_t> PathOmissions;
  size_t UnrepresentedIncidences = 0;
};

[[nodiscard]] std::expected<MvtGroundPortInputNormalization, MvtGroundPortInputError>
NormalizeMvtGroundPortInputs(const MvtTransportSnapshot &snapshot,
                             const OsmField &field,
                             const StreetField &streets,
                             MvtGroundPortInputLimits limits,
                             const std::stop_token &stop);
}
#endif
