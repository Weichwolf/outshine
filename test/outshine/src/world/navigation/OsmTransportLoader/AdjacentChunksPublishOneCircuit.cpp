#include "OsmTransportLoader.h"
#include "Check.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace {

bool WaitFor(outshine::World::OsmTransportLoader &source, outshine::Tasks &tasks) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    source.Poll();
    if (source.CurrentPhase() != outshine::World::OsmTransportLoader::Phase::Loading) {
      return true;
    }
    (void)tasks.AwaitCompletion(0.05);
  }
  return false;
}

outshine::Data::SourceProvider
Chunk(std::string path, int rank, std::string revision, outshine::Data::SourceCoverage coverage) {
  return {.Kind = "osm",
          .Revision = std::move(revision),
          .Priority = rank,
          .Missing = outshine::Data::MissingDataPolicy::Fail,
          .Dataset = "osm-analytical",
          .Location = std::move(path),
          .Coverage = coverage};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr std::string_view firstXml =
      "<osm version='0.6'>"
      "<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='0.001'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<relation id='9'><member type='way' ref='12' role=''/>"
      "<member type='way' ref='10' role=''/><member type='way' ref='11' role=''/>"
      "<tag k='type' v='circuit'/></relation></osm>";
  constexpr std::string_view secondXml =
      "<osm version='0.6'>"
      "<node id='2' lat='0' lon='0.001'/><node id='3' lat='0.001' lon='0.002'/>"
      "<way id='11'><nd ref='2'/><nd ref='3'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way>"
      "<way id='12'><nd ref='3'/><nd ref='1'/><tag k='highway' v='raceway'/>"
      "<tag k='oneway' v='yes'/></way></osm>";
  const auto stem = std::filesystem::temp_directory_path() /
                    ("outshine-osm-adjacent-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  const std::filesystem::path firstPath = stem.string() + "-a.osm";
  const std::filesystem::path secondPath = stem.string() + "-b.osm";
  {
    std::ofstream first(firstPath, std::ios::binary);
    std::ofstream second(secondPath, std::ios::binary);
    first << firstXml;
    second << secondXml;
  }

  Tasks tasks(1);
  OsmTransportLoader loader(tasks);
  std::array providers{
      Chunk(secondPath.string(),
            1,
            "r1",
            {.WestDeg = 0.001, .SouthDeg = 0.0, .EastDeg = 0.002, .NorthDeg = 0.001}),
      Chunk(firstPath.string(),
            0,
            "r1",
            {.WestDeg = 0.0, .SouthDeg = 0.0, .EastDeg = 0.001, .NorthDeg = 0.001})};
  CHECK(loader.Request(providers, "."), "shuffled adjacent source declarations are queued");
  CHECK(WaitFor(loader, tasks) && loader.CurrentPhase() == OsmTransportLoader::Phase::Ready,
        "cross-chunk nodes and relation publish only after both chunks close");
  const auto first = loader.Current();
  bool circuitReady = false;
  if (first) {
    const auto circuit = first->ResolveCircuit(9);
    circuitReady = circuit && circuit->EdgeIds.size() == 3;
  }
  CHECK(first && first->SourceIdentity().Revision == "r1" && circuitReady,
        "source IDs form one directed circuit independently of declaration order");
  if (!first) {
    std::error_code ignored;
    std::filesystem::remove(firstPath, ignored);
    std::filesystem::remove(secondPath, ignored);
    return Report();
  }

  {
    std::ofstream changed(secondPath, std::ios::binary | std::ios::trunc);
    changed << "<osm version='0.6'><node id='2' lat='0.5' lon='0.001'/></osm>";
  }
  for (auto &provider : providers) { provider.Revision = "r2"; }
  CHECK(loader.Request(providers, "."), "changed revision is queued");
  CHECK(WaitFor(loader, tasks) && loader.CurrentPhase() == OsmTransportLoader::Phase::Failed &&
            loader.Error().find("2") != std::string_view::npos && loader.Current() == first,
        "conflicting cross-chunk ID rejects replacement and retains the published graph");

  {
    std::ofstream restored(secondPath, std::ios::binary | std::ios::trunc);
    restored << secondXml;
  }
  CHECK(loader.Request(providers, "."), "same revision retries after corrected source bytes");
  CHECK(WaitFor(loader, tasks) && loader.CurrentPhase() == OsmTransportLoader::Phase::Ready &&
            loader.Current() && loader.Current()->SourceIdentity().Revision == "r2" &&
            loader.Current()->Topology().Edges().size() == first->Topology().Edges().size(),
        "corrected revision atomically replaces an equal-sized prior graph");

  providers[0].Revision = "r3";
  CHECK(!loader.Request(providers, ".") && loader.Current() &&
            loader.Current()->SourceIdentity().Revision == "r2",
        "mixed source revisions reject before scheduling or changing publication");

  std::error_code cleanupError;
  std::filesystem::remove(firstPath, cleanupError);
  CHECK(!cleanupError, "first temporary chunk is removed");
  std::filesystem::remove(secondPath, cleanupError);
  CHECK(!cleanupError, "second temporary chunk is removed");
  return Report();
}
