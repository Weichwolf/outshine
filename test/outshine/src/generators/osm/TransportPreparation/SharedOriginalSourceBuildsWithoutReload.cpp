#include "OsmTransportPreparation.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::shared_ptr<const outshine::Data::OsmSourceSnapshot> Source(std::string_view nodes) {
  auto parsed = outshine::Data::OsmXmlReader::Read(
      "<osm version='0.6'>" + std::string(nodes) +
          "<way id='10'><nd ref='1'/><nd ref='2'/><tag k='highway' v='residential'/></way></osm>",
      {.DatasetId = "shared-original", .Revision = "r1"});
  if (!parsed) { return {}; }
  return std::make_shared<const outshine::Data::OsmSourceSnapshot>(
      outshine::Data::OsmSourceSnapshot{.Elements = std::move(*parsed)});
}

bool WaitFor(outshine::Generators::Osm::TransportPreparation &loader, outshine::Tasks &tasks) {
  for (int attempt = 0; attempt < 200; ++attempt) {
    loader.Poll();
    if (loader.CurrentPhase() != outshine::Generators::Osm::TransportPreparation::Phase::Loading) {
      return true;
    }
    (void)tasks.AwaitCompletion(0.05);
  }
  return false;
}

}

int main() {
  using namespace outshine;
  using namespace outshine::World;
  using namespace outshine::Test;
  Tasks tasks(1);
  outshine::Generators::Osm::TransportPreparation loader(tasks);
  auto source = Source("<node id='1' lat='0' lon='0'/><node id='2' lat='0' lon='0.001'/>");
  CHECK(source != nullptr, "original source fixture parses");
  if (!source) { return Report(); }
  const auto *identity = source.get();
  CHECK(loader.RequestSource(source).has_value(), "already loaded source enters async graph build");
  source.reset();
  CHECK(WaitFor(loader, tasks) &&
            loader.CurrentPhase() == outshine::Generators::Osm::TransportPreparation::Phase::Ready,
        "graph builds from source memory without a provider or file path");
  if (!loader.Current()) { return Report(); }
  CHECK(loader.Source().get() == identity,
        "engine source owner retains original elements without copying them");
  const auto published = loader.Current();
  const auto completed = loader.CompletedCount();
  CHECK(loader.RequestSource(loader.Source()).has_value(), "same source may be requested again");
  loader.Poll();
  CHECK(loader.PendingCount() == 0 && loader.CompletedCount() == completed,
        "unchanged source does not rebuild its graph");
  const auto incomplete = Source("<node id='1' lat='0' lon='0'/>");
  CHECK(loader.RequestSource(incomplete).has_value(), "replacement is admitted independently");
  CHECK(WaitFor(loader, tasks) &&
            loader.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Failed &&
            loader.Current() == published,
        "failed graph replacement retains the native graph and engine source owner");
  CHECK(incomplete && incomplete->Elements.FindWay(10) != nullptr,
        "graph failure does not consume or invalidate shared original data");
  const std::weak_ptr<const Data::OsmSourceSnapshot> oldSource = loader.Source();
  CHECK(loader.Request({}, ".").has_value() &&
            loader.CurrentPhase() ==
                outshine::Generators::Osm::TransportPreparation::Phase::Inactive &&
            !loader.Current(),
        "legacy empty request also clears snapshot-backed input");
  CHECK(oldSource.expired() && published->Topology().FindNode(1) != nullptr,
        "retained native publication does not pin the released original archive");
  return Report();
}
