#include "Check.h"
#include "ContentStore.h"
#include "OfflineTransport.h"
#include "SourceSet.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Data;

class Cells final : public Source {
public:
  SourceDecl Decl{.Id = "external-elevation-cells",
                  .Revision = "revision-1",
                  .Endpoint = "https://native.example/cells",
                  .Kind = DataKind::Elevation,
                  .How = Scheme::GeodeticGrid,
                  .MaximumPayloadBytes = 64};

  const SourceDecl &Declaration() const noexcept override { return Decl; }

  Coverage Covers(const Fetch &request) const noexcept override {
    const auto cell = request.Where().GeoCell();
    return request.Kind() == Decl.Kind && cell && cell->Valid() ? Coverage::Inside
                                                                : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &at, Transport &transport) const override {
    return transport.Begin(Decl.Endpoint + "/" + at.Text());
  }

  Fetched Collect(const Address &, Ticket ticket, Transport &transport) const override {
    auto wire = transport.Collect(ticket);
    auto answer = wire.Take();
    return answer ? Fetched::Delivered(std::move(answer->Body))
                  : Fetched::Meant(Meaning::Refused, wire.FailureReason());
  }
};

class Network final : public Transport {
public:
  int Starts = 0;

  FetchStart Begin(const std::string &) override { return static_cast<Ticket>(++Starts); }

  Wire Collect(Ticket) override { return Wire::Answered(200, {1, 2, 3, 4}); }

  void Cancel(Ticket) override {}
};
}

int main() {
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-shared-cells-XXXXXX").string();
  CHECK(mkdtemp(path.data()) != nullptr, "isolated source byte cache created");
  if (!std::filesystem::is_directory(path)) { return Report(); }
  const GeoCellId root{.Level = 1, .X = 1, .Y = 0};
  const std::array children{GeoCellId{.Level = 2, .X = 2, .Y = 0},
                            GeoCellId{.Level = 2, .X = 2, .Y = 1},
                            GeoCellId{.Level = 2, .X = 3, .Y = 0},
                            GeoCellId{.Level = 2, .X = 3, .Y = 1}};
  Network online;
  OfflineTransport offline;
  const Cells provider;
  for (int pass = 0; pass < 2; ++pass) {
    ContentStore store({.Directory = path});
    SourceSet sources(store);
    CHECK(sources.Add(std::make_unique<Cells>()) == SourceSet::Registration::Accepted,
          "external geodetic source uses the same public source contract");
    Transport &wire = pass == 0 ? static_cast<Transport &>(online) : offline;
    for (size_t at = 0; at < children.size(); ++at) {
      auto query = sources.Ask(Fetch(DataKind::Elevation, Address::AtGeoCell(children[at])));
      const auto answer = sources.Collect(query, wire).Take();
      CHECK(answer && answer->Bytes == std::vector<uint8_t>({1, 2, 3, 4}) &&
                answer->At == Address::AtGeoCell(children[at]),
            "cold and fresh offline delivery retain original bytes and cell identity");
      CHECK(store.HasCompleteChildCoverage(provider.Decl, root) ==
                (pass == 1 || at + 1 == children.size()),
            "all providers require four verified child payloads for parent coverage");
    }
    CHECK(online.Starts == 4, "reopened source cells need no remote request");
  }
  const auto key = ContentKey(provider.Decl, Address::AtGeoCell(children[0]));
  const std::array<char, 4> changed{1, 2, 3, 5};
  std::ofstream(path + "/" + key, std::ios::binary).write(changed.data(), changed.size());
  ContentStore fresh({.Directory = path});
  SourceSet sources(fresh);
  CHECK(sources.Add(std::make_unique<Cells>()) == SourceSet::Registration::Accepted,
        "fresh provider registered for corrupted-source replay");
  auto query = sources.Ask(Fetch(DataKind::Elevation, Address::AtGeoCell(children[0])));
  const auto failed = sources.Collect(query, offline);
  CHECK(failed.Failure() && failed.Failure()->Reason == FetchFailureReason::OfflineMiss &&
            !fresh.HasCompleteChildCoverage(provider.Decl, root),
        "same-size corrupted bytes are neither delivered nor counted as world coverage");
  std::error_code error;
  std::filesystem::remove_all(path, error);
  CHECK(!error, "isolated source byte cache removed");
  return Report();
}
