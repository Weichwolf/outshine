#include <cstdio>

#include "Check.h"

#include "Store.h"

using outshine::Entity;
using outshine::kNoEntity;
using outshine::Relation;
using outshine::Role;
using outshine::Store;
namespace tags = outshine::tags;

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Store scene;
  CHECK(scene.Open(32), "a store opens");

  const Entity mind = scene.Add(Role::Mind);
  Entity tools[6];
  for (int at = 0; at < 6; ++at) {
    tools[at] = scene.Add(Role::Tool);
    CHECK(scene.Link(mind, Relation::Uses, tools[at]),
          "a mind may use many tools -- six pairs in one holder, all in the Uses index");
  }
  CHECK(scene.Targets(mind, Relation::Uses, nullptr, 0) == 6, "six stand");

  scene.Remove(tools[2]);
  scene.Remove(tools[0]);
  scene.Remove(tools[5]);
  CHECK(scene.Targets(mind, Relation::Uses, nullptr, 0) == 3,
        "**ERASE SWAPS AND RELINKS WITHOUT LYING**: removing the middle, the first and the last "
        "tool erases their pairs from the holder's array -- each erase moves the last pair into "
        "the hole and relinks it, and the index answers exactly the three that stand");
  Entity still[6];
  const size_t kept = scene.Targets(mind, Relation::Uses, still, 6);
  CHECK(kept == 3 && scene.Alive(still[0]) && scene.Alive(still[1]) && scene.Alive(still[2]),
        "and every answered target is alive -- no tombstone answers");

  Entity fromSide[6];
  CHECK(scene.Sources(tools[1], Relation::Uses, fromSide, 6) == 1 &&
            fromSide[0] == mind,
        "the reverse index still names the one source after its neighbours moved");

  Entity pairsFrom[8], pairsTo[8];
  const size_t pairs = scene.Pairs(Relation::Uses, pairsFrom, pairsTo, 8);
  CHECK(pairs == 3,
        "**THE GLOBAL RELATION LIST AGREES**: three Uses pairs walk out of it, because unlink "
        "and relink maintain it through every swap");

  scene.Remove(mind);
  CHECK(scene.Pairs(Relation::Uses, nullptr, nullptr, 0) == 0 &&
            scene.Sources(tools[1], Relation::Uses, nullptr, 0) == 0,
        "removing the holder empties every index it fed -- nothing dangles from either side");
  CHECK(scene.Alive(tools[1]) && scene.Alive(tools[3]) && scene.Alive(tools[4]),
        "and Uses owns nothing, so the surviving tools stand");

  Covers("II.16 erase-swap-relink keeps every index true: the holder's array, the reverse "
         "index, and the global relation list agree through removals from middle, front, end, "
         "and the holder itself");
  return Report();
}
