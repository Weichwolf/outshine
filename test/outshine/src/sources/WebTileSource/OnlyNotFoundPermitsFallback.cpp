#include "Check.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "WebTileFixture.h"
#include <SourceTransport.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;
constexpr int kOk = 200;
constexpr int kUnauthorized = 401;
constexpr int kForbidden = 403;
constexpr int kNotFound = 404;
constexpr uint8_t kSecondByte = 22;

class HttpTransport final : public SourceTransport {
public:
  int Status = kForbidden;
  std::vector<std::string> Urls;

  FetchStart Begin(const std::string &url) override {
    Urls.push_back(url);
    return static_cast<Ticket>(Urls.size());
  }

  Wire Collect(Ticket ticket) override {
    const auto &url = Urls[static_cast<size_t>(ticket) - 1];
    if (url.starts_with("https://fallback.example/") || Status == kOk) {
      return Wire::Answered(kOk, {11, kSecondByte});
    }
    return Wire::Answered(Status, {});
  }

  void Cancel(Ticket /*ticket*/) override {}
};

void CheckResponse(int status) {
  using namespace outshine::Test;
  RawSourceCache store({.Directory = {}, .Using = RawSourceCache::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
            "pin",
            Rank{0},
            AbsencePolicy::Continue,
            "primary",
            "https://primary.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
        "primary source registered");
  CHECK(sources.Add(std::make_unique<outshine::Test::WebTileFixture>(
            "pin",
            Rank{1},
            AbsencePolicy::Continue,
            "fallback",
            "https://fallback.example/{z}/{x}/{y}.png")) == SourceSet::Registration::Accepted,
        "fallback source registered");
  HttpTransport transport;
  transport.Status = status;
  const SourceRequest request(DataKind::Elevation,
                              Address::At(TileId{.Zoom = 5, .X = 17, .Y = 11}));
  auto query = sources.Ask(request);
  auto reply = sources.Collect(query, transport);
  if (status == kNotFound) {
    const auto answer = reply.Take();
    CHECK(answer && answer->SourceId == "fallback" && transport.Urls.size() == 2,
          "authoritative not-found permits ranked fallback");
    CHECK(sources.Counters().HandedOver == 1, "absence counted once");
  } else {
    CHECK(reply.Where() == SourceDelivery::State::Refused && reply.SourceId() == "primary" &&
              transport.Urls.size() == 1,
          "access refusal does not masquerade as absent terrain");
    CHECK(sources.Counters().HandedOver == 0, "refusal never recorded as absence");
  }
  transport.Status = kOk;
  auto recovered = sources.Ask(request);
  const auto answer = sources.Collect(recovered, transport).Take();
  CHECK(answer && answer->SourceId == "primary" &&
            answer->Bytes == std::vector<uint8_t>({11, kSecondByte}),
        "a subsequent authorized request can deliver the original terrain");
}
}

int main() {
  CheckResponse(kNotFound);
  CheckResponse(kForbidden);
  CheckResponse(kUnauthorized);
  return outshine::Test::Report();
}
