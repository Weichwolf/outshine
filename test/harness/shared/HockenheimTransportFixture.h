#ifndef OUTSHINE_TEST_HOCKENHEIMTRANSPORTFIXTURE_H
#define OUTSHINE_TEST_HOCKENHEIMTRANSPORTFIXTURE_H

#include "HockenheimCircuitFixture.h"
#include "OsmTransportBuilder.h"
#include "OsmXmlReader.h"
#include "ReadTextFile.h"
#include <cstddef>
#include <expected>
#include <string>
#include <utility>

namespace outshine::Test::HockenheimFixture {
struct Network {
  World::TransportTopology Graph;
  World::CircuitRoute Route;
};

[[nodiscard]] inline std::expected<Network, std::string> LoadNetwork() {
  constexpr size_t sourceBytes = 33557;
  const auto xml = ReadTextFile(SourcePath, sourceBytes);
  if (!xml) { return std::unexpected(xml.error()); }
  const auto source =
      Generators::Osm::XmlReader::Read(*xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  if (!source) { return std::unexpected("pinned circuit fixture cannot be parsed"); }
  auto graph = Generators::Osm::TransportBuilder::Build(*source);
  if (!graph) { return std::unexpected("pinned circuit fixture cannot build its topology"); }
  auto route = Generators::Osm::TransportBuilder::ResolveCircuit(*graph, *source, RelationId);
  if (!route) { return std::unexpected("pinned circuit fixture cannot resolve its route"); }
  return Network{.Graph = std::move(*graph), .Route = std::move(*route)};
}
}
#endif
