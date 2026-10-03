#include "Check.h"
#include "Digest.h"
#include "GroundMaterials.h"
#include "StreetField.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <cerrno>
#include <csignal>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
void RejectsRead(int) {
  _exit(23);
}

void StopsOverdueRead(int) {
  _exit(25);
}

void ChecksPointIsolation(const ::outshine::Generators::Osm::StreetField &streets,
                          const ::outshine::Generators::Osm::OsmField &field,
                          uint64_t expected) {
  using namespace outshine::Test;
  const long pageSize = sysconf(_SC_PAGESIZE);
  CHECK(pageSize > 0, "OS page size exists");
  if (pageSize <= 0) { return; }
  const auto points = field.Points();
  const auto first = reinterpret_cast<uintptr_t>(points.data());
  const auto page = static_cast<uintptr_t>(pageSize);
  const auto interior = (first + page - 1) / page * page;
  CHECK(interior + page <= first + points.size_bytes(), "guard page lies inside point data");
  if (interior + page > first + points.size_bytes()) { return; }
  const pid_t child = fork();
  CHECK(child >= 0, "point isolation process starts");
  if (child == 0) {
    std::signal(SIGSEGV, RejectsRead);
    std::signal(SIGBUS, RejectsRead);
    std::signal(SIGALRM, StopsOverdueRead);
    alarm(2);
    if (mprotect(reinterpret_cast<void *>(interior), page, PROT_NONE) != 0) { _exit(22); }
    _exit(streets.SourceDigest(field, 0) == expected ? 0 : 24);
  }
  if (child < 0) { return; }
  int status = 0;
  pid_t waited;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) != 25,
        "point isolation watchdog did not expire");
  CHECK(waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "source digest lookup never reads protected coordinates");
}
}
#endif

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "road rules load");
  if (!templates.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  ::outshine::Generators::Osm::OsmField field(14, layers);
  std::array<::outshine::Generators::Osm::OsmField::Declared, 1> roads{
      {{.Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {47, 9, 47.0001, 9.0001}}}};
  CHECK(field.Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(), "road declared");
  ::outshine::Generators::Osm::StreetField streets;
  CHECK(streets.SourceDigest(field, 0) == kDigestBasis, "empty unbound street source stays empty");
  CHECK(streets.Ingest(field, templates) == 1 && streets.Ways().front().HalfWidthM == 0.75f,
        "path rule yields the independently specified half width");
  constexpr uint64_t expected = 0x29f3509d7c0d5050;
  CHECK(
      streets.SourceDigest(field, 0) == expected && streets.SourceDigest(field, 20) == kDigestBasis,
      "cached digest matches independent count/width/f64 byte oracle; untouched tiles stay empty");
  const size_t bytes = streets.HeapBytes();
  CHECK(streets.Ingest(field, templates) == 1 && streets.HeapBytes() == bytes &&
            streets.SourceDigest(field, 0) == expected,
        "repeated ingestion preserves identity/storage");
  auto snapshot = field.SnapshotQueries();
  ::outshine::Generators::Osm::StreetField old = streets;
  CHECK(old.SourceDigest(*snapshot, 0) == expected, "snapshot shares the original data context");
  ::outshine::Generators::Osm::OsmField foreign(14, layers);
  CHECK(foreign.Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(),
        "foreign road declared");
  CHECK(foreign.Generation() == field.Generation() && !streets.SourceDigest(foreign, 0),
        "equal generation and geometry from another owner cannot hit");
  roads.front().Value = "primary";
  CHECK(field.Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(),
        "new width declared");
  CHECK(!old.SourceDigest(field, 0) && old.SourceDigest(*snapshot, 0) == expected,
        "later generation cannot use old digest; its snapshot remains valid");
  CHECK(streets.Ingest(field, templates) == 1 && streets.Ways().front().HalfWidthM == 4.75f &&
            streets.SourceDigest(field, 0) != expected,
        "ingestion rebuilds after width change");
  const auto wide = streets.SourceDigest(field, 0);
  roads.front().LatLon.back() += 0.001;
  CHECK(field.Declare(roads, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value(),
        "new coordinates declared");
  CHECK(!streets.SourceDigest(field, 0) && streets.Ingest(field, templates) == 1 &&
            streets.SourceDigest(field, 0) != wide,
        "coordinate change invalidates and recomputes");
  CHECK(streets.Ingest(foreign, templates) == 1 && streets.SourceDigest(foreign, 0) == expected &&
            !streets.SourceDigest(field, 0),
        "foreign input replaces old context instead of appending");
  ::outshine::Generators::Osm::StreetField snapshotStreets;
  CHECK(snapshotStreets.Ingest(*snapshot, templates) == 1,
        "snapshot ingests in its own derived owner");
  const auto leased = snapshot->ShareOriginToken();
  const std::weak_ptr<const ::outshine::Generators::Osm::OsmField> released = snapshot;
  snapshot.reset();
  CHECK(released.expired() && leased != nullptr,
        "street context retains no OSM snapshot or coordinates");
  std::vector<::outshine::Generators::Osm::OsmField::Declared> many(8, roads.front());
  for (auto &road : many) {
    road.Value = "path";
    road.LatLon.clear();
    for (size_t point = 0; point < 512; ++point) {
      road.LatLon.push_back(47 + static_cast<double>(point) * 0.000001);
      road.LatLon.push_back(9 + static_cast<double>(point) * 0.000001);
    }
  }
  ::outshine::Generators::Osm::OsmField large(14, layers);
  CHECK(large.Declare(many, {.LongitudeDeg = 9, .LatitudeDeg = 47}).has_value() &&
            streets.Ingest(large, templates) == 8,
        "large road input ingests once");
  const auto largeDigest = streets.SourceDigest(large, 0);
  CHECK(largeDigest.has_value(), "large input has a cached digest");
#if defined(__unix__) || defined(__APPLE__)
  if (largeDigest) { ChecksPointIsolation(streets, large, *largeDigest); }
#else
  Skip("point page isolation requires POSIX");
#endif
  streets.Settle();
  CHECK(streets.SourceDigest(large, 0) == largeDigest &&
            streets.HeapBytes() >=
                8 * sizeof(::outshine::Generators::Osm::StreetField::Way) + sizeof(uint64_t),
        "settling retains digest and accounts its word storage");
  return Report();
}
