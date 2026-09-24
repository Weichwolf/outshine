#include "OsmTransportLoader.h"
#include "Check.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <system_error>

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
Region(std::string location, std::string revision, outshine::Data::SourceCoverage coverage) {
  return {.Kind = "osm",
          .Revision = std::move(revision),
          .Missing = outshine::Data::MissingDataPolicy::Fail,
          .Dataset = "openstreetmap",
          .Location = std::move(location),
          .Coverage = coverage};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::World;

  Tasks tasks(1);
  OsmTransportLoader source(tasks);
  const auto hockenheim =
      Region("src/assets/world/osm/HockenheimringGrandPrix.osm",
             "pin-r1",
             {.WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34});
  CHECK(source.Request(std::span(&hockenheim, 1), "."), "versioned regional source is queued");
  CHECK(WaitFor(source, tasks) && source.CurrentPhase() == OsmTransportLoader::Phase::Ready,
        "background import publishes a complete native graph");
  const auto original = source.Current();
  bool circuitValid = false;
  if (original) {
    const auto circuit = original->ResolveCircuit(284588);
    circuitValid = circuit && circuit->EdgeIds.size() == 267;
  }
  CHECK(original && original->SourceIdentity().Revision == "pin-r1" &&
            original->SourceBytes > 30000 && circuitValid,
        "published source IDs resolve the independent Hockenheim circuit oracle");
  if (!original) { return Report(); }

  auto wrongPin = hockenheim;
  wrongPin.Revision = "sha256:" + std::string(64, '0');
  CHECK(source.Request(std::span(&wrongPin, 1), "."), "well-formed digest pin is queued");
  CHECK(WaitFor(source, tasks) && source.CurrentPhase() == OsmTransportLoader::Phase::Failed &&
            source.Error().find("sha256") != std::string_view::npos && source.Current() == original,
        "different source bytes cannot publish under a declared digest pin");

  const auto temporary =
      std::filesystem::temp_directory_path() /
      ("outshine-osm-source-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".osm");
  const auto changed = Region(temporary.string(),
                              "r2",
                              {.WestDeg = 0.0, .SouthDeg = 0.0, .EastDeg = 0.01, .NorthDeg = 0.01});
  {
    std::ofstream file(temporary, std::ios::binary);
    file << "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
            "<way id='9'><nd ref='1'/><nd ref='2'/>"
            "<tag k='highway' v='residential'/></way></osm>";
  }
  CHECK(source.Request(std::span(&changed, 1), "."), "replacement source is queued");
  CHECK(WaitFor(source, tasks) && source.CurrentPhase() == OsmTransportLoader::Phase::Failed &&
            source.Error().find("9") != std::string_view::npos &&
            source.Error().find("2") != std::string_view::npos,
        "missing source node rejects the replacement with IDs");
  CHECK(source.Current() == original, "failed replacement retains the prior graph");

  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    file << "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
            "<node id='2' lat='0' lon='0.001'/>"
            "<way id='9'><nd ref='1'/><nd ref='2'/>"
            "<tag k='highway' v='residential'/></way></osm>";
  }
  CHECK(source.Request(std::span(&changed, 1), "."), "same revision retries after failure");
  CHECK(WaitFor(source, tasks) && source.CurrentPhase() == OsmTransportLoader::Phase::Ready &&
            source.Current() && source.Current()->Topology().FindNode(2) != nullptr,
        "valid retry atomically replaces the failed candidate");

  std::promise<void> release;
  const auto gate = release.get_future().share();
  const Tasks::Handle blocker = tasks.Post([gate] { gate.wait(); });
  const auto stale = Region(hockenheim.Location, "r3", *hockenheim.Coverage);
  const auto newest = Region(hockenheim.Location, "r4", *hockenheim.Coverage);
  CHECK(source.Request(std::span(&stale, 1), "."), "older revision is queued behind a gate");
  CHECK(source.Request(std::span(&newest, 1), "."), "newer revision supersedes it");
  release.set_value();
  tasks.Wait(blocker);
  CHECK(WaitFor(source, tasks) && source.CurrentPhase() == OsmTransportLoader::Phase::Ready &&
            source.Current() && source.Current()->SourceIdentity().Revision == "r4",
        "late older result cannot replace the requested source revision");

  std::error_code cleanupError;
  std::filesystem::remove(temporary, cleanupError);
  CHECK(!cleanupError, "temporary negative-control source is removed");
  return Report();
}
