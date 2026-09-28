#include "Check.h"
#include "ContentStore.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-absence-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache created");
  const auto markers = std::filesystem::path(directory) / ".outshine-absence-v1";
  int64_t now = 1000;
  const ContentStore::Config config{.Directory = directory,
                                    .CapBytes = 16,
                                    .AbsenceEntries = 2,
                                    .UtcSeconds = [&now] { return now; }};
  const std::string first(64, 'a'), second(64, 'b'), third(64, 'c');
  {
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown, "unknown is not absent");
    CHECK(store.KeepAbsent(first, 10), "bounded authoritative evidence published");
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Absent && !store.Read(first),
          "typed absence never invents a byte payload");
  }
  {
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Absent,
          "confirmed absence survives a new store instance");
    now = 1010;
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown &&
              !std::filesystem::exists(markers / first),
          "evidence expires at its exact deadline");
    CHECK(store.KeepAbsent(first, 10) && store.KeepAbsent(second, 20) &&
              store.KeepAbsent(third, 30),
          "negative-cache pressure is admitted within its bound");
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown &&
              store.Lookup(second).Where == ContentStore::Presence::Absent &&
              store.Lookup(third).Where == ContentStore::Presence::Absent,
          "earliest expiry evicted instead of growing metadata indefinitely");
    size_t count = 0;
    for (const auto &entry : std::filesystem::directory_iterator(markers)) {
      count += entry.is_regular_file() ? 1u : 0u;
    }
    CHECK(count == 2, "disk evidence count obeys the same bound");
    const std::array<uint8_t, 2> bytes{17, 42};
    CHECK(store.Keep(second, bytes.data(), bytes.size()), "later real bytes published");
    CHECK(store.Lookup(second).Where == ContentStore::Presence::Bytes &&
              !std::filesystem::exists(markers / second),
          "bytes invalidate negative evidence");
    CHECK(!store.KeepAbsent("../escape", 10) && !store.KeepAbsent(first, 0) &&
              !store.KeepAbsent(first, ContentStore::PinnedAbsenceLifetimeS + 1),
          "unsafe keys and unbounded lifetime refused");
    now = std::numeric_limits<int64_t>::max();
    CHECK(!store.KeepAbsent(first, 10), "expiry cannot overflow");
  }
  now = 1010;
  std::ofstream(markers / first) << "outshine-absence-v1\n";
  {
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown,
          "truncated evidence is unknown and cannot trigger undefined parsing");
  }
  for (const auto &record : {"outshine-absence-v2\n1020\n",
                             "outshine-absence-v1\n1020junk\n",
                             "outshine-absence-v1\n9223372036854775807\n"}) {
    std::ofstream(markers / first) << record;
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown,
          "wrong version, corrupt number and implausible future evidence are unknown");
  }
  std::array<int, 3> order{0, 1, 2};
  do {
    std::filesystem::remove_all(markers);
    std::filesystem::create_directory(markers);
    const std::array<std::string, 3> keys{first, second, third};
    for (const int index : order) {
      std::ofstream(markers / keys[static_cast<size_t>(index)]) << "outshine-absence-v1\n"
                                                                << now + 10 * (index + 1) << "\n";
    }
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown &&
              store.Lookup(third).Where == ContentStore::Presence::Absent,
          "reload under pressure keeps the longest-lived evidence independent of directory order");
  } while (std::next_permutation(order.begin(), order.end()));
  std::filesystem::remove_all(markers);
  std::filesystem::create_directory(markers);
  std::ofstream(std::filesystem::path(directory) / "foreign") << "keep";
  std::filesystem::create_symlink(std::filesystem::path(directory) / "foreign", markers / first);
  {
    ContentStore store(config);
    CHECK(!store.KeepAbsent(first, 10) && std::filesystem::is_symlink(markers / first),
          "symbolic evidence is never read or overwritten");
  }
  std::filesystem::remove_all(markers);
  std::filesystem::create_directory(std::filesystem::path(directory) / "foreign-directory");
  std::filesystem::create_directory_symlink(std::filesystem::path(directory) / "foreign-directory",
                                            markers);
  {
    ContentStore store(config);
    CHECK(!store.KeepAbsent(first, 10) &&
              std::filesystem::is_empty(std::filesystem::path(directory) / "foreign-directory"),
          "marker directory cannot redirect writes to a foreign directory");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}
