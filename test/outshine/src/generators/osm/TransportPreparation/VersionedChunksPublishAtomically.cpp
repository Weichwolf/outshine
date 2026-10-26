#include "HockenheimCircuitFixture.h"
#include "OsmTransportPreparation.h"
#include "Check.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <span>
#include <ios>
#include <string>
#include <system_error>
#include <utility>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {

bool WaitFor(outshine::Generators::Osm::TransportPreparation &source, outshine::Tasks &tasks) {
  constexpr int mostPolls = 200;
  constexpr double waitSliceS = 0.05;
  for (int attempt = 0; attempt < mostPolls; ++attempt) {
    source.Poll();
    if (source.CurrentPhase() != outshine::Generators::Osm::TransportPreparation::Phase::Loading) {
      return true;
    }
    (void)tasks.AwaitCompletion(waitSliceS);
  }
  return false;
}

outshine::Data::SourceProvider Region(const std::filesystem::path &location,
                                      std::string revision,
                                      outshine::GeographicBounds coverage) {
  auto source = Circuit::LocalSource();
  source.Location = location.string();
  source.Revision = std::move(revision);
  source.Coverage = coverage;
  return source;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using namespace outshine::World;

  Tasks tasks(1);
  outshine::Generators::Osm::TransportPreparation source(tasks);
  const auto hockenheim =
      Region(Circuit::SourcePath,
             "pin-r1",
             {.WestDeg = 8.54, .SouthDeg = 49.315, .EastDeg = 8.61, .NorthDeg = 49.34});
  const outshine::Generators::Osm::CircuitRequest mainRoute{.Id = "grand-prix",
                                                            .RelationId = Circuit::RelationId};
  CHECK(source.Request(std::span(&hockenheim, 1), ".", std::span(&mainRoute, 1)),
        "versioned regional source and named route are queued");
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() == outshine::Generators::Osm::TransportPreparation::Phase::Ready,
        "background import publishes a complete native graph");
  const auto original = source.Current();
  bool circuitValid = false;
  if (original) {
    const CircuitRoute *circuit = original->FindRoute("grand-prix");
    circuitValid = circuit != nullptr && circuit->EdgeIds.size() == Circuit::EdgeCount;
  }
  CHECK(original && original->SourceIdentity().Revision == "pin-r1" &&
            original->Metrics().SourceBytes > Circuit::MinimumReportedBytes && circuitValid,
        "published source IDs resolve the independent Hockenheim circuit oracle");
  if (!original) { return Report(); }

  constexpr uint64_t missingRelationId = 999999;
  const outshine::Generators::Osm::CircuitRequest missingRoute{.Id = "missing",
                                                               .RelationId = missingRelationId};
  CHECK(source.Request(std::span(&hockenheim, 1), ".", std::span(&missingRoute, 1)),
        "a replacement route request queues against the same source");
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Failed &&
            source.Error().contains("999999") && source.Current() == original &&
            source.Current()->FindRoute("grand-prix") != nullptr,
        "missing relation leaves the previous named route and graph published");

  auto wrongPin = hockenheim;
  wrongPin.Revision = "sha256:" + std::string(64, '0');
  CHECK(source.Request(std::span(&wrongPin, 1), "."), "well-formed digest pin is queued");
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Failed &&
            source.Error().contains("sha256") && source.Current() == original,
        "different source bytes cannot publish under a declared digest pin");

  const auto temporary =
      std::filesystem::temp_directory_path() /
      ("outshine-osm-source-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".osm");
  constexpr outshine::GeographicBounds changedBounds{
      .WestDeg = 0, .SouthDeg = 0, .EastDeg = 0.01, .NorthDeg = 0.01};
  const auto changed = Region(temporary.string(), "r2", changedBounds);
  {
    std::ofstream file(temporary, std::ios::binary);
    file << "<osm version='0.6'><node id='1' lat='0' lon='0'/>"
            "<way id='9'><nd ref='1'/><nd ref='2'/>"
            "<tag k='highway' v='residential'/></way></osm>";
  }
  CHECK(source.Request(std::span(&changed, 1), "."), "replacement source is queued");
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Failed &&
            source.Error().contains('9') && source.Error().contains('2'),
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
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Ready &&
            source.Current() && source.Current()->Topology().FindNode(2) != nullptr,
        "valid retry atomically replaces the failed candidate");

  CHECK(hockenheim.Coverage.has_value(), "fixture retains explicit source bounds");
  if (!hockenheim.Coverage) { return Report(); }
  std::promise<void> release;
  const auto gate = release.get_future().share();
  const Tasks::Handle blocker = tasks.Post([gate] { gate.wait(); });
  const auto stale = Region(hockenheim.Location, "r3", *hockenheim.Coverage);
  const auto newest = Region(hockenheim.Location, "r4", *hockenheim.Coverage);
  const auto latest = Region(hockenheim.Location, "r5", *hockenheim.Coverage);
  CHECK(source.Request(std::span(&stale, 1), "."), "older revision is queued behind a gate");
  CHECK(source.Request(std::span(&newest, 1), "."), "newer revision supersedes it");
  CHECK(source.Request(std::span(&latest, 1), ".") && source.PendingCount() == 1,
        "third revision replaces the desired state without queuing another build");
  release.set_value();
  tasks.Wait(blocker);
  CHECK(WaitFor(source, tasks) &&
            source.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Ready &&
            source.Current() && source.Current()->SourceIdentity().Revision == "r5" &&
            source.CanceledCount() == 1,
        "only the newest revision publishes after one stale build is canceled");

  std::error_code cleanupError;
  std::filesystem::remove(temporary, cleanupError);
  CHECK(!cleanupError, "temporary negative-control source is removed");
  return Report();
}
