#include "OsmSourceLoader.h"
#include "SourceProviderValidation.h"
#include "OsmBuildingFootprints.h"
#include "OsmTransportLoader.h"
#include "Check.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

namespace {
class ApiWire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0};
  std::atomic<int> Canceled{0};
  std::atomic<bool> Block{false};
  const std::string Xml =
      "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
      "<node id='2' lat='0' lon='0.001'/><node id='3' lat='0.001' lon='0'/>"
      "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='1'/>"
      "<tag k='building' v='yes'/><tag k='custom:unknown' v='preserved'/></way>"
      "<way id='11'><nd ref='1'/><nd ref='2'/><tag k='highway' v='residential'/></way></osm>";

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (Block) { return outshine::Data::Wire::Working(); }
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(Xml.begin(), Xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Canceled; }
};

bool WaitFor(outshine::OsmSourceLoader &loader) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    loader.Poll();
    if (loader.CurrentPhase() != outshine::OsmSourceLoader::Phase::Loading) { return true; }
    (void)loader.AwaitSlice(0.01);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-native-api-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated native source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    ApiWire wire;
    Tasks compute(1);
    OsmSourceLoader loader(compute, &wire, directory);
    const std::array providers{Data::SourceProvider{
        .Kind = "osm",
        .Revision = "r1",
        .Missing = Data::MissingDataPolicy::Fail,
        .Dataset = "openstreetmap.original",
        .Endpoint = std::string(Data::kOfficialOsmApi),
        .Coverage =
            Data::SourceCoverage{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 0.01, .NorthDeg = 0.01}}};
    CHECK(loader.Request(providers, ".") && WaitFor(loader) && loader.Current(),
          "official source acquisition and native compute publication complete asynchronously");
    const auto source = loader.Current();
    if (source) {
      const auto buildings = Ground::OsmBuildingFootprints::Build(source, 3);
      CHECK(buildings && buildings->Buildings().size() == 1 && &buildings->Source() == source.get(),
            "building product consumes the fetched original snapshot");
      World::OsmTransportLoader transport(compute);
      CHECK(transport.RequestSource(source), "native transport consumes the same original source");
      const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      do {
        transport.Poll();
        if (transport.CurrentPhase() != World::OsmTransportLoader::Phase::Loading) { break; }
        (void)compute.AwaitCompletion(0.01);
      } while (std::chrono::steady_clock::now() < until);
      CHECK(transport.Current() && transport.Current()->Topology().Edges().size() == 2,
            "original road semantics reach the native graph without map tiles");
    }
    CHECK(loader.Request(providers, ".") && loader.PendingCount() == 0 && wire.Starts == 1,
          "unchanged source does not repeat IO or native generation");
    auto replacement = providers;
    replacement[0].Revision = "r2";
    wire.Block = true;
    CHECK(loader.Request(replacement, "."), "a changed source revision enters IO");
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (wire.Starts < 2 && std::chrono::steady_clock::now() < until) {
      (void)loader.AwaitSlice(0.01);
    }
    std::atomic<bool> ran{false};
    const auto job = compute.Post([&ran] { ran = true; });
    while (!compute.Done(job) && std::chrono::steady_clock::now() < until) {
      (void)compute.AwaitCompletion(0.01);
    }
    CHECK(wire.Starts == 2 && ran && loader.Current() == source,
          "waiting network IO leaves the single compute worker free and the old source resident");
    CHECK(loader.Request({}, "."), "source removal cancels pending acquisition");
    do {
      loader.Poll();
      if (loader.PendingCount() == 0) { break; }
      (void)loader.AwaitSlice(0.01);
    } while (std::chrono::steady_clock::now() < until);
    CHECK(wire.Canceled == 1 && loader.PendingCount() == 0 && !loader.Current(),
          "canceled network result cannot publish after source removal");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated native source cache removed");
  return Report();
}
