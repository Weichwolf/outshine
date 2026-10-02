#include "Check.h"
#include "ContentStore.h"
#include "OfflineTransport.h"
#include "OsmApiReader.h"
#include "OsmApiSource.h"
#include "SourceProviderValidation.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine::Data;
const std::string kXml = "<osm version='0.6'><node id='1' lat='48' lon='16'/></osm>";

class Network final : public Transport {
public:
  size_t Starts = 0;

  FetchStart Begin(const std::string &) override { return static_cast<Ticket>(++Starts); }

  Wire Collect(Ticket) override {
    return Wire::Answered(200, std::vector<uint8_t>(kXml.begin(), kXml.end()));
  }

  void Cancel(Ticket) override {}
};

std::array<GeoCellId, 4> Children(GeoCellId cell) {
  return {{{.Level = cell.Level + 1, .X = cell.X * 2, .Y = cell.Y * 2},
           {.Level = cell.Level + 1, .X = cell.X * 2, .Y = cell.Y * 2 + 1},
           {.Level = cell.Level + 1, .X = cell.X * 2 + 1, .Y = cell.Y * 2},
           {.Level = cell.Level + 1, .X = cell.X * 2 + 1, .Y = cell.Y * 2 + 1}}};
}
}

int main() {
  using namespace outshine::Test;
  using namespace outshine::Generators::Osm;
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-partial-original-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated original source cache created");
  if (!std::filesystem::is_directory(directory)) { return Report(); }
  const SourceProvider provider{.Kind = "osm",
                                .Revision = "partial-source",
                                .Missing = MissingDataPolicy::Fail,
                                .Dataset = "openstreetmap.original",
                                .Endpoint = std::string(kOfficialOsmApi)};
  auto source = ApiSource::Create(provider, 0);
  CHECK(source.has_value(), "official original source configured");
  if (!source) { return Report(); }
  const auto &decl = (*source)->Declaration();
  const GeoCellId root{.Level = 9, .X = 274, .Y = 387};
  const auto children = Children(root);
  const auto leaves = Children(children.front());
  {
    ContentStore partial({.Directory = directory});
    CHECK(partial.KeepCell(decl,
                           leaves.front(),
                           std::span(reinterpret_cast<const uint8_t *>(kXml.data()), kXml.size())),
          "an interrupted preparation retains one deep received leaf");
    CHECK(!partial.HasCompleteChildCoverage(decl, root),
          "one received leaf cannot certify the remaining world");
  }
  Network online;
  OfflineTransport offline;
  for (int pass = 0; pass < 2; ++pass) {
    ContentStore fresh({.Directory = directory});
    Transport &wire = pass == 0 ? static_cast<Transport &>(online) : offline;
    std::vector<GeoCellId> pending{root};
    size_t delivered = 0, refined = 0, cached = 0;
    for (size_t at = 0; at < pending.size() && at < 16; ++at) {
      const std::array wanted{pending[at]};
      const auto startsBefore = online.Starts;
      auto read = ReadCells(provider, wanted, fresh, wire, wire.NowMs() + 1000, {});
      CHECK(read.has_value(), "partition acquisition succeeds online and freshly offline");
      if (!read) { break; }
      for (const auto &chunk : read->Chunks) {
        CHECK(chunk.Xml == kXml && chunk.Cell == wanted.front(),
              "received original bytes retain their requested cell identity");
        ++delivered;
        cached += static_cast<size_t>(chunk.FromStore);
      }
      for (const auto cell : read->Refine) {
        CHECK(online.Starts == startsBefore, "cached descendants skip the parent network probe");
        CHECK(cell == root || cell == children.front(),
              "only ancestors of existing received leaves are subdivided");
        const auto split = Children(cell);
        pending.insert(pending.end(), split.begin(), split.end());
        ++refined;
      }
    }
    CHECK(pending.size() == 9 && delivered == 7 && refined == 2 &&
              cached == (pass == 0 ? 1u : 7u) && online.Starts == 6,
          "all quadrants complete without refetching cached leaves or probing missing parents");
    CHECK(fresh.HasCompleteChildCoverage(decl, root),
          "only the completed seven-leaf partition certifies full source coverage");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated original source cache removed");
  return Report();
}
