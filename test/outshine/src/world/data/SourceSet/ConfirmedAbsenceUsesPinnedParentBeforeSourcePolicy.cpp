#include "SourceSet.h"
#include "WebTileSource.h"
#include "OfflineTransport.h"
#include "Check.h"
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;

class Tiles final : public WebTileSource {
public:
  Tiles()
      : WebTileSource({.Id = "hierarchical",
                       .Revision = "r1",
                       .Endpoint = "https://tiles.example/{z}/{x}/{y}",
                       .Kind = DataKind::Elevation,
                       .OnAbsent = AbsencePolicy::Fail,
                       .MinZoom = 1,
                       .MaxZoom = 3,
                       .AncestorFill = true,
                       .TileAbsence = TileAbsencePolicy::Parent},
                      "https://tiles.example/{z}/{x}/{y}") {}
};

class Http final : public Transport {
public:
  std::vector<std::string> Requested;
  int Status = 404;

  FetchStart Begin(const std::string &url) override {
    Requested.push_back(url);
    return static_cast<Ticket>(Requested.size());
  }

  Wire Collect(Ticket ticket) override {
    const auto &url = Requested[static_cast<size_t>(ticket) - 1];
    return url.ends_with("/1/1/1") ? Wire::Answered(200, {11, 22}) : Wire::Answered(Status, {});
  }

  void Cancel(Ticket) override {}

  double NowMs() override { return 0; }
};
}

int main() {
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-parent-XXXXXX").string();
  CHECK(mkdtemp(path.data()) != nullptr, "isolated source-byte cache created");
  const Fetch request(DataKind::Elevation, Address::At({.Zoom = 3, .X = 6, .Y = 4}));
  {
    ContentStore store({.Directory = path});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Tiles>()) == SourceSet::Registration::Accepted,
          "one provider registered");
    Http wire;
    auto query = sources.Ask(request);
    auto answer = sources.Collect(query, wire).Take();
    CHECK(answer && answer->At == Address::At({.Zoom = 1, .X = 1, .Y = 1}) &&
              answer->SourceId == "hierarchical" && answer->SourceRevision == "r1" &&
              answer->Bytes == std::vector<uint8_t>({11, 22}),
          "actual parent bytes retain provider, revision and served coordinates");
    CHECK(wire.Requested == std::vector<std::string>({"https://tiles.example/3/6/4",
                                                      "https://tiles.example/2/3/2",
                                                      "https://tiles.example/1/1/1"}),
          "confirmed misses traverse exactly one parent per request before Fail");
  }
  {
    ContentStore store({.Directory = path});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Tiles>()) == SourceSet::Registration::Accepted,
          "fresh process source state configured");
    OfflineTransport offline;
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, offline).Take();
    CHECK(answer && answer->At == Address::At({.Zoom = 1, .X = 1, .Y = 1}) &&
              answer->Bytes == std::vector<uint8_t>({11, 22}),
          "cached absence chain reaches parent bytes without network or changed pins");
  }
  {
    ContentStore store({.Using = ContentStore::Use::Off});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Tiles>()) == SourceSet::Registration::Accepted,
          "failure source configured");
    Http wire;
    wire.Status = 403;
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, wire);
    CHECK(answer.Where() == Delivery::State::Refused && wire.Requested.size() == 1,
          "authorization failures do not masquerade as absent detail");
  }
  std::error_code error;
  std::filesystem::remove_all(path, error);
  CHECK(!error, "owned test cache removed");
  return Report();
}
