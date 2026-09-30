#include "Check.h"
#include "ContentStore.h"
#include "CopernicusDem.h"
#include "OfflineTransport.h"
#include "SourceSet.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;

class Http final : public Transport {
public:
  int Begins = 0, WholeBegins = 0, Status = kHttpPartialContent;
  bool Waiting = false, WithoutReceipt = false;
  std::string Url, Pin;
  ByteRange Asked;
  std::vector<uint8_t> Body{4, 5, 6};
  RangeResponse Origin{
      .Bytes = {.First = 100, .Length = 3}, .TotalBytes = 1000, .EntityTag = "\"rev-1\""};

  FetchStart Begin(const std::string &) override {
    ++WholeBegins;
    return Ticket{1};
  }

  FetchStart Begin(const std::string &url, ByteRange range, std::string_view pin) override {
    ++Begins;
    Url = url;
    Asked = range;
    Pin = pin;
    return Ticket{1};
  }

  Wire Collect(Ticket) override {
    if (Waiting) { return Wire::Working(); }
    return Status == kHttpPartialContent && !WithoutReceipt ? Wire::Answered(Body, Origin)
                                                            : Wire::Answered(Status, Body);
  }

  void Cancel(Ticket) override {}
};

void Add(SourceSet &sources) {
  using namespace outshine::Test;
  CHECK(sources.Add(std::make_unique<CopernicusDem>()) == SourceSet::Registration::Accepted,
        "Copernicus uses the same owned source registry as external providers");
}
}

int main() {
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-cog-source-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-source cache");
  const ContentStore::Config config{.Directory = directory};
  const auto at = Address::AtCell({54, 9});
  const Fetch request(DataKind::Elevation, at, ByteRange{.First = 100, .Length = 3}, "\"rev-1\"");
  const std::string url =
      "https://copernicus-dem-30m.s3.amazonaws.com/"
      "Copernicus_DSM_COG_10_N54_00_E009_00_DEM/Copernicus_DSM_COG_10_N54_00_E009_00_DEM.tif";
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    Http http;
    http.Waiting = true;
    auto query = sources.Ask(request);
    CHECK(sources.Collect(query, http).Where() == Delivery::State::Pending && http.Begins == 1,
          "partial source IO starts once and parks while the original response is unavailable");
    auto moved = std::move(query);
    http.Waiting = false;
    const auto answer = sources.Collect(moved, http).Take();
    CHECK(answer && answer->At == at && answer->Range && answer->Range->Bytes == *request.Range() &&
              answer->Range->EntityTag == request.EntityTag() &&
              answer->Range->TotalBytes == 1000 && answer->Bytes == http.Body &&
              answer->SourceId == "copernicus-glo30" && http.Url == url &&
              http.Asked == *request.Range() && http.Pin == request.EntityTag() &&
              http.WholeBegins == 0,
          "query movement preserves interval, pin, original-file identity and the received "
          "allocation");
    CHECK(sources.Counters().DeliveredBytes == 3 && store.Counters().Writes == 1,
          "delivery counts original bytes and persists the body with its receipt once");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    OfflineTransport offline;
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, offline).Take();
    CHECK(answer && answer->Range && answer->At == at && answer->Range->Bytes == *request.Range() &&
              answer->Range->EntityTag == request.EntityTag() &&
              answer->Range->TotalBytes == 1000 &&
              answer->Bytes == std::vector<uint8_t>({4, 5, 6}) &&
              sources.Counters().FromStore == 1 && sources.Counters().ProviderStarts == 0,
          "warm original ranges remain decodable offline without losing revision or file bounds");
    auto missing = sources.Ask(Fetch(DataKind::Elevation, at, *request.Range(), "\"rev-2\""));
    const auto refused = sources.Collect(missing, offline);
    CHECK(refused.Failure() && refused.Failure()->Reason == FetchFailureReason::OfflineMiss,
          "a different revision cannot silently consume previously cached bytes");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    const auto key =
        ContentKey(sources.At(0).Declaration(), at, request.Range(), request.EntityTag());
    auto damaged = store.Read(key);
    CHECK(damaged.has_value(), "received raw record exists");
    if (!damaged) { return Report(); }
    damaged->back() ^= 1;
    CHECK(store.Keep(key, damaged->data(), damaged->size()), "cache damage injected independently");
    Http http;
    auto query = sources.Ask(request);
    const auto recovered = sources.Collect(query, http).Take();
    CHECK(recovered && recovered->Range && http.Begins == 1 && sources.Counters().FromStore == 0,
          "damaged raw cache cannot impersonate a receipt; valid network delivery replaces it");
  }
  for (int bad = 0; bad < 6; ++bad) {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    Http http;
    if (bad == 0) { ++http.Origin.Bytes.First; }
    if (bad == 1) { http.Origin.EntityTag = "\"rev-2\""; }
    if (bad == 2) { http.Status = 200; }
    if (bad == 3) { http.WithoutReceipt = true; }
    if (bad == 4) { http.Body.pop_back(); }
    if (bad == 5) { http.Status = 412; }
    const auto where = Address::AtCell({54, 10 + bad});
    auto query = sources.Ask(Fetch(DataKind::Elevation, where, *request.Range(), "\"rev-1\""));
    const auto refused = sources.Collect(query, http);
    CHECK(
        refused.Failure() && refused.Failure()->SourceId == "copernicus-glo30" &&
            refused.Failure()->Served == where &&
            refused.Failure()->Reason ==
                (bad == 5 ? FetchFailureReason::SourceChanged : FetchFailureReason::CorruptPayload),
        "whole responses, mismatched intervals, mixed revisions and missing receipts stay refused");
    CHECK(store.Counters().Writes == 0 && sources.Counters().Delivered == 0,
          "refused original ranges cannot become durable cache facts or deliveries");
  }
  CopernicusDem source;
  const std::array invalid{
      Fetch(DataKind::Elevation, at, ByteRange{.First = 0, .Length = 0}),
      Fetch(DataKind::Elevation,
            at,
            ByteRange{.First = std::numeric_limits<uint64_t>::max(), .Length = 2}),
      Fetch(DataKind::Elevation,
            at,
            ByteRange{.First = 0, .Length = source.Declaration().MaximumPayloadBytes + 1}),
      Fetch(DataKind::Elevation, at, *request.Range(), "W/\"rev-1\"")};
  for (size_t i = 0; i < invalid.size(); ++i) {
    ContentStore store(config);
    SourceSet sources(store);
    Add(sources);
    Http http;
    auto query = sources.Ask(invalid[i]);
    const auto refused = sources.Collect(query, http);
    CHECK(refused.Failure() && http.Begins == 0 && http.WholeBegins == 0 &&
              store.Counters().Hits == 0 && store.Counters().Misses == 0 &&
              refused.Failure()->Reason == (i == 2 ? FetchFailureReason::CapacityRefused
                                                   : FetchFailureReason::InvalidRequest),
          "invalid intervals, over-budget demand and weak pins stop before cache or network work");
  }
  CHECK(source.Declaration().Wire == WireFormat::CopernicusCog &&
            source.Covers(Fetch(DataKind::Elevation, at)) == Coverage::Outside &&
            source.Covers(Fetch(DataKind::Elevation, Address::At({}), *request.Range())) ==
                Coverage::Outside &&
            source.Url(Address::AtCell({-34, -18})).find("S34_00_W018_00") != std::string::npos &&
            source.Url(Address::AtCell({90, 0})).empty(),
        "native product addressing preserves hemispheres and excludes Mercator or whole-file "
        "fallback");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}
