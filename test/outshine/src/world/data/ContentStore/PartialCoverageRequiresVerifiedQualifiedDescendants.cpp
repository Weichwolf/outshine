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
  auto path = (std::filesystem::temp_directory_path() / "outshine-partial-cells-XXXXXX").string();
  CHECK(mkdtemp(path.data()) != nullptr, "isolated source cache created");
  if (!std::filesystem::is_directory(path)) { return Report(); }
  const SourceDecl decl{.Id = "external-geodetic",
                        .Revision = "v1",
                        .Endpoint = "https://example.test/cells",
                        .Kind = DataKind::Elevation,
                        .How = Scheme::GeodeticGrid,
                        .MaximumPayloadBytes = 64};
  const GeoCellId root{.Level = 1, .X = 1, .Y = 0};
  const GeoCellId leaf{.Level = 3, .X = 4, .Y = 0};
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  ContentStore store({.Directory = path});
  CHECK(!store.CanResumeFromChildren(decl, root), "empty cache cannot select a partition");
  CHECK(store.KeepCell(decl, leaf, bytes) && store.CanResumeFromChildren(decl, root) &&
            !store.HasCompleteChildCoverage(decl, root),
        "one verified deep leaf enables resumption without claiming complete coverage");
  CHECK(!store.CanResumeFromChildren(decl, root, 1),
        "a bounded probe does not accept an unverified descendant");
  auto changed = decl;
  changed.Revision = "v2";
  CHECK(!store.CanResumeFromChildren(changed, root), "revision qualifies source descendants");
  changed = decl;
  changed.Endpoint += "/different";
  CHECK(!store.CanResumeFromChildren(changed, root), "endpoint qualifies source descendants");
  {
    ContentStore fresh({.Directory = path});
    CHECK(fresh.CanResumeFromChildren(decl, root), "fresh process reuses verified source bytes");
  }
  const auto leafKey = ContentKey(decl, Address::AtGeoCell(leaf));
  std::ofstream(path + "/" + leafKey, std::ios::binary) << "fake";
  CHECK(!store.CanResumeFromChildren(decl, root),
        "same-size corrupt bytes cannot select a partition");
  CHECK(store.KeepCell(decl, leaf, bytes), "new received bytes replace the corrupt leaf");
  CHECK(store.KeepCell(decl, root, bytes) && !store.CanResumeFromChildren(decl, root),
        "valid cached parent bytes avoid unnecessary subdivision");
  const auto rootKey = ContentKey(decl, Address::AtGeoCell(root));
  std::ofstream(path + "/" + rootKey, std::ios::binary) << "fake";
  CHECK(store.CanResumeFromChildren(decl, root),
        "a corrupt parent does not hide verified descendants");
  std::filesystem::remove(path + "/" + leafKey);
  CHECK(!store.CanResumeFromChildren(decl, root),
        "receipt without payload cannot select a partition");
  CHECK(store.KeepCell(decl, leaf, bytes), "leaf restored for disabled-cache verification");
  ContentStore disabled({.Directory = path, .Using = ContentStore::Use::Off});
  CHECK(!disabled.CanResumeFromChildren(decl, root), "disabled cache supplies no source partition");
  std::error_code error;
  std::filesystem::remove_all(path, error);
  CHECK(!error, "isolated source cache removed");
  return Report();
}
