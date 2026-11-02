#include "OpenAssetStore.h"
#include "AssetCache.h"
#include "Check.h"
#include "JobAdmission.h"
#include "OsmCacheFixture.h"
#include "OsmField.h"
#include "PreparedOsmTiles.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-source-error-owner-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  CHECK(!error, "the private storage-failure fixture directory opens");
  if (error) { return Report(); }
  const auto blocked = directory / "file";
  {
    std::ofstream file(blocked);
    file << "not a directory";
  }
  std::atomic_uint calls{0};
  const auto result = [&] -> std::expected<bool, std::string> {
    Tasks compute(1);
    OsmCache::NoTransport transport;
    auto session = OsmCache::OpenSession(
        std::make_unique<OsmCache::VectorSource>("storage-r1", std::vector<uint8_t>{}, calls));
    CHECK(session, "source metadata opens without source IO");
    if (!session) { return std::unexpected(session.error()); }
    auto prepared = PreparedOsmTiles::Open(
        outshine::Test::OpenAssetStore(
            blocked.string(),
            {.Workers = &compute, .Connections = std::make_shared<JobAdmission>(2)}),
        *session,
        transport,
        compute,
        {});
    CHECK(prepared, "runtime configuration defers the actual filesystem failure");
    if (!prepared) { return std::unexpected(prepared.error()); }
    const std::array<std::string, 1> layers{"buildings"};
    OsmField field(2, layers, MvtSchema::OpenMapTiles, *prepared);
    OsmCache::UnusedReader reader;
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
      auto response = field.PollTile(reader, {.X = 1, .Y = 1});
      if (!response || *response) { return response; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < until);
    return std::unexpected("fixture did not receive its storage error");
  }();
  const std::vector<std::string> reuse(256, std::string(128, 'x'));
  CHECK(!result && result.error() == "native OSM tile index failed",
        "the source, native job, decoder facade and field may retire before their error is read");
  CHECK(calls == 0 && reuse.size() == 256,
        "a real storage failure cannot fall through to source acquisition");
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "the blocked fixture path is removed after all producer work retires");
  return Report();
}
