#ifndef OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSOURCEFACTSINTERNAL_H
#define OUTSHINE_GENERATORS_OSM_TRANSPORT_MVTTRANSPORTSOURCEFACTSINTERNAL_H

#include "MvtTransportSnapshotInternal.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators::Osm::MvtTransport {
struct SourceFactInput {
  const Sources &SourceRows;
  std::span<const Segment> Segments;
  const Path::Network &Network;
  const OsmField &Field;
  const StreetField &Streets;
};

[[nodiscard]] bool FitsSourceFacts(const SourceFactInput &input, size_t groups, size_t bytesMost);
[[nodiscard]] std::expected<void, std::string> FillSourceFacts(MvtTransportSourceFacts &facts,
                                                               const SourceFactInput &input,
                                                               const std::stop_token &stop);
[[nodiscard]] std::expected<void, std::string> FillSourceWayFacts(MvtTransportSourceFacts &facts,
                                                                  const SourceFactInput &input,
                                                                  const std::stop_token &stop);
}
#endif
