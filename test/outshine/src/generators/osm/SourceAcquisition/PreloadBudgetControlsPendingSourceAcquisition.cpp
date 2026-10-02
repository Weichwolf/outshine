#include "Check.h"
#include "OsmSourceAcquisition.h"
#include "SourceProviderValidation.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <thread>

namespace {
class DelayedWire final : public outshine::Data::Transport {
public:
  std::atomic<double> ClockMs{0};
  std::atomic<bool> Released{false};
  size_t Canceled = 0;
  double ReadyAtMs = 12000;

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (ClockMs < ReadyAtMs) { return outshine::Data::Wire::Working(); }
    const std::string body = "<osm version='0.6'><node id='1' lat='0' lon='0'/></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(body.begin(), body.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Canceled; }

  double NowMs() override { return ClockMs; }

  bool Await(double ms) override {
    if (!Released && ClockMs >= 3000) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    } else {
      ClockMs.fetch_add(ms);
    }
    return false;
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
}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-budget-cells-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  SourceProvider provider{.Kind = "osm",
                          .Revision = "longer",
                          .Missing = MissingDataPolicy::Fail,
                          .Dataset = "openstreetmap.original",
                          .Endpoint = std::string(kOfficialOsmApi)};
  const std::array cells{GeoCellId{.Level = 9, .X = 256, .Y = 256}};
  const outshine::Generators::Osm::SourceAcquisition::CellLimits limits{
      .CellsMost = 1, .SnapshotBytesMost = 1024 * 1024};
  Tasks compute(1);
  Tasks io(1);
  DelayedWire wire;
  wire.ReadyAtMs = 21000;
  outshine::Generators::Osm::SourceAcquisition loader(compute, io, &wire, directory);
  CHECK(loader.RequestCells(provider, cells, limits, ".") &&
            Await(loader, [&wire] { return wire.ClockMs >= 3000; }),
        "source IO is pending before the caller updates its budget");
  CHECK(loader.SetAcquisitionBudget(20).has_value(), "longer preparation budget accepted");
  wire.Released = true;
  CHECK(Await(loader,
              [&loader] {
                return loader.CurrentPhase() !=
                       outshine::Generators::Osm::SourceAcquisition::Phase::Loading;
              }) &&
            loader.CurrentPhase() == outshine::Generators::Osm::SourceAcquisition::Phase::Ready &&
            loader.CurrentCells().size() == 1 && wire.ClockMs == 21000 && wire.Canceled == 0,
        "the renewed caller budget starts when granted instead of at the earlier source epoch");
  for (const double invalid : {-1.0,
                               std::numeric_limits<double>::max(),
                               std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!loader.SetAcquisitionBudget(invalid),
          "invalid budgets cannot replace the valid source limit");
  }
  DelayedWire bounded;
  bounded.Released = true;
  outshine::Generators::Osm::SourceAcquisition shortLoader(compute, io, &bounded, directory);
  provider.Revision = "shorter";
  CHECK(shortLoader.SetAcquisitionBudget(5) &&
            shortLoader.RequestCells(provider, cells, limits, ".") &&
            Await(shortLoader,
                  [&shortLoader] {
                    return shortLoader.CurrentPhase() !=
                           outshine::Generators::Osm::SourceAcquisition::Phase::Loading;
                  }) &&
            shortLoader.CurrentPhase() ==
                outshine::Generators::Osm::SourceAcquisition::Phase::Failed &&
            bounded.ClockMs == 5000 && bounded.Canceled == 1,
        "a short preload keeps its own bound and cancels unfinished source IO");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source cache removed");
  return Report();
}
