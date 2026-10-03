#include "OsmChunkSetLoader.h"
#include "Check.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  const auto path =
      std::filesystem::temp_directory_path() /
      ("outshine-osm-region-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".osm");
  {
    std::ofstream file(path);
    file << "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
            "<node id='2' lat='0' lon='1'/><node id='3' lat='1' lon='0'/>"
            "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
            "<tag k='building' v='yes'/><tag k='min_height' v='5.5'/></way>"
            "<relation id='20'><member type='way' ref='99' role=''/>"
            "<tag k='type' v='route'/></relation></osm>";
  }
  const std::array providers{SourceProvider{
      .Kind = "osm",
      .Revision = "r1",
      .Dataset = "original",
      .Location = path.string(),
      .Coverage = SourceCoverage{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}}};
  const auto region = outshine::Generators::Osm::ChunkSetLoader::LoadRegion(providers, ".");
  CHECK(region.has_value(), "spatial source retains an unrelated open relation");
  if (region) {
    const std::array building{outshine::Generators::Osm::ElementId{
        .Kind = outshine::Generators::Osm::ElementKind::Way, .Id = 10}};
    const std::array route{outshine::Generators::Osm::ElementId{
        .Kind = outshine::Generators::Osm::ElementKind::Relation, .Id = 20}};
    CHECK(!region->Elements.FirstMissingReference(building),
          "complete building has independent product closure");
    const auto missing = region->Elements.FirstMissingReference(route);
    CHECK(missing && missing->MissingId == 99 && missing->OwnerId == 20,
          "required route dependency remains present and unresolved");
    const auto *way = region->Elements.FindWay(10);
    CHECK(way && way->Tags.size() == 2 && way->Tags[1].Value == "5.5",
          "original minimum-height semantics survive source loading");
  }
  const auto strict = outshine::Generators::Osm::ChunkSetLoader::Load(providers, ".");
  CHECK(!strict, "declared complete chunk sets still reject every missing reference");
  std::error_code error;
  std::filesystem::remove(path, error);
  return Report();
}
