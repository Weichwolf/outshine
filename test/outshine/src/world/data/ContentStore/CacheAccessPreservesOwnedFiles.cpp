#include "ContentStore.h"
#include "CacheTrim.h"
#include "Check.h"
#include <array>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto pattern = (std::filesystem::temp_directory_path() / "outshine-cache-XXXXXX").string();
  const char *created = mkdtemp(pattern.data());
  CHECK(created != nullptr, "temporary root created");
  if (!created) { return Report(); }
  const std::filesystem::path root(created);
  const auto cache = root / "cache";
  std::filesystem::create_directory(cache);
  const std::string key(64, 'a');
  const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  {
    ContentStore store({.Directory = cache.string(), .CapBytes = 4});
    CHECK(store.Keep(key, bytes.data(), bytes.size()), "exact cap published");
    CHECK(store.Lookup(key).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
          "exact cap read");
    const auto cost = store.Counters();
    CHECK(cost.ReadCalls == 1 && cost.ReadBytes == bytes.size() && cost.WriteBytes == bytes.size(),
          "cache counts the bytes actually read and written");
    CHECK(cost.ReadMs >= 0.0 && cost.WriteMs >= 0.0, "cache IO durations are nonnegative");
    CHECK(store.Lookup(key, 3).Where == ContentStore::Presence::Unknown, "caller can tighten cap");
    for (const std::string &invalid : std::vector<std::string>{
             "../outside", "", "plain", std::string(64, 'A'), key + std::string(1, '\0')}) {
      CHECK(!store.Keep(invalid, bytes.data(), bytes.size()), "invalid key rejected");
      CHECK(store.Lookup(invalid).Where == ContentStore::Presence::Unknown,
            "invalid key cannot be read");
    }
    CHECK(!std::filesystem::exists(root / "outside"), "no file escaped cache");
    const std::array<uint8_t, 5> oversized{5, 6, 7, 8, 9};
    CHECK(!store.Keep(key, oversized.data(), oversized.size()), "oversized write rejected");
    CHECK(store.Lookup(key).Bytes == std::vector<uint8_t>(bytes.begin(), bytes.end()),
          "rejected replacement preserves bytes");
    std::ofstream(cache / key, std::ios::binary).write("12345", 5);
    CHECK(store.Lookup(key).Where == ContentStore::Presence::Unknown &&
              store.Lookup(key, 100).Where == ContentStore::Presence::Unknown,
          "caller cannot enlarge store cap");
  }
  std::filesystem::remove(cache / key);
  std::ofstream(cache / "unrelated") << "must remain";
  std::ofstream(cache / (key + ".outshine-0.tmp")) << "active writer";
  std::ofstream(root / "target") << "ok";
  std::filesystem::create_symlink(root / "target", cache / key);
  {
    ContentStore store({.Directory = cache.string(), .CapBytes = 4});
    CHECK(outshine::TrimSourceCache(cache, {.BudgetBytes = 4, .Apply = true}),
          "maintenance ignores foreign files and unsupported entry types");
    CHECK(std::filesystem::exists(cache / "unrelated"), "eviction preserves foreign files");
    CHECK(std::filesystem::exists(cache / (key + ".outshine-0.tmp")),
          "eviction preserves temporary files");
    CHECK(std::filesystem::is_symlink(cache / key), "eviction leaves symlinks alone");
    CHECK(store.Lookup(key).Where == ContentStore::Presence::Unknown,
          "symlink is not a cache entry");
  }
  std::filesystem::remove(cache / key);
  std::filesystem::create_directory(cache / key);
  {
    ContentStore store({.Directory = cache.string(), .CapBytes = 4});
    CHECK(!store.Keep(key, bytes.data(), bytes.size()), "directory replacement fails");
    CHECK(std::filesystem::is_directory(cache / key), "failed publication preserves destination");
  }
  std::filesystem::remove(cache / key);
  const std::string newerKey(64, 'b');
  std::ofstream(cache / key) << "1234";
  std::ofstream(cache / newerKey) << "5678";
  const auto now = std::filesystem::file_time_type::clock::now();
  std::filesystem::last_write_time(cache / key, now - std::chrono::seconds(1));
  std::filesystem::last_write_time(cache / newerKey, now);
  {
    ContentStore store({.Directory = cache.string(), .CapBytes = 4});
    CHECK(store.Lookup(key).Where == ContentStore::Presence::Bytes &&
              store.Lookup(newerKey).Where == ContentStore::Presence::Bytes,
          "opening preserves existing source bytes even above the maintenance budget");
  }
  const auto trimmed = outshine::TrimSourceCache(cache, {.BudgetBytes = 4, .Apply = true});
  CHECK(trimmed && trimmed->EntriesRemoved == 1 && trimmed->StoredBytes == 8 &&
            trimmed->RetainedBytes == 4,
        "explicit maintenance accounts only owned source bytes");
  CHECK(std::filesystem::exists(cache / key) != std::filesystem::exists(cache / newerKey),
        "one complete source remains after applying the budget");
  const auto repeated = outshine::TrimSourceCache(cache, {.BudgetBytes = 4, .Apply = true});
  CHECK(repeated && repeated->EntriesRemoved == 0 && repeated->RetainedBytes == 4,
        "repeated maintenance preserves entries already within budget");
  CHECK(!outshine::TrimSourceCache(root / "target", {.BudgetBytes = 4, .Apply = true}),
        "unavailable maintenance directory is an explicit failure");
  {
    ContentStore disabled(
        {.Directory = (root / "target").string(), .Using = ContentStore::Use::Off});
    CHECK(disabled.Lookup(key).Where == ContentStore::Presence::Unknown &&
              !disabled.Keep(key, bytes.data(), bytes.size()),
          "disabled stores perform no cache IO");
    CHECK(std::filesystem::file_size(root / "target") == 2,
          "disabled stores preserve foreign target");
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "temporary fixtures removed");
  return Report();
}
