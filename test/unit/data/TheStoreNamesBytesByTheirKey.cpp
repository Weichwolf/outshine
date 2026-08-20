#include "Check.h"
#include "ContentStore.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace outshine;
using namespace outshine::Data;

namespace {

[[nodiscard]] SourceDecl Declared(const char *id, uint32_t version) {
  SourceDecl d;
  d.Id = id;
  d.Version = version;
  d.Kind = DataKind::Elevation;
  return d;
}

[[nodiscard]] std::string Scratch(const char *leaf) {
  std::error_code ec;
  const std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / leaf;
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  return dir.string();
}

[[nodiscard]] size_t FilesIn(const std::string &dir) {
  std::error_code ec;
  size_t n = 0;
  for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) n++;
  return n;
}

}

int main() {
  Test::Covers("I.22 the provider delivers hash and data, and hash is the filename");
  Test::Covers("I.22 the provider's hash covers its own version");

  const Address at = Address::Tile(14, 8620, 5403);
  const std::string keyV1 = ContentKey(Declared("terrarium.s3", 1), at);
  const std::string keyV2 = ContentKey(Declared("terrarium.s3", 2), at);
  const std::string keyOther = ContentKey(Declared("other.dem", 1), at);
  const std::string keyElsewhere = ContentKey(Declared("terrarium.s3", 1), Address::Tile(14, 8621, 5403));

  CHECK(keyV1.size() == 64, "the key is a 64-character hex digest, which is 256 bits");
  CHECK(keyV1 != keyV2, "raising the declared version changes the name");
  CHECK(keyV1 != keyOther, "two sources at one address do not share a name");
  CHECK(keyV1 != keyElsewhere, "two addresses under one source do not share a name");
  CHECK(ContentKey(Declared("terrarium.s3", 1), at) == keyV1, "and the key is a function of nothing else");

  const std::vector<uint8_t> payload = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};

  {
    ContentStore::Config config;
    config.Directory = Scratch("outshine-store-test");
    ContentStore store(config);
    std::vector<uint8_t> back;
    CHECK(!store.TryRead(keyV1, &back), "an empty store holds nothing");
    store.Keep(keyV1, payload.data(), payload.size());
    CHECK(store.TryRead(keyV1, &back), "what was kept is found again");
    CHECK(back == payload, "and it is the same bytes");
    CHECK(!store.TryRead(keyV2, &back), "the same address at another version is a miss");
    CHECK(FilesIn(config.Directory) == 1,
          "one delivery is one file: no index, no sidecar, no temporary left behind");
    CHECK(std::filesystem::exists(config.Directory + "/" + keyV1),
          "and the file is named by the key itself");
    const ContentStore::Ledger ledger = store.Counters();
    CHECK(ledger.Writes == 1 && ledger.Hits == 1 && ledger.Misses == 2, "the ledger agrees");
  }

  {
    ContentStore::Config config;
    config.Directory = Scratch("outshine-store-off");
    config.Using = ContentStore::Use::Off;
    ContentStore store(config);
    std::vector<uint8_t> back;
    store.Keep(keyV1, payload.data(), payload.size());
    CHECK(!store.TryRead(keyV1, &back), "a store that is off never answers");
    CHECK(FilesIn(config.Directory) == 0, "and writes nothing");
  }

  {
    const std::string dir = Scratch("outshine-store-cap");
    const std::vector<uint8_t> kilobyte(1024, 0xAB);
    {
      ContentStore::Config config;
      config.Directory = dir;
      ContentStore store(config);
      for (int i = 0; i < 4; i++) {
        const std::string key = ContentKey(Declared("capped", (uint32_t)i), at);
        store.Keep(key, kilobyte.data(), kilobyte.size());
      }
      CHECK(FilesIn(dir) == 4, "four deliveries are four files before the sweep");
    }
    ContentStore::Config capped;
    capped.Directory = dir;
    capped.CapBytes = 2048;
    ContentStore store(capped);
    CHECK(FilesIn(dir) <= 2, "the sweep at open brings the directory back under its cap");
    CHECK(store.Counters().Swept >= 2, "and it says how many it took");
    Test::Note("swept at open", (double)store.Counters().Swept, "file");
  }

  return Test::Report();
}
