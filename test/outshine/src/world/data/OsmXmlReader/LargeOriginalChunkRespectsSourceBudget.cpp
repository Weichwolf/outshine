#include "OsmXmlReader.h"
#include "Xml.h"
#include "Check.h"

#include <cstddef>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;

  std::string xml = "<osm version='0.6'>";
  for (size_t id = 1; id <= kXmlMaxNodes; ++id) {
    xml += "<node id='" + std::to_string(id) + "' lat='0' lon='0'/>";
  }
  xml += "</osm>";
  CHECK(xml.size() < kMaxOsmXmlBytes,
        "a legal original-source chunk can exceed the generic DOM element count below four MiB");
  Xml ordinary;
  CHECK(!ordinary.Parse(xml.data(), xml.size()) &&
            ordinary.Error().find("element bound") != std::string::npos,
        "ordinary XML consumers retain their existing element bound");
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "large-original", .Revision = "r1"});
  CHECK(source && source->Nodes().size() == kXmlMaxNodes && source->FindNode(kXmlMaxNodes),
        "the original OSM reader retains every node within its unchanged source byte budget");
  const std::string tooLarge(kMaxOsmXmlBytes + 1, ' ');
  const auto refused =
      OsmXmlReader::Read(tooLarge, {.DatasetId = "large-original", .Revision = "r2"});
  CHECK(!refused && refused.error() == OsmXmlError::BudgetExceeded,
        "source bytes beyond four MiB are rejected before building a DOM");
  return Report();
}
