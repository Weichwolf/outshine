#include "TileAdmission.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;

  const std::array<::outshine::Generators::Osm::OsmField::Tile, 2> tiles{
      {{.Z = 14, .X = 0, .Y = 1, .FirstFeature = 0, .FeatureCount = 1},
       {.Z = 14, .X = 1, .Y = 1, .FirstFeature = 1, .FeatureCount = 1}}};
  ::outshine::Generators::Osm::TileAdmission source;
  source.Take(0);
  source.Take(1);

  CHECK(source.Done(tiles) && source.Takes() == 2,
        "the scan can pass two reserved tiles before either product is accepted");

  ::outshine::Generators::Osm::TileAdmission snapshot = source;
  const std::array<uint32_t, 1> accepted{0};
  CHECK(snapshot.ReleaseUnaccepted(accepted) == 1 && snapshot.Takes() == 1 && !snapshot.Done(tiles),
        "the snapshot releases a pending reservation even behind the admission state");
  const ::outshine::Generators::Osm::TileAdmission::Next retry =
      snapshot.Ask(tiles, {.CentreX = 0, .CentreY = 1, .Rings = kEveryRing}, [](size_t, size_t) {
        return true;
      });
  CHECK(retry.Found && retry.Tile == 1,
        "the snapshot retries only the tile without an accepted product");
  CHECK(source.Done(tiles) && source.Takes() == 2,
        "the source retains both reservations without a feature scan");
  const pid_t child = fork();
  CHECK(child >= 0, "release invariant probe starts");
  if (child == 0) {
    source.Release(1);
    CHECK(source.Takes() == 1 && !source.Done(tiles),
          "a discarded worker releases its reservation after all tiles were admitted");
    const auto released = source.Ask(tiles,
                                     {.CentreX = 0, .CentreY = 1, .Rings = kEveryRing},
                                     [](size_t, size_t) { return true; });
    CHECK(released.Found && released.Tile == 1,
          "admission retries the released tile while another held tile stays unavailable");
    source.Take(1);

    CHECK(source.Done(tiles) && source.Takes() == 2,
          "replacement reservation completes admission without duplicating the other owner");
    source.Release(0);
    const auto firstReleased = source.Ask(tiles,
                                          {.CentreX = 0, .CentreY = 1, .Rings = kEveryRing},
                                          [](size_t, size_t) { return true; });
    CHECK(firstReleased.Found && firstReleased.Tile == 0 && source.Takes() == 1,
          "releasing the earlier owner cannot release the replacement behind it");
    std::_Exit(Failures.Value() == 0 ? 0 : 1);
  }
  if (child > 0) {
    int status = 0;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
          "advanced reservations release and retry without violating ownership invariants");
  }
  return Report();
}
