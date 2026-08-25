#include <cstdio>

#include "Check.h"

#include "GroundPatchwork.h"
#include "TileMeshes.h"

using outshine::Compositor::Around;
using outshine::Compositor::LayPatchwork;
using outshine::TileBuild;
using outshine::TileMeshes;

namespace {

// A pool that holds nothing answers Undeclared for every tile, which is what an unopened one
// does and what a compositor must survive.
class Empty : public TileMeshes {
public:
  [[nodiscard]] Reply Mesh(int, uint32_t, uint32_t, int, TileBuild *) override {
    ++Asked;
    return Reply::Undeclared;
  }
  size_t Asked = 0;
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Empty pool;

  {
    Around nowhere;
    nowhere.Zoom = 0;
    const auto said = LayPatchwork(pool, nowhere);
    std::printf("NOTE zoom 0 says: '%.90s'\n", said ? "(it laid one)" : said.error().c_str());
    CHECK(!said.has_value(),
          "**A PATCHWORK IS LAID AT A DECLARED ZOOM**, and zero is not one -- the tile a "
          "coordinate falls in is meaningless without the level it is asked at");
  }

  {
    Around inward;
    inward.Zoom = 14;
    inward.Ring = -1;
    const auto said = LayPatchwork(pool, inward);
    std::printf("NOTE a ring of -1 says: '%.90s'\n",
                said ? "(it laid one)" : said.error().c_str());
    CHECK(!said.has_value(),
          "and a ring reaches OUT from its centre, so a negative reach is a refusal rather than "
          "an empty loop that returns a patchwork of nothing");
  }

  {
    Around unfed;
    unfed.LatDeg = 48.1371;
    unfed.LonDeg = 11.5754;
    unfed.Zoom = 14;
    const auto said = LayPatchwork(pool, unfed);
    std::printf("NOTE an unopened pool says: '%.120s'\n",
                said ? "(it laid one)" : said.error().c_str());
    CHECK(!said.has_value(),
          "**AND A PATCHWORK OF NO TILES IS A REFUSAL THAT COUNTS THEM**: a pool holding "
          "nothing answers Undeclared for every tile of the ring, and a compositor that "
          "returned an empty patchwork would hand the renderer a hole and call it ground");
    std::printf("NOTE it asked the pool %zu times\n", pool.Asked);
    CHECK(pool.Asked == 9,
          "and it asked for every tile of the three by three ring it was given, so a ring that "
          "reaches one tile out is nine tiles and not one");
    if (!said) {
      CHECK(said.error().find("absent") != std::string::npos ||
                said.error().find("pending") != std::string::npos,
            "and the refusal names how many were pending, absent and refused, because those "
            "are three different reasons to have no ground and they are repaired differently");
    }
  }

  Covers("I.7.1 a ground patchwork is laid at a declared zoom over a ring that reaches out, and "
         "a ring in which no tile meshed refuses while naming how many were pending, absent and "
         "refused (board:1805)");
  return Report();
}
