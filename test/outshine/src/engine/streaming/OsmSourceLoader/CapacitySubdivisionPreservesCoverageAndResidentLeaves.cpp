#include "Check.h"
#include "OsmSourceLoader.h"
#include "SourceProviderValidation.h"

#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <map>
#include <thread>

namespace {
class ApiWire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0}, Blocked{0};
  std::atomic<bool> BlockLeaves{false};
  double WidthMost = 0.4;
  std::map<outshine::Data::Ticket, std::array<double, 4>> Bounds;

  outshine::Data::FetchStart Begin(const std::string &url) override {
    std::array<double, 4> bounds{};
    const char *at = url.data() + url.find("bbox=") + 5;
    for (auto &coordinate : bounds) {
      at = std::from_chars(at, url.data() + url.size(), coordinate).ptr;
      if (at != url.data() + url.size()) { ++at; }
    }
    const auto ticket = static_cast<outshine::Data::Ticket>(++Starts);
    Bounds.emplace(ticket, bounds);
    return ticket;
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    const auto bounds = Bounds.at(ticket);
    const bool crowded = bounds[2] - bounds[0] > WidthMost;
    if (!crowded && BlockLeaves) {
      ++Blocked;
      return outshine::Data::Wire::Working();
    }
    Bounds.erase(ticket);
    const std::string body = crowded ? "You requested too many nodes (limit is 50000). Either "
                                       "request a smaller area, or use planet.osm"
                                     : "<osm version='0.6'><node id='" + std::to_string(ticket) +
                                           "' lat='" + std::to_string((bounds[1] + bounds[3]) / 2) +
                                           "' lon='" + std::to_string((bounds[0] + bounds[2]) / 2) +
                                           "'><tag k='custom:unknown' v='kept'/></node></osm>";
    return outshine::Data::Wire::Answered(crowded ? 400 : 200,
                                          std::vector<uint8_t>(body.begin(), body.end()));
  }

  void Cancel(outshine::Data::Ticket ticket) override { Bounds.erase(ticket); }
};

template <typename Ready> bool Await(outshine::OsmSourceLoader &loader, Ready ready) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    loader.Poll();
    if (ready()) { return true; }
    (void)loader.AwaitSlice(0.01);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

bool Settled(outshine::OsmSourceLoader &loader) {
  return Await(loader, [&loader] {
    return loader.CurrentPhase() != outshine::OsmSourceLoader::Phase::Loading &&
           loader.PendingCount() == 0;
  });
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  const SourceProvider provider{.Kind = "osm",
                                .Revision = "adaptive",
                                .Missing = MissingDataPolicy::Fail,
                                .Dataset = "openstreetmap.original",
                                .Endpoint = std::string(kOfficialOsmApi)};
  const std::array roots{GeoCellId{.Level = 9, .X = 270, .Y = 411}};
  const OsmSourceLoader::CellLimits limits{.CellsMost = 32, .SnapshotBytesMost = 1024 * 1024};
  Tasks compute(1);
  ApiWire wire;
  OsmSourceLoader loader(compute, &wire);
  CHECK(loader.RequestCells(provider, roots, limits, ".") && Settled(loader) &&
            loader.CurrentPhase() == OsmSourceLoader::Phase::Ready &&
            loader.CurrentCells().size() == 4 && wire.Starts == 5,
        "one overloaded parent is replaced by all four children without retrying its request");
  if (loader.CurrentCells().size() != 4) { return Report(); }
  double area = 0;
  for (const auto &entry : loader.CurrentCells()) {
    const auto cell = *entry.Snapshot->Cell;
    const auto bounds = *cell.Bounds();
    area += (bounds.EastDeg - bounds.WestDeg) * (bounds.NorthDeg - bounds.SouthDeg);
    CHECK(cell.Level == 10 && cell.X / 2 == roots[0].X && cell.Y / 2 == roots[0].Y,
          "each source belongs to a distinct direct child of the requested root");
  }
  const auto parent = *roots[0].Bounds();
  CHECK(area == (parent.EastDeg - parent.WestDeg) * (parent.NorthDeg - parent.SouthDeg),
        "published child coverage has the complete requested area");
  const auto retained = loader.CurrentCells().front().Snapshot;
  CHECK(loader.RequestCells(provider, roots, limits, ".") && loader.PendingCount() == 0 &&
            wire.Starts == 5 && loader.CurrentCells().front().Snapshot == retained,
        "unchanged root demand reuses the adaptive leaf plan and exact resident snapshots");
  const std::array moved{roots[0], GeoCellId{.Level = 9, .X = 271, .Y = 411}};
  wire.BlockLeaves = true;
  CHECK(loader.RequestCells(provider, moved, limits, ".") &&
            Await(loader, [&wire] { return wire.Blocked != 0; }) &&
            loader.CurrentCells().size() == 4 && loader.CurrentCells().front().Snapshot == retained,
        "a partially acquired new root never replaces the complete published world");
  wire.BlockLeaves = false;
  CHECK(Settled(loader) && loader.CurrentCells().size() == 8 && wire.Starts == 10,
        "movement acquires only the new parent and its four leaves");
  CHECK(loader.RequestCells(provider, roots, limits, ".") && Settled(loader) &&
            loader.CurrentCells().size() == 4 && wire.Starts == 10 &&
            loader.CurrentCells().front().Snapshot == retained,
        "leaving a root retains the other root's adaptive partition without IO");
  auto revision = provider;
  revision.Revision = "adaptive-capacity";
  CHECK(loader.RequestCells(
            revision, roots, {.CellsMost = 3, .SnapshotBytesMost = 1024 * 1024}, ".") &&
            Settled(loader) && loader.CurrentPhase() == OsmSourceLoader::Phase::Failed &&
            loader.Error().find("cell budget") != std::string_view::npos &&
            loader.CurrentCells().front().Snapshot == retained,
        "exhausting subdivision capacity preserves the previous world and reports failure");
  revision.Revision = "adaptive-depth";
  CHECK(loader.RequestCells(revision,
                            roots,
                            {.CellsMost = 32, .SnapshotBytesMost = 1024 * 1024, .LevelMost = 9},
                            ".") &&
            Settled(loader) && loader.CurrentPhase() == OsmSourceLoader::Phase::Failed &&
            loader.Error().find("level budget") != std::string_view::npos,
        "the depth bound terminates overloaded demand explicitly");
  ApiWire nested;
  nested.WidthMost = 0.2;
  OsmSourceLoader deeper(compute, &nested);
  CHECK(deeper.RequestCells(provider, roots, limits, ".") && Settled(deeper) &&
            deeper.CurrentPhase() == OsmSourceLoader::Phase::Ready &&
            deeper.CurrentCells().size() == 16 && nested.Starts == 21,
        "repeated subdivision publishes sixteen leaves after one parent and four child refusals");
  return Report();
}
