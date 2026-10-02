#include "Check.h"
#include "OsmSourceAcquisition.h"
#include "SourceProviderValidation.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <system_error>
#include <thread>

namespace {
class ApiWire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0}, Canceled{0}, Active{0};
  std::atomic<int> BlockAfter{0};
  std::atomic<bool> Block{false}, Large{false}, MainIo{false};
  const std::thread::id Main = std::this_thread::get_id();
  std::map<outshine::Data::Ticket, double> Longitude;

  outshine::Data::FetchStart Begin(const std::string &url) override {
    MainIo = MainIo || std::this_thread::get_id() == Main;
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    double west = 0;
    const auto at = url.find("bbox=") + 5;
    (void)std::from_chars(url.data() + at, url.data() + url.size(), west);
    Longitude.emplace(ticket, west + 0.1);
    ++Active;
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    if (Block || (BlockAfter != 0 && static_cast<int>(ticket) >= BlockAfter)) {
      return outshine::Data::Wire::Working();
    }
    const auto found = Longitude.find(ticket);
    const std::string xml = "<osm version='0.6'><node id='1' lat='54.7' lon='" +
                            std::to_string(found->second) + "'><tag k='custom:unknown' v='" +
                            (Large ? std::string(100000, 'x') : std::string("kept")) +
                            "'/></node></osm>";
    Longitude.erase(found);
    --Active;
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override {
    if (Longitude.erase(ticket) != 0) {
      ++Canceled;
      --Active;
    }
  }
};

template <typename Ready>
bool Await(outshine::Generators::Osm::SourceAcquisition &loader, Ready ready) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    loader.Poll();
    if (ready()) { return true; }
    (void)loader.AwaitSlice(0.01);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

bool Settled(outshine::Generators::Osm::SourceAcquisition &loader) {
  return Await(loader, [&loader] {
    return loader.CurrentPhase() != outshine::Generators::Osm::SourceAcquisition::Phase::Loading &&
           loader.PendingCount() == 0;
  });
}

