#include "OsmChunkSetLoader.h"
#include "Check.h"

#include <span>
#include <stop_token>
#include <string_view>

int main() {
  using namespace outshine;
  using namespace outshine::Test;

  const Data::SourceProvider source{
      .Kind = "osm",
      .Revision = "r1",
      .Dataset = "missing",
      .Location = "source-which-must-never-be-read.osm",
      .Coverage =
          Data::SourceCoverage{.WestDeg = 0.0, .SouthDeg = 0.0, .EastDeg = 1.0, .NorthDeg = 1.0}};
  std::stop_source stop;
  (void)stop.request_stop();
  const auto loaded = Data::OsmChunkSetLoader::Load(std::span(&source, 1), ".", stop.get_token());
  CHECK(!loaded && std::string_view(loaded.error()).find("canceled") != std::string_view::npos,
        "a superseded OSM build stops before touching its first source file");
  return Report();
}
