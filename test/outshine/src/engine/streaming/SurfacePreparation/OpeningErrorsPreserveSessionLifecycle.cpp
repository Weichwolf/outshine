#include "SurfacePreparation.h"
#include "SourceSession.h"
#include "OfflineSourceTransport.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <limits>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const ContentDirectories directories{.BuiltinData = "data", .Sources = {}, .Assets = {}};
  const Data::ProviderRegistry registry;
  Data::OfflineSourceTransport wire;
  Tasks compute(1);
  Ground::SurfacePreparation preparation;
  constexpr LongitudeLatitude focus{.LongitudeDeg = 9, .LatitudeDeg = 47};
  const auto absent = preparation.OpenSession(directories, {}, focus, wire, compute, nullptr);
  CHECK(!absent && absent.error() == "surface preparation requires configured sources" &&
            !preparation.Opened(),
        "a missing session returns an owned error without opening resources");
  auto session = Data::SourceSession::Open(
      {.Directory = {}, .Using = Data::RawSourceCache::Use::Off}, {}, "data", registry);
  CHECK(session, "an explicit empty offline source session opens");
  if (!session) { return Report(); }
  const auto opened = preparation.OpenSession(directories, *session, focus, wire, compute, nullptr);
  CHECK(opened && preparation.Opened(), "a valid session opens without a diagnostic adapter");
  if (!opened) { return Report(); }
  const auto *pool = &preparation.Pool();
  for (const LongitudeLatitude invalid :
       {LongitudeLatitude{.LongitudeDeg = std::numeric_limits<double>::quiet_NaN()},
        LongitudeLatitude{.LatitudeDeg = 91}}) {
    const auto rejected =
        preparation.OpenSession(directories, *session, invalid, wire, compute, nullptr);
    CHECK(!rejected && !rejected.error().empty(),
          "invalid coordinates return their preparation error");
    CHECK(preparation.Opened() && &preparation.Pool() == pool,
          "validation failure preserves the previously open session");
  }
  std::array<Data::SourceProvider, 1> unsupported{};
  unsupported.front().Kind = "unregistered";
  const auto rejected =
      preparation.Open(directories, unsupported, registry, focus, wire, compute, nullptr);
  CHECK(!rejected && !rejected.error().empty() && preparation.Opened() &&
            &preparation.Pool() == pool,
        "source construction errors reach the caller without replacing its valid session");
  const auto rejectedAgain =
      preparation.OpenSession(directories, {}, focus, wire, compute, nullptr);
  CHECK(!rejectedAgain && !absent && rejectedAgain.error() == absent.error() &&
            preparation.Opened() && &preparation.Pool() == pool,
        "each failed request owns its error and retains the current session");
  CHECK(preparation.Pool().Counters().Posts == 0 && preparation.Pool().Counters().Fetches == 0,
        "opening and validation perform no implicit acquisition");
  const auto blocked =
      std::filesystem::temp_directory_path() /
      ("outshine-blocked-asset-cache-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    const std::ofstream file(blocked);
    CHECK(file.is_open(), "an ordinary file blocks the private asset-cache directory");
    if (!file.is_open()) { return Report(); }
  }
  auto badCache = directories;
  badCache.Assets = blocked.string();
  const auto unavailable =
      preparation.OpenSession(badCache, *session, focus, wire, compute, nullptr);
  CHECK(!unavailable && !unavailable.error().empty() && !preparation.Opened(),
        "late cache binding failures return their owned error and release partial preparation");
  std::error_code cleanupError;
  std::filesystem::remove(blocked, cleanupError);
  CHECK(!cleanupError, "private failure input removed");
  const auto recovered =
      preparation.OpenSession(directories, *session, focus, wire, compute, nullptr);
  CHECK(recovered && preparation.Opened() && !unavailable && !unavailable.error().empty(),
        "a later valid request opens independently of the previous cache error");
  preparation.Close();
  CHECK(!preparation.Opened() && !absent && !rejectedAgain &&
            absent.error() == rejectedAgain.error(),
        "errors remain valid after preparation teardown");
  return Report();
}
