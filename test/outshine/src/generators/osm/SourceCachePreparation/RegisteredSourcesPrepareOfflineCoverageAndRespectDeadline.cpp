#include "OsmSourceCachePreparation.h"
#include "OsmProvider.h"
#include "OfflineTransport.h"
#include "SourceProviderValidation.h"
#include "Check.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {
class Wire final : public outshine::Data::Transport {
public:
  std::atomic<int> Starts{0};
  std::atomic<int> Cancels{0};
  bool Pending = false;

  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(++Starts);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    if (Pending) { return outshine::Data::Wire::Working(); }
    const std::string xml = "<osm version='0.6'><node id='1' lat='0.1' lon='0.1'/></osm>";
    return outshine::Data::Wire::Answered(200, std::vector<uint8_t>(xml.begin(), xml.end()));
  }

  void Cancel(outshine::Data::Ticket) override { ++Cancels; }
};

class RegisteredProvider final : public outshine::Data::Provider {
public:
  mutable std::atomic<int> Calls{0};

  std::string_view kind() const override { return "osm"; }

  std::expected<std::unique_ptr<outshine::Data::Source>, std::string>
  make(const outshine::Data::SourceProvider &declaration, std::string_view root) const override {
    ++Calls;
    return outshine::Generators::Osm::Provider{}.make(declaration, root);
  }
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-source-prepare-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated source cache exists");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const Data::SourceProvider declaration{.Kind = "osm",
                                         .Revision = "r1",
                                         .Missing = Data::MissingDataPolicy::Fail,
                                         .Dataset = "openstreetmap.original",
                                         .Endpoint = std::string(Data::kOfficialOsmApi)};
  const std::array cells{Data::GeoCellId{.Level = 9, .X = 256, .Y = 256}};
  Generators::Osm::SourceCacheRequest request{
      .Catalogue = declaration,
      .Cells = cells,
      .Limits = {.CellsMost = 32, .SnapshotBytesMost = 1024 * 1024},
      .ShippedRoot = ".",
      .CacheDirectory = directory,
      .BudgetS = 2};
  Tasks compute(1);
  Tasks io(1);
  Wire wire;
  RegisteredProvider provider;
  Data::ProviderRegistry registry;
  CHECK(registry.registerProvider(provider), "the external provider is publicly registered");
  size_t completed = 0;
  size_t required = 0;
  CHECK(Generators::Osm::PrepareSourceCache(request,
                                            {compute, io},
                                            wire,
                                            registry,
                                            [&](Generators::Osm::SourceCacheProgress how) {
                                              completed = how.ValidatedCells;
                                              required = how.RequiredCells;
                                            }),
        "preparation finishes through the supplied provider and borrowed shared workers");
  CHECK(completed == 1 && required == 1 && wire.Starts == 1 && provider.Calls >= 2,
        "acquisition and final cache proof both use the registered provider");
  Data::OfflineTransport offline;
  CHECK(Generators::Osm::PrepareSourceCache(request, {compute, io}, offline, registry),
        "a fresh offline preparation validates persisted original bytes");
  wire.Pending = true;
  request.CacheDirectory = directory + "/pending";
  request.BudgetS = 0.03;
  const auto began = std::chrono::steady_clock::now();
  const auto failed = Generators::Osm::PrepareSourceCache(request, {compute, io}, wire, registry);
  const auto elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
  CHECK(!failed && failed.error().find("deadline") != std::string::npos && elapsed < 1 &&
            wire.Cancels > 0,
        "one total deadline cancels pending IO and returns without detached work");
  std::filesystem::remove_all(directory);
  return Report();
}