size_t ActiveCharge(const outshine::Generators::Osm::SourceAcquisition &loader) {
  size_t bytes = 0;
  for (const auto &entry : loader.CurrentCells()) { bytes += entry.ChargedBytes; }
  return bytes;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-cell-residency-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-response cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    ApiWire wire;
    Tasks compute(1);
    Tasks io(1);
    outshine::Generators::Osm::SourceAcquisition loader(compute, io, &wire, directory);
    const SourceProvider provider{.Kind = "osm",
                                  .Revision = "resident-r1",
                                  .Missing = MissingDataPolicy::Fail,
                                  .Dataset = "openstreetmap.original",
                                  .Endpoint = std::string(kOfficialOsmApi)};
    const std::array cells{GeoCellId{.Level = 9, .X = 269, .Y = 411},
                           GeoCellId{.Level = 9, .X = 270, .Y = 411},
                           GeoCellId{.Level = 9, .X = 271, .Y = 411}};
    const outshine::Generators::Osm::SourceAcquisition::CellLimits limits{
        .CellsMost = 8, .SnapshotBytesMost = 1024 * 1024};
    CHECK(loader.RequestCells(provider, std::span(cells).first(3), limits, ".") &&
              Settled(loader) &&
              loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready &&
              loader.CurrentCells().size() == 3 && !loader.Current() && wire.Starts == 3 &&
              !wire.MainIo,
          "a demand beyond one two-cell batch publishes separate snapshots off the main thread");
    if (loader.CurrentCells().size() != 3) { return Report(); }
    auto old = loader.CurrentCells()[0].Snapshot;
    const auto retained = loader.CurrentCells()[1].Snapshot;
    const auto oldCharge = loader.CurrentCells()[0].ChargedBytes;
    CHECK(loader.CellSnapshotChargeBytes() == ActiveCharge(loader) &&
              retained->Elements.FindNode(1)->Tags.front().Value == "kept",
          "original tags and independently charged resident sources remain available");
    const std::array reversed{cells[2], cells[1], cells[0]};
    CHECK(loader.RequestCells(provider, reversed, limits, ".") &&
              loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready &&
              loader.PendingCount() == 0 && wire.Starts == 3,
          "camera-equivalent demand order performs no IO or recomputation");
    CHECK(loader.RequestCells(provider, std::span(cells).subspan(1), limits, ".") &&
              Settled(loader) && loader.CurrentCells().size() == 2 && wire.Starts == 3 &&
              loader.CurrentCells()[0].Snapshot == retained,
          "movement reuses the exact retained snapshots instead of rebuilding the region");
    CHECK(loader.CellSnapshotChargeBytes() == ActiveCharge(loader) + oldCharge,
          "a retired source still pinned by a product remains charged to the source owner");
    old.reset();
    CHECK(loader.CellSnapshotChargeBytes() == ActiveCharge(loader),
          "the charge disappears only after the final source pin is released");
    const std::array extended{cells[1], cells[2], GeoCellId{.Level = 9, .X = 272, .Y = 411}};
    const auto beforeExtension = wire.Starts.load();
    CHECK(loader.RequestCells(provider, extended, limits, ".") &&
              loader.CurrentCells().size() == 2 && Settled(loader) &&
              loader.CurrentCells().size() == 3 && loader.CurrentCells()[0].Snapshot == retained &&
              wire.Starts == beforeExtension + 1,
          "movement acquires only the added cell and atomically keeps both existing sources");
    CHECK(loader.RequestCells(provider, std::span(cells).subspan(1), limits, ".") &&
              Settled(loader) && loader.CellSnapshotChargeBytes() == ActiveCharge(loader),
          "leaving the added region releases it without regenerating the remaining cells");
    const auto starts = wire.Starts.load();
    const std::array duplicate{cells[1], cells[1]};
    const std::array invalid{GeoCellId{.Level = 8}};
    CHECK(
        !loader.RequestCells(provider, duplicate, limits, ".") &&
            !loader.RequestCells(provider, invalid, limits, ".") &&
            !loader.RequestCells(
                provider, cells, {.CellsMost = 2, .SnapshotBytesMost = 1024}, ".") &&
            wire.Starts == starts && loader.CurrentCells()[0].Snapshot == retained,
        "invalid or excessive demand never truncates the requested world or destroys the old set");

    auto changed = provider;
    changed.Revision = "resident-r2";
    wire.Large = true;
    CHECK(loader.RequestCells(changed,
                              std::span(cells).subspan(1, 1),
                              {.CellsMost = 8, .SnapshotBytesMost = ActiveCharge(loader) + 4096},
                              ".") &&
              Settled(loader) &&
              loader.CurrentPhase() ==
                  outshine::Generators::Osm::SourceAcquisition::Phase::Failed &&
              loader.CurrentCells()[0].Snapshot == retained &&
              loader.CellSnapshotChargeBytes() == ActiveCharge(loader),
          "large retained tag buffers exceed the snapshot budget and preserve the valid old world");
    wire.Large = false;
    CHECK(loader.RequestCells(provider, std::span(cells).subspan(1), limits, ".") &&
              Settled(loader) &&
              loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready,
          "returning to a retained source revision recovers without losing products");

    changed.Revision = "resident-r3";
    wire.Block = true;
    const auto before = wire.Starts.load();
    CHECK(loader.RequestCells(changed, std::span(cells).subspan(1), limits, ".") &&
              Await(loader, [&wire, before] { return wire.Starts >= before + 2; }),
          "a source replacement starts both bounded IO requests");
    const auto probe = compute.Post([] {});
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    bool probeDone = false;
    while (!(probeDone = compute.TakeCompletion(probe)) &&
           std::chrono::steady_clock::now() < until) {
      (void)compute.AwaitCompletion(0.01);
    }
    CHECK(probeDone && loader.CurrentCells()[0].Snapshot == retained,
          "stalled IO leaves the shared compute worker and published world available");
    CHECK(loader.RequestCells(provider, std::span(cells).subspan(1), limits, ".") &&
              Settled(loader) && wire.Canceled == 2 && wire.Active == 0 &&
              loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready &&
              loader.CurrentCells()[0].Snapshot == retained,
          "superseded source work cancels every ticket and cannot overwrite retained cells");
    wire.Block = false;
    changed.Revision = "resident-partial";
    const auto beforePartial = wire.Starts.load();
    const auto publishedCharge = loader.CellSnapshotChargeBytes();
    wire.BlockAfter = beforePartial + 3;
    CHECK(loader.RequestCells(changed, cells, limits, ".") &&
              Await(loader,
                    [&loader, &wire, beforePartial, publishedCharge] {
                      return wire.Starts >= beforePartial + 3 &&
                             loader.CellSnapshotChargeBytes() > publishedCharge;
                    }),
          "two completed cells remain staged while the final requested source is blocked");
    const auto stagedCharge = loader.CellSnapshotChargeBytes();
    const std::array redirected{cells[0], cells[1], extended[2]};
    CHECK(loader.RequestCells(changed, redirected, limits, ".") &&
              loader.CellSnapshotChargeBytes() == stagedCharge,
          "changed demand retains completed unpublished cells instead of re-ingesting them");
    wire.BlockAfter = 0;
    CHECK(Settled(loader) &&
              loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready &&
              loader.CurrentCells().size() == 3 && wire.Starts == beforePartial + 4,
          "redirecting a partial load acquires only its newly missing source");
    CHECK(loader.Request({}, ".") && Settled(loader) && loader.CurrentCells().empty() &&
              !loader.Current(),
          "regional-mode removal does not expose former cell snapshots as a global source");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  return Report();
}
