#include "Check.h"
#include "ContentStore.h"
#include "DeclaredSources.h"
#include "OfflineTransport.h"
#include "SourceSet.h"
#include "SourceProviderValidation.h"
#include "Transport.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

class TileTransport final : public outshine::Data::Transport {
public:
  std::vector<std::string> Urls;

  outshine::Data::Ticket Begin(const std::string &url) override {
    Urls.push_back(url);
    return static_cast<outshine::Data::Ticket>(Urls.size());
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket ticket) override {
    const size_t at = static_cast<size_t>(ticket) - 1;
    if (Urls[at].starts_with("https://first.example/")) {
      return outshine::Data::Wire::Answered(404, {});
    }
    return outshine::Data::Wire::Answered(200, {17, 42});
  }

  void Cancel(outshine::Data::Ticket) override {}
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Test;

  const std::array<SourceProvider, 2> providers = {{
      {.Kind = "terrain",
       .Revision = "same-pin",
       .Priority = -1,
       .Dataset = "first-dem",
       .Endpoint = "https://first.example/dem/{z}/{x}/{y}.png"},
      {.Kind = "terrain",
       .Revision = "same-pin",
       .Priority = 2,
       .Dataset = "second-dem",
       .Endpoint = "https://second.example/dem/{z}/{x}/{y}.png"},
  }};
  CHECK(ValidateSourceProviders(providers).has_value(), "distinct tile declarations validate");
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  std::string error;
  CHECK(RegisterDeclared(sources, providers, {}, error), error.c_str());
  CHECK(sources.Count() == 2, "both tile sources registered");
  if (sources.Count() != 2) { return Report(); }
  const Address tile = Address::At(TileId{.Zoom = 5, .X = 17, .Y = 11});
  const Fetch request(DataKind::Elevation, tile);
  CHECK(ContentKey(sources.At(0).Declaration(), tile) !=
            ContentKey(sources.At(1).Declaration(), tile),
        "same pin at different datasets/endpoints cannot alias in cache");
  SourceDecl changedEndpoint = sources.At(0).Declaration();
  changedEndpoint.Endpoint = "https://third.example/dem/{z}/{x}/{y}.png";
  CHECK(ContentKey(sources.At(0).Declaration(), tile) != ContentKey(changedEndpoint, tile),
        "endpoint change under the same dataset and pin invalidates cache identity");
  TileTransport transport;
  auto query = sources.Ask(request);
  Delivery delivered = sources.Collect(query, transport);
  auto answer = delivered.Take();
  CHECK(answer && answer->SourceId == "second-dem" && answer->SourceRevision == "same-pin" &&
            answer->Bytes == std::vector<uint8_t>({17, 42}),
        "absent preferred provider hands over to distinct second source");
  CHECK(transport.Urls == std::vector<std::string>({"https://first.example/dem/5/17/11.png",
                                                    "https://second.example/dem/5/17/11.png"}),
        "ranked providers expand their own tile endpoints in order");

  auto failing = providers;
  failing[0].Missing = MissingDataPolicy::Fail;
  SourceSet failSources(store);
  error.clear();
  CHECK(RegisterDeclared(failSources, failing, {}, error), error.c_str());
  TileTransport failTransport;
  auto failQuery = failSources.Ask(request);
  CHECK(failSources.Collect(failQuery, failTransport).Where() == Delivery::State::Refused &&
            failTransport.Urls.size() == 1,
        "fail policy stops before the second endpoint");

  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-tile-endpoint-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  ContentStore cache({.Directory = directory});
  SourceSet cached(cache);
  error.clear();
  CHECK(RegisterDeclared(cached, failing, {}, error), error.c_str());
  const std::array<uint8_t, 2> bytes{5, 9};
  CHECK(cache.Keep(ContentKey(cached.At(0).Declaration(), tile), bytes.data(), bytes.size()),
        "first provider's cached bytes are installed");
  OfflineTransport offline;
  auto cachedQuery = cached.Ask(request);
  auto cachedAnswer = cached.Collect(cachedQuery, offline).Take();
  CHECK(cachedAnswer && cachedAnswer->SourceId == "first-dem" &&
            cachedAnswer->Bytes == std::vector<uint8_t>({5, 9}) &&
            cached.Counters().RemoteStarts == 0,
        "offline delivery uses the selected source's bytes without network");
  SourceSet missing(cache);
  error.clear();
  CHECK(RegisterDeclared(missing, providers, {}, error), error.c_str());
  const Address uncached = Address::At(TileId{.Zoom = 5, .X = 18, .Y = 11});
  auto missQuery = missing.Ask(Fetch(DataKind::Elevation, uncached));
  const Delivery missed = missing.Collect(missQuery, offline);
  CHECK(missed.Where() == Delivery::State::Refused && missed.SourceId() == "first-dem" &&
            missed.SourceRevision() == "same-pin" && missing.Counters().RemoteStarts == 0,
        "offline miss reports selected source and pin without fetched data");
  std::error_code removeError;
  std::filesystem::remove_all(directory, removeError);
  CHECK(!removeError, "isolated source cache removed");

  for (const std::string invalid :
       {"http://a/{z}/{x}/{y}", "https://a/{z}/{x}/{x}", "https://a/{z}/{x}/{y}/{q}"}) {
    auto rejected = providers;
    rejected[0].Endpoint = invalid;
    CHECK(!ValidateSourceProviders(rejected).has_value(),
          "unsafe scheme or invalid placeholder is rejected");
  }
  return Report();
}
