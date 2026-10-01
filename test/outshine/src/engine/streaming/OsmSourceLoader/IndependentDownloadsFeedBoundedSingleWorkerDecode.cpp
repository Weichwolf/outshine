#include "Check.h"
#include "OsmSourceLoader.h"
#include "SourceProviderValidation.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <system_error>
#include <thread>

namespace {
class DelayedCellTransport final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0}, Active{0}, Peak{0};
  std::atomic<bool> ReleaseFirst{false}, MainIo{false};
  const std::thread::id Main = std::this_thread::get_id();
  std::map<outshine::Data::Ticket, double> Longitude;

  outshine::Data::FetchStart Begin(const std::string &url) override {
    MainIo = MainIo || std::this_thread::get_id() == Main;
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    double west = 0;
    const auto at = url.find("bbox=") + 5;
    (void)std::from_chars(url.data() + at, url.data() + url.size(), west);
    Longitude.emplace(ticket, west + 0.1);
    Peak = std::max(Peak.load(), ++Active);
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    if (!ReleaseFirst && ticket == static_cast<outshine::Data::Ticket>(1)) {
      return outshine::Data::Wire::Working();
    }
    const auto longitude = Longitude.at(ticket);
    Longitude.erase(ticket);
    --Active;
    const std::string xml = "<osm version='0.6'><node id='" +
                            std::to_string(static_cast<uint64_t>(ticket)) + "' lat='54.7' lon='" +
                            std::to_string(longitude) +
                            "'><tag k='custom:unknown' v='kept'/></node></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override {
    if (Longitude.erase(ticket)) { --Active; }
  }
};

template <typename Ready> bool Await(outshine::OsmSourceLoader &loader, Ready ready) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
  do {
    loader.Poll();
    if (ready()) { return true; }
    (void)loader.AwaitSlice(0.005);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-osm-pipeline-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated pipeline cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  {
    Tasks compute(1);
    std::atomic<bool> releaseCompute{false}, computeEntered{false}, guardExpired{false};
    const auto blocker = compute.Post([&] {
      computeEntered = true;
      const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (!releaseCompute && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      guardExpired = !releaseCompute;
    });
    DelayedCellTransport wire;
    OsmSourceLoader loader(compute, &wire, directory);
    const SourceProvider provider{.Kind = "osm",
                                  .Revision = "pipeline",
                                  .Missing = MissingDataPolicy::Fail,
                                  .Dataset = "openstreetmap.original",
                                  .Endpoint = std::string(kOfficialOsmApi)};
    std::array<GeoCellId, 10> cells{};
    for (size_t index = 0; index < cells.size(); ++index) {
      cells[index] = {.Level = 9, .X = static_cast<uint32_t>(269 + index), .Y = 411};
    }
    CHECK(loader.RequestCells(
              provider, cells, {.CellsMost = 16, .SnapshotBytesMost = 1024 * 1024}, ".") &&
              Await(loader, [&] { return wire.Starts == 8 && computeEntered; }) &&
              loader.CellSnapshotChargeBytes() == 0 && loader.CurrentCells().empty() &&
              !wire.MainIo,
          "eight original downloads start independently while the sole compute worker is occupied");
    releaseCompute = true;
    compute.Wait(blocker);
    CHECK(
        !guardExpired &&
            Await(loader,
                  [&] { return loader.CellSnapshotChargeBytes() > 0 && wire.Starts == 10; }) &&
            loader.CurrentCells().empty() && !wire.ReleaseFirst,
        "ready neighbors parse and refill download slots without waiting for the slow first cell");
    wire.ReleaseFirst = true;
    CHECK(Await(loader,
                [&] {
                  return loader.CurrentPhase() != OsmSourceLoader::Phase::Loading &&
                         loader.PendingCount() == 0;
                }) &&
              loader.CurrentPhase() == OsmSourceLoader::Phase::Ready &&
              loader.CurrentCells().size() == 10 && wire.Starts == 10 && wire.Active == 0 &&
              wire.Peak <= 8,
          "all cells publish once with bounded concurrency and no incomplete replacement");
    for (const auto &cell : loader.CurrentCells()) {
      CHECK(cell.Snapshot->Elements.Nodes().front().Tags.front().Value == "kept",
            "original unknown tags survive pipelined native decoding");
    }
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated pipeline cache removed");
  return Report();
}
