#include <Outshine.h>
#include "Check.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto path =
      std::filesystem::temp_directory_path() /
      ("outshine-independent-source-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".osm");
  const std::string xml =
      "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
      "<node id='2' lat='0' lon='0.001'/><node id='3' lat='0.001' lon='0'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
      "<tag k='building' v='yes'/></way>"
      "<way id='11'><nd ref='1'/><nd ref='999'/><tag k='highway' v='residential'/></way>"
      "<relation id='20'><member type='way' ref='11' role=''/>"
      "<tag k='type' v='route'/><tag k='route' v='road'/></relation></osm>";
  {
    std::ofstream file(path);
    file << xml;
    CHECK(file.good(), "isolated original source is written");
  }
  Scenario::Document document;
  document.Providers.push_back(
      {.Kind = "osm",
       .Revision = "original-r1",
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "openstreetmap.original",
       .Location = path.string(),
       .Coverage = Data::SourceCoverage{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}});
  {
    Engine sourceOnly;
    CHECK(sourceOnly.setRoots({.Offline = true}) && sourceOnly.declare(document) &&
              sourceOnly.assemble(),
          "renderer-free source declaration queues original data");
    CHECK(!sourceOnly.settled() &&
              sourceOnly.unsettledReasons(WorldQuality::Playable)
                      .find("original OSM source pending") != std::string::npos,
          "pending original data remains a readiness blocker");
    CHECK(sourceOnly.preload(2.0) && sourceOnly.settled() &&
              sourceOnly.settled(WorldQuality::Refined),
          "an unrelated incomplete highway does not prevent original source publication");
    double bytes = -1, pending = -1, graphMs = -1;
    for (const DiagnosticSample &sample : sourceOnly.measures()) {
      if (sample.Name == "semantic OSM source bytes") { bytes = sample.Value; }
      if (sample.Name == "semantic OSM jobs pending") { pending = sample.Value; }
      if (sample.Name == "semantic OSM graph time") { graphMs = sample.Value; }
    }
    CHECK(bytes == static_cast<double>(xml.size()) && pending == 0 && graphMs == -1,
          "actual source bytes publish without an unused graph job");
    document.Routes.push_back({.Id = "incomplete", .OsmRelationId = 20});
    Engine routed;
    CHECK(routed.setRoots({.Offline = true}) && routed.declare(document) && routed.assemble(),
          "the same original data admits an explicit route request");
    const auto loaded = routed.preload(2.0);
    CHECK(!loaded && !loaded.error().empty() && !routed.settled(),
          "a demanded incomplete transport product still blocks publication");
    double originalBytes = -1, routedBytes = -1;
    for (const DiagnosticSample &sample : routed.measures()) {
      if (sample.Name == "original OSM source bytes") { originalBytes = sample.Value; }
      if (sample.Name == "semantic OSM source bytes") { routedBytes = sample.Value; }
    }
    CHECK(originalBytes == static_cast<double>(xml.size()) && routedBytes == -1,
          "original acquisition and transport publication have distinct diagnostic completion");
  }
  std::error_code error;
  std::filesystem::remove(path, error);
  CHECK(!error, "isolated original source is removed");
  return Report();
}
