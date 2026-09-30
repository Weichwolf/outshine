#include "Check.h"
#include "ContentStore.h"
#include "OfflineTransport.h"
#include "SourceSet.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;

class Native final : public Source {
public:
  Native() {
    Decl_.Id = "native-cell-contract";
    Decl_.Revision = "fixture-1";
    Decl_.How = Scheme::GeographicCell;
    Decl_.Latency = LatencyClass::Local;
  }

  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const Fetch &request) const noexcept override {
    const auto cell = request.Where().Cell();
    return request.Kind() == Decl_.Kind && cell && cell->SouthDeg >= -90 && cell->SouthDeg < 90 &&
                   cell->WestDeg >= -180 && cell->WestDeg < 180
               ? Coverage::Inside
               : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override {
    ++Starts;
    return Ticket::None;
  }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    return Fetched::Delivered({1, 2, 3});
  }

  mutable int Starts = 0;

private:
  SourceDecl Decl_;
};
}

int main() {
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-cells-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original-payload cache");
  const ContentStore::Config config{.Directory = directory};
  const std::array cells{CellId{54, 9}, CellId{-34, 18}, CellId{-90, -180}, CellId{89, 179}};
  const std::array<std::string, 4> names{"g/54/9", "g/-34/18", "g/-90/-180", "g/89/179"};
  std::vector<std::string> keys;
  for (int pass = 0; pass < 2; ++pass) {
    ContentStore store(config);
    SourceSet sources(store);
    auto native = std::make_unique<Native>();
    const Native *provider = native.get();
    CHECK(sources.Add(std::move(native)) == SourceSet::Registration::Accepted,
          "same public provider registry accepts native source cells");
    OfflineTransport offline;
    for (size_t i = 0; i < cells.size(); ++i) {
      const Address at = Address::AtCell(cells[i]);
      const Fetch request(DataKind::Elevation, at);
      CHECK(at.How() == Scheme::GeographicCell && at.Cell() == cells[i] && !at.Tile() &&
                !at.Index() && at.Text() == names[i],
            "signed geographic cells retain a distinct native address family");
      auto query = sources.Ask(request);
      const auto answer = sources.Collect(query, offline).Take();
      CHECK(answer && answer->At == at && answer->Bytes == std::vector<uint8_t>({1, 2, 3}) &&
                answer->SourceId == "native-cell-contract" && answer->SourceRevision == "fixture-1",
            "delivery and reopened raw cache preserve native cell and source identity");
      const auto key = ContentKey(provider->Declaration(), at);
      CHECK(key != ContentKey(provider->Declaration(), Address::Whole(0)) &&
                key != ContentKey(provider->Declaration(), Address::At({})),
            "native cells cannot alias indexed or Mercator payloads");
      if (pass == 0) { keys.push_back(key); }
      CHECK(keys[i] == key, "source-cell identity survives store reopening");
    }
    for (const CellId invalid : {CellId{-91, 0}, CellId{90, 0}, CellId{0, -181}, CellId{0, 180}}) {
      auto query = sources.Ask(Fetch(DataKind::Elevation, Address::AtCell(invalid)));
      CHECK(sources.Collect(query, offline).Where() == Delivery::State::Undeclared,
            "invalid source cells cannot reach IO or an unrelated fallback");
    }
    CHECK(provider->Starts == (pass == 0 ? 4 : 0), "warm original payloads require no provider IO");
    CHECK(sources.Counters().FromStore == (pass == 0 ? 0 : 4),
          "warm native delivery uses shared cache");
  }
  for (size_t i = 0; i < keys.size(); ++i) {
    for (size_t j = i + 1; j < keys.size(); ++j) {
      CHECK(keys[i] != keys[j], "separate native cells cannot share cached payloads");
    }
  }
  CHECK(Address::At({.Zoom = 17, .X = 69, .Y = 45}).Text() == "17/69/45" &&
            Address::Whole(123).Text() == "w/123",
        "existing source-cache identities remain compatible");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  return Report();
}
