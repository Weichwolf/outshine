#include "OsmXmlReader.h"
#include "Check.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

std::string_view TagValue(const std::vector<outshine::Data::OsmTag> &tags, std::string_view key) {
  for (const auto &tag : tags) {
    if (tag.Key == key) { return tag.Value; }
  }
  return {};
}

}

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  std::ifstream input("test/outshine/integration/places/HockenheimringGrandPrix.osm",
                      std::ios::binary);
  CHECK(input.good(), "pinned OSM relation source is available");
  if (!input) { return Report(); }
  const std::string xml(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto parsed = OsmXmlReader::Read(xml);
  CHECK(parsed.has_value(), "pinned OSM source parses");
  if (!parsed) { return Report(); }
  const OsmElements &elements = *parsed;
  CHECK(elements.Nodes().size() == 319 && elements.Ways().size() == 17 &&
            elements.Relations().size() == 1 && !elements.FirstMissingReference(),
        "all source objects and references survive the import");
  const OsmRelation *circuit = elements.FindRelation(284588);
  CHECK(circuit != nullptr && circuit->Members.size() == 17 &&
            TagValue(circuit->Tags, "type") == "circuit",
        "the Grand Prix relation and its members survive by source ID");
  if (circuit == nullptr) { return Report(); }

  std::unordered_map<uint64_t, uint64_t> successor;
  std::unordered_map<uint64_t, size_t> indegree;
  size_t mainWays = 0;
  size_t pitWays = 0;
  bool validMembers = true;
  for (const OsmRelationMember &member : circuit->Members) {
    const OsmWay *way = elements.FindWay(member.Id);
    validMembers &= member.Kind == OsmElementKind::Way && way != nullptr;
    if (way == nullptr) { continue; }
    if (member.Role == "pitlane") {
      ++pitWays;
      continue;
    }
    validMembers &= member.Role.empty() && TagValue(way->Tags, "highway") == "raceway" &&
                    TagValue(way->Tags, "oneway") == "yes" && way->NodeIds.size() >= 2;
    ++mainWays;
    for (size_t at = 1; at < way->NodeIds.size(); ++at) {
      validMembers &= successor.emplace(way->NodeIds[at - 1], way->NodeIds[at]).second;
      ++indegree[way->NodeIds[at]];
    }
  }
  CHECK(validMembers && mainWays == 16 && pitWays == 1 && successor.size() == 267,
        "sixteen directed raceway ways form the main route; the pitlane has its own role");

  std::unordered_set<uint64_t> visited;
  uint64_t node = successor.empty() ? 0 : successor.begin()->first;
  const uint64_t start = node;
  bool cycle = !successor.empty();
  for (size_t step = 0; step < successor.size() && cycle; ++step) {
    cycle &= visited.insert(node).second && indegree[node] == 1;
    const auto next = successor.find(node);
    cycle &= next != successor.end();
    if (next != successor.end()) { node = next->second; }
  }
  CHECK(cycle && node == start && visited.size() == 267,
        "directed node topology closes exactly once independently of relation member order");
  return Report();
}
