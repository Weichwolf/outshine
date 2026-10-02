#include "OsmSourceAcquisition.h"
#include "OfflineTransport.h"
#include "SourceProviderValidation.h"
#include "Check.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using Loader = outshine::Generators::Osm::SourceAcquisition;

class ApiWire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0};
  const std::string Xml = "<osm version='0.6'><node id='1' lat='0' lon='0'>"
                          "<tag k='custom:unknown' v='original'/></node></osm>";

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(Xml.begin(), Xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override {}
};

bool WaitFor(Loader &loader) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    loader.Poll();
    const auto phase = loader.CurrentPhase();
    if (phase != Loader::Phase::Loading && phase != Loader::Phase::Verifying) { return true; }
    (void)loader.AwaitSlice(0.01);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-cache-target-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original source cache exists");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const Data::SourceProvider source{.Kind = "osm",
                                    .Revision = "r1",
                                    .Missing = Data::MissingDataPolicy::Fail,
                                    .Dataset = "openstreetmap.original",
                                    .Endpoint = std::string(Data::kOfficialOsmApi)};
  std::vector<Data::GeoCellId> cells;
  for (uint32_t x = 256; x < 276; ++x) { cells.push_back({.Level = 9, .X = x, .Y = 256}); }
  Tasks compute(1);
  Tasks io(1);
  ApiWire wire;
  size_t singleBytes = 0;
  {
    Loader prime({.Compute = compute, .Io = io}, &wire, directory);
    CHECK(prime.RequestCells(source, std::span(cells).first(1), {1, 1024 * 1024}, ".") &&
              WaitFor(prime) && prime.CurrentPhase() == Loader::Phase::Ready &&
              prime.CurrentCells().size() == 1,
          "one original source establishes the measured decoded snapshot charge");
    if (!prime.CurrentCells().empty()) { singleBytes = prime.CurrentCells().front().ChargedBytes; }
  }
  CHECK(singleBytes > 0, "the fixture has a nonzero decoded source charge");
  if (singleBytes == 0) { return Report(); }
  const Loader::CellLimits limits{.CellsMost = cells.size(), .SnapshotBytesMost = 3 * singleBytes};
  {
    Loader cache({.Compute = compute, .Io = io}, &wire, directory, Loader::Target::Cache);
    CHECK(cache.RequestCells(source, cells, limits, ".") && WaitFor(cache) &&
              cache.CurrentPhase() == Loader::Phase::Ready &&
              cache.PreparedCellCount() == cells.size(),
          "twenty cells complete although their sum exceeds a three-cell decoded retention budget");
    CHECK(cache.CellSnapshotChargeBytes() == 0 && cache.CurrentCells().empty() && !cache.Current(),
          "cache preparation releases decoded inputs and does not claim resident world inputs");
  }
  {
    Data::OfflineTransport offline;
    Loader cached({.Compute = compute, .Io = io}, &offline, directory, Loader::Target::Cache);
    CHECK(cached.RequestCells(source, cells, limits, ".") && WaitFor(cached) &&
              cached.CurrentPhase() == Loader::Phase::Ready &&
              cached.PreparedCellCount() == cells.size(),
          "a fresh offline instance independently proves every original source cell is cached");
    size_t corrupted = 0;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(directory)) {
      if (!entry.is_regular_file()) { continue; }
      std::ifstream in(entry.path(), std::ios::binary);
      const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
      if (bytes != wire.Xml) { continue; }
      std::ofstream out(entry.path(), std::ios::binary | std::ios::trunc);
      out << "corrupt";
      ++corrupted;
      break;
    }
    CHECK(corrupted == 1, "one controlled original payload is corrupted");
    CHECK(cached.RequestCells(source, cells, limits, ".") && WaitFor(cached) &&
              cached.CurrentPhase() == Loader::Phase::Failed,
          "repeated cache preparation verifies bytes instead of trusting old completion markers");
  }
  {
    Loader resident({.Compute = compute, .Io = io}, &wire, directory);
    CHECK(resident.RequestCells(source, cells, limits, ".") && WaitFor(resident) &&
              resident.CurrentPhase() == Loader::Phase::Failed &&
              resident.Error().find("budget") != std::string_view::npos,
          "normal world-input acquisition retains its strict decoded snapshot budget");
  }
  std::filesystem::remove_all(directory);
  return Report();
}
