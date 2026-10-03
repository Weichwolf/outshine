#include "Check.h"
#include "ContentStore.h"

#include <array>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

int main() {
  using namespace outshine::Data;
  using namespace outshine::Test;
  auto directory = (std::filesystem::temp_directory_path() / "outshine-absence-XXXXXX").string();
  CHECK(mkdtemp(directory.data()) != nullptr, "isolated cache created");
  const auto markers = std::filesystem::path(directory) / ".outshine-absence-v1";
  const ContentStore::Config config{.Directory = directory};
  const std::string first(64, 'a'), second(64, 'b'), third(64, 'c');
  {
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown, "unknown is not absent");
    CHECK(store.KeepAbsent(first), "authoritative evidence published");
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Absent && !store.Read(first),
          "typed absence never invents a byte payload");
    bool retained = true;
    for (unsigned index = 0; index < 5000; ++index) {
      retained &= store.KeepAbsent(std::format("{:064x}", index));
    }
    CHECK(retained && store.Lookup(first).Where == ContentStore::Presence::Absent,
          "more than 4096 other responses cannot evict confirmed source data");
    CHECK(!store.KeepAbsent("../escape"), "unsafe keys are refused");
  }
  {
    ContentStore store(config);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Absent &&
              store.Lookup(std::format("{:064x}", 0)).Where == ContentStore::Presence::Absent &&
              store.Lookup(std::format("{:064x}", 4999)).Where == ContentStore::Presence::Absent,
          "source evidence survives a fresh cache instance without an index ceiling");
    std::ofstream(markers / second) << "outshine-absence-v1\n1000\n";
    CHECK(store.Lookup(second).Where == ContentStore::Presence::Absent,
          "previously confirmed legacy responses remain usable after their old deadline");
    CHECK(store.KeepAbsent(third), "new permanent response written");
    const std::array<uint8_t, 2> bytes{17, 42};
    CHECK(store.Keep(third, bytes.data(), bytes.size()), "real bytes published explicitly");
    CHECK(store.Lookup(third).Where == ContentStore::Presence::Bytes &&
              !std::filesystem::exists(markers / third),
          "explicit bytes replace negative evidence");
    std::filesystem::remove(markers / first);
    CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown,
          "explicit deletion clears the cached response immediately");
    for (const auto &record : {"outshine-absence-v3\n",
                               "outshine-absence-v1\n",
                               "outshine-absence-v1\n1000junk\n",
                               "outshine-absence-v1\n9223372036854775807\n"}) {
      std::ofstream(markers / first) << record;
      CHECK(store.Lookup(first).Where == ContentStore::Presence::Unknown &&
                std::filesystem::exists(markers / first),
            "malformed evidence stays unknown and is not silently deleted");
    }
  }
  std::filesystem::remove_all(markers);
  std::filesystem::create_directory(markers);
  std::ofstream(std::filesystem::path(directory) / "foreign") << "keep";
  std::filesystem::create_symlink(std::filesystem::path(directory) / "foreign", markers / first);
  {
    ContentStore store(config);
    CHECK(!store.KeepAbsent(first) &&
              store.Lookup(first).Where == ContentStore::Presence::Unknown &&
              std::filesystem::is_symlink(markers / first),
          "symbolic evidence is never read or overwritten");
  }
  std::filesystem::remove_all(markers);
  std::filesystem::create_directory(std::filesystem::path(directory) / "foreign-directory");
  std::filesystem::create_directory_symlink(std::filesystem::path(directory) / "foreign-directory",
                                            markers);
  {
    ContentStore store(config);
    CHECK(!store.KeepAbsent(first) &&
              store.Lookup(first).Where == ContentStore::Presence::Unknown &&
              std::filesystem::is_empty(std::filesystem::path(directory) / "foreign-directory"),
          "marker directory cannot redirect reads or writes to a foreign directory");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "fixture removed");
  return Report();
}
