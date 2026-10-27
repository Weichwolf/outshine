#ifndef OUTSHINE_TEST_TRANSPORT_NATIVE_TRANSPORT_JSON_H
#define OUTSHINE_TEST_TRANSPORT_NATIVE_TRANSPORT_JSON_H
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

namespace outshine::Generators::Osm {
class OsmField;
class StreetField;
struct MvtTransportSnapshot;
struct MvtTransportSourceFacts;
}

namespace outshine::Test::TransportCapture {
void WriteBytes(std::FILE *file, std::span<const uint8_t> bytes);
void WriteCoordinate(std::FILE *file, double value);
void WriteSourceFacts(std::FILE *file,
                      const Generators::Osm::MvtTransportSourceFacts &facts,
                      const Generators::Osm::OsmField &field,
                      const std::array<int, 3> &tile);
bool WriteSnapshot(std::FILE *file,
                   const Generators::Osm::MvtTransportSnapshot &snapshot,
                   const Generators::Osm::OsmField &field,
                   const Generators::Osm::StreetField &streets,
                   const std::array<int, 3> &tile);
}
#endif
