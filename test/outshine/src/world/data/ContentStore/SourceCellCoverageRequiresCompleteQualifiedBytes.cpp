#include "Check.h"
#include "ContentStore.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto path = (std::filesystem::temp_directory_path() / "outshine-cell-cache-XXXXXX").string();
  CHECK(mkdtemp(path.data()) != nullptr, "isolated source cell cache created");
  if (!std::filesystem::is_directory(path)) { return Report(); }
  const SourceDecl source{.Id = "official.original",
                          .Revision = "v1",
                          .Endpoint = "https://example.test/map",
                          .Kind = DataKind::OriginalOsm,
                          .How = Scheme::GeodeticGrid,
                          .Wire = WireFormat::OsmXml,
                          .MaximumPayloadBytes = 64};
  const GeoCellId root{.Level = 1, .X = 1, .Y = 0};
  const std::array children{GeoCellId{.Level = 2, .X = 2, .Y = 0},
                            GeoCellId{.Level = 2, .X = 2, .Y = 1},
                            GeoCellId{.Level = 2, .X = 3, .Y = 0},
                            GeoCellId{.Level = 2, .X = 3, .Y = 1}};
  const std::array<uint8_t, 4> payload{'<', 'x', '/', '>'};
  ContentStore store({.Directory = path});
  for (size_t at = 0; at < children.size(); ++at) {
    CHECK(store.KeepCell(source, children[at], payload),
          "original cell bytes stored with address receipt");
    CHECK(store.HasCompleteChildCoverage(source, root) == (at + 1 == children.size()),
          "coverage requires every child, including the final quadrant");
  }
  CHECK(!store.HasCompleteChildCoverage(source, root, 3),
        "bounded traversal fails instead of claiming partial coverage");
  auto other = source;
  other.Revision = "v2";
  CHECK(!store.HasCompleteChildCoverage(other, root),
        "another revision cannot reuse source coverage");
  other = source;
  other.Endpoint += "/other";
  CHECK(!store.HasCompleteChildCoverage(other, root),
        "another endpoint cannot reuse source coverage");
  {
    ContentStore fresh({.Directory = path});
    CHECK(fresh.HasCompleteChildCoverage(source, root),
          "coverage survives destruction of the receiving process state");
    CHECK(fresh.LookupCell(source, children[0]).Bytes ==
              std::vector<uint8_t>(payload.begin(), payload.end()),
          "source payload roundtrips in a fresh store");
  }
  const auto key = ContentKey(source, Address::AtGeoCell(children[0]));
  std::ofstream(path + "/" + key, std::ios::binary) << "<y/>";
  CHECK(!store.HasCompleteChildCoverage(source, root) &&
            store.LookupCell(source, children[0]).Where == ContentStore::Presence::Unknown,
        "same-size changed bytes fail the received payload digest");
  CHECK(store.KeepCell(source, children[0], payload),
        "a new received response replaces the corrupt entry");
  std::filesystem::remove(path + "/" + key);
  CHECK(!store.HasCompleteChildCoverage(source, root),
        "a receipt without source bytes is not coverage");
  CHECK(store.Keep(key, payload.data(), payload.size()), "legacy flat payload preserved");
  CHECK(store.KeepCell(source, root, payload) && !store.HasCompleteChildCoverage(source, root),
        "cached parent bytes are served directly rather than forcing refinement");
  ContentStore disabled({.Directory = path, .Using = ContentStore::Use::Off});
  CHECK(!disabled.HasCompleteChildCoverage(source, root) &&
            !disabled.KeepCell(source, root, payload) &&
            disabled.LookupCell(source, root).Where == ContentStore::Presence::Unknown,
        "disabled source cache does not read or write receipts");
  std::error_code error;
  std::filesystem::remove_all(path, error);
  CHECK(!error, "isolated source cell cache removed");
  return Report();
}
