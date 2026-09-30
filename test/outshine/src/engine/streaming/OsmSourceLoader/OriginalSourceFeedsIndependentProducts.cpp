#include "OsmSourceLoader.h"
#include "OsmBuildingFootprints.h"
#include "OsmTransportLoader.h"
#include "Check.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>

namespace {
template <class Loader> bool WaitFor(Loader &loader, outshine::Tasks &tasks) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    loader.Poll();
    if (loader.CurrentPhase() != Loader::Phase::Loading) { return true; }
    (void)tasks.AwaitCompletion(0.05);
  }
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto path =
      std::filesystem::temp_directory_path() /
      ("outshine-source-products-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".osm");
  {
    std::ofstream file(path);
    file << "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
            "<node id='2' lat='0' lon='0.001'/><node id='3' lat='0.001' lon='0'/>"
            "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
            "<tag k='building' v='yes'/><tag k='min_height' v='5.5'/></way>"
            "<way id='11'><nd ref='1'/><nd ref='2'/><tag k='highway' v='residential'/></way>"
            "<relation id='20'><member type='way' ref='99' role=''/>"
            "<tag k='type' v='route'/></relation></osm>";
  }
  const std::array providers{Data::SourceProvider{
      .Kind = "osm",
      .Revision = "r1",
      .Missing = Data::MissingDataPolicy::Fail,
      .Dataset = "original",
      .Location = path.string(),
      .Coverage = Data::SourceCoverage{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}}};
  Tasks tasks(1);
  OsmSourceLoader loader(tasks);
  CHECK(loader.Request(providers, ".").has_value(), "original source admitted");
  CHECK(WaitFor(loader, tasks) && loader.CurrentPhase() == OsmSourceLoader::Phase::Ready,
        "source publishes independently of an unrelated unresolved route");
  std::error_code error;
  std::filesystem::remove(path, error);
  if (!loader.Current()) { return Report(); }
  const auto source = loader.Current();
  const auto footprints = Ground::OsmBuildingFootprints::Build(source, 3);
  CHECK(footprints && &footprints->Source() == source.get() && footprints->Buildings().size() == 1,
        "building product pins the same original snapshot");
  World::OsmTransportLoader transport(tasks);
  CHECK(transport.RequestSource(source).has_value(), "transport accepts the same source");
  CHECK(WaitFor(transport, tasks) && transport.Current() &&
            transport.Current()->Source() == source &&
            transport.Current()->Topology().Edges().size() == 2,
        "transport uses source memory after the original file has been removed");
  if (!transport.Current()) { return Report(); }
  CHECK(loader.Request(providers, ".").has_value() && loader.PendingCount() == 0 &&
            loader.Current() == source,
        "unchanged camera/source does not repeat source IO");
  auto replacement = providers;
  replacement[0].Revision = "r2";
  CHECK(loader.Request(replacement, ".").has_value(), "changed revision requests replacement");
  CHECK(WaitFor(loader, tasks) && loader.CurrentPhase() == OsmSourceLoader::Phase::Failed &&
            loader.Current() == source && transport.Current()->Source() == source,
        "failed source replacement retains both valid published products");
  return Report();
}
