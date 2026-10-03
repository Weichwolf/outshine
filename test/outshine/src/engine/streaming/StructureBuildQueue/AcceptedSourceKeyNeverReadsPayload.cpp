#include "BuildingField.h"
#include "Check.h"
#include "StructureBuildQueue.h"
#include <array>
#include <cstdint>
#include <string>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <csignal>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void RejectsPayloadRead(int) {
  _exit(23);
}

void StopsOverdueRead(int) {
  _exit(25);
}

void ChecksPayloadIsolation(const ::outshine::Generators::Osm::BuildingField &field,
                            uint64_t expected) {
  using namespace outshine;
  using namespace outshine::Test;
  const long pageSize = sysconf(_SC_PAGESIZE);
  CHECK(pageSize > 0, "OS page size is available for the source-payload guard");
  if (pageSize <= 0) { return; }
  const auto &text = field.InputOfTile(7)->Sources.front().SourceId;
  const auto first = reinterpret_cast<uintptr_t>(text.data());
  const auto page = static_cast<uintptr_t>(pageSize);
  const auto interior = (first + page - 1) / page * page;
  CHECK(interior + page <= first + text.size(), "guard page lies inside source string storage");
  if (interior + page > first + text.size()) { return; }
  const pid_t child = fork();
  CHECK(child >= 0, "isolated getter process starts");
  if (child == 0) {
    std::signal(SIGSEGV, RejectsPayloadRead);
    std::signal(SIGBUS, RejectsPayloadRead);
    std::signal(SIGALRM, StopsOverdueRead);
    alarm(2);
    if (mprotect(reinterpret_cast<void *>(interior), page, PROT_NONE) != 0) { _exit(22); }
    const auto key = StructureBuildQueue::QualifiedSourceKey(field, 7);
    _exit(key && *key == expected ? 0 : 24);
  }
  if (child < 0) { return; }
  int status = 0;
  pid_t waited;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) != 25,
        "source-payload guard watchdog did not expire");
  CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "real runtime getter returns the cached key without reading protected source bytes");
}

}
#endif

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ::outshine::Generators::Osm::OsmField vectors(14, {});
  ::outshine::Generators::Osm::BuildingField field;
  field.AnchorAt({{1, 2, 3}});
  const Data::TileSourceIdentity source{.Kind = Data::DataKind::Elevation,
                                        .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                        .SourceId = std::string(65536, 'd'),
                                        .Revision = "revision-a"};
  auto sources = std::array{source, source};
  Data::TileSourceIdentity vector{.Kind = Data::DataKind::VectorMap,
                                  .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                  .SourceId = "osm",
                                  .Revision = "vector-a"};
  const ::outshine::Generators::Osm::BuildingField::Baked empty;
  field.PreparesAcceptances({.Tiles = 1});
  field.Take(7);
  auto pending = field.PrepareAcceptance(7,
                                         empty,
                                         sources,
                                         true,
                                         vector,
                                         {.HeightRasterDigest = 17,
                                          .StreetDigest = 19,
                                          .Projection = {.FocalPx = 0},
                                          .TileSpanM = 100,
                                          .Eye = {}});
  sources.front().SourceId.front() = 'x';
  vector.Revision = "producer-changed";
  field.CommitAcceptance(std::move(pending), vectors, empty);
  constexpr uint64_t expected = 0x1ae933c37f74e20a;
  CHECK(field.InputOfTile(7)->SourceKey == expected && field.InputOfTile(7)->Sources.size() == 1 &&
            StructureBuildQueue::QualifiedSourceKey(field, 7) == expected,
        "canonical accepted owner matches the independent little-endian FNV byte oracle");
#if defined(__unix__) || defined(__APPLE__)
  ChecksPayloadIsolation(field, expected);
#else
  Skip("source-payload page isolation requires POSIX");
#endif
  ::outshine::Generators::Osm::BuildingField snapshot = field.SnapshotAccepted();
  field.ResetDerived();
  CHECK(!StructureBuildQueue::QualifiedSourceKey(field, 7) &&
            StructureBuildQueue::QualifiedSourceKey(snapshot, 7) == expected,
        "reset revokes lookup while snapshots retain their owned cached key");
  const auto *accepted = snapshot.InputOfTile(7);
  auto bake = accepted->Bake;
  ++bake.HeightRasterDigest;
  auto changed =
      snapshot.PrepareAcceptance(7, empty, accepted->Sources, true, accepted->Vector, bake);
  snapshot.ReplaceAcceptance(std::move(changed), empty);
  const auto refreshed = StructureBuildQueue::QualifiedSourceKey(snapshot, 7);
  CHECK(refreshed && *refreshed != expected && *refreshed == snapshot.InputOfTile(7)->SourceKey,
        "source replacement recomputes the key at the accepted owner");
  auto unqualified = snapshot.PrepareAcceptance(7, empty);
  snapshot.ReplaceAcceptance(std::move(unqualified), empty);
  CHECK(!StructureBuildQueue::QualifiedSourceKey(snapshot, 7) &&
            snapshot.InputOfTile(7)->SourceKey == 0,
        "unqualified replacement cannot expose an old or invented key");
  return Report();
}
