#include "Check.h"
#include "ContentStore.h"
#include "SourceSet.h"
#include "TerrariumDem.h"
#include "Transport.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class HttpTransport final : public Transport {
public:
  int Status = 403;
  std::vector<std::string> Urls;

  FetchStart Begin(const std::string &url) override {
    Urls.push_back(url);
    return static_cast<Ticket>(Urls.size());
  }

  Wire Collect(Ticket ticket) override {
    const auto &url = Urls[static_cast<size_t>(ticket) - 1];
    if (url.starts_with("https://fallback.example/") || Status == 200) {
      return Wire::Answered(200, {11, 22});
    }
    return Wire::Answered(Status, {});
  }

  void Cancel(Ticket) override {}
};

void CheckResponse(int status) {
  using namespace outshine::Test;
  ContentStore store({.Using = ContentStore::Use::Off});
  SourceSet sources(store);
  CHECK(sources.Add(std::make_unique<TerrariumDem>("pin",
                                                   Rank{0},
                                                   AbsencePolicy::Continue,
                                                   "primary",
                                                   "https://primary.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "primary source registered");
  CHECK(sources.Add(std::make_unique<TerrariumDem>("pin",
                                                   Rank{1},
                                                   AbsencePolicy::Continue,
                                                   "fallback",
                                                   "https://fallback.example/{z}/{x}/{y}.png")) ==
            SourceSet::Registration::Accepted,
        "fallback source registered");
  HttpTransport transport;
  transport.Status = status;
  const Fetch request(DataKind::Elevation, Address::At(TileId{.Zoom = 5, .X = 17, .Y = 11}));
  auto query = sources.Ask(request);
  auto reply = sources.Collect(query, transport);
  if (status == 404) {
    const auto answer = reply.Take();
    CHECK(answer && answer->SourceId == "fallback" && transport.Urls.size() == 2,
          "authoritative not-found permits ranked fallback");
    CHECK(sources.Counters().HandedOver == 1, "absence counted once");
  } else {
    CHECK(reply.Where() == Delivery::State::Refused && reply.SourceId() == "primary" &&
              transport.Urls.size() == 1,
          "access refusal does not masquerade as absent terrain");
    CHECK(sources.Counters().HandedOver == 0, "refusal never recorded as absence");
  }
  transport.Status = 200;
  auto recovered = sources.Ask(request);
  const auto answer = sources.Collect(recovered, transport).Take();
  CHECK(answer && answer->SourceId == "primary" && answer->Bytes == std::vector<uint8_t>({11, 22}),
        "a subsequent authorized request can deliver the original terrain");
}
}

int main() {
  CheckResponse(404);
  CheckResponse(403);
  CheckResponse(401);
  return outshine::Test::Report();
}
