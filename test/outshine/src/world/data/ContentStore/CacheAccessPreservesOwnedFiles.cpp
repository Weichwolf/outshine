#include "ContentStore.h"
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
    CHECK(store.Read(key) == std::optional(std::vector<uint8_t>(bytes.begin(), bytes.end())),
          "exact cap read");
    CHECK(!store.Read(key, 3), "caller can tighten cap");
    for (const std::string &invalid : std::vector<std::string>{
             "../outside", "", "plain", std::string(64, 'A'), key + std::string(1, '\0')}) {
      CHECK(!store.Keep(invalid, bytes.data(), bytes.size()), "invalid key rejected");
      CHECK(!store.Read(invalid), "invalid key cannot be read");
    }
    CHECK(!std::filesystem::exists(root / "outside"), "no file escaped cache");
    const std::array<uint8_t, 5> oversized{5, 6, 7, 8, 9};
    CHECK(!store.Keep(key, oversized.data(), oversized.size()), "oversized write rejected");
    CHECK(store.Read(key) == std::optional(std::vector<uint8_t>(bytes.begin(), bytes.end())),
          "rejected replacement preserves bytes");
    std::ofstream(cache / key, std::ios::binary).write("12345", 5);
    CHECK(!store.Read(key) && !store.Read(key, 100), "caller cannot enlarge store cap");
  }
  std::filesystem::remove(cache / key);
  std::ofstream(cache / "unrelated") << "must remain";
  std::ofstream(cache / (key + ".outshine-0.tmp")) << "active writer";
  std::ofstream(root / "target") << "ok";
  std::filesystem::create_symlink(root / "target", cache / key);
  {
    ContentStore store({.Directory = cache.string(), .CapBytes = 4});
    CHECK(std::filesystem::exists(cache / "unrelated"), "eviction preserves foreign files");
    CHECK(std::filesystem::exists(cache / (key + ".outshine-0.tmp")),
          "eviction preserves temporary files");
    CHECK(std::filesystem::is_symlink(cache / key), "eviction leaves symlinks alone");
    CHECK(!store.Read(key), "symlink is not a cache entry");
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
    CHECK(!store.Read(key) && store.Read(newerKey).has_value(), "oldest owned entry evicted first");
    CHECK(store.Counters().Swept == 1 && store.Counters().SweptBytes == 4,
          "eviction accounts only owned bytes");
  }
  std::error_code error;
  std::filesystem::remove_all(root, error);
  CHECK(!error, "temporary fixtures removed");
  return Report();
}
