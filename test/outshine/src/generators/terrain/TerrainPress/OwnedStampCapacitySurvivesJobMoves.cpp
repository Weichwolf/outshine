#include "Check.h"
#include "TerrainPress.h"

#include <cstddef>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;

  std::vector<EarthworkStamp> stamps;
  stamps.reserve(7);
  stamps.resize(2);
  stamps[0].RingEastNorthM.reserve(31);
  stamps[0].RingEastNorthM = {0, 0, 10, 0, 0, 10};
  stamps[0].SeamEastNorthM.reserve(19);
  stamps[0].HoleRingsEastNorthM.reserve(5);
  stamps[0].HoleRingsEastNorthM.resize(2);
  stamps[0].HoleRingsEastNorthM[0].reserve(17);
  stamps[0].HoleRingsEastNorthM[0] = {1, 1, 2, 1, 1, 2};
  stamps[0].HoleRingsEastNorthM[1].reserve(23);
  stamps[1].SeamEastNorthM.reserve(11);
  size_t expected = stamps.capacity() * sizeof(EarthworkStamp);
  for (const auto &stamp : stamps) {
    expected +=
        (stamp.RingEastNorthM.capacity() + stamp.SeamEastNorthM.capacity()) * sizeof(double);
    expected += stamp.HoleRingsEastNorthM.capacity() * sizeof(std::vector<double>);
    for (const auto &hole : stamp.HoleRingsEastNorthM) {
      expected += hole.capacity() * sizeof(double);
    }
  }
  Patchwork candidate;
  TerrainPressJob job(std::move(stamps), candidate, TangentFrame::At({}), {}, 30);
  CHECK(job.HeapBytes() == expected,
        "a zero-work job owns outer slots, rings, seams and retained empty hole capacity");
  TerrainPressJob moved(std::move(job));
  CHECK(moved.HeapBytes() == expected && job.HeapBytes() == 0,
        "moving the job transfers every owned stamp allocation once");
  CHECK(moved.Advance(1, 1) && moved.Take().Nodes == 0 && moved.HeapBytes() == expected,
        "completing a zero-work job retains its owned stamp capacity");

  std::vector<EarthworkStamp> empty;
  empty.reserve(3);
  const size_t emptyBytes = empty.capacity() * sizeof(EarthworkStamp);
  TerrainPressJob unstarted(std::move(empty), candidate, TangentFrame::At({}), {}, 30);
  CHECK(unstarted.HeapBytes() == emptyBytes, "empty input still owns its reserved stamp slots");
  moved = std::move(unstarted);
  CHECK(moved.HeapBytes() == emptyBytes && unstarted.HeapBytes() == 0,
        "move assignment retires the former stamp allocation and adopts the new owner");
  return Report();
}
