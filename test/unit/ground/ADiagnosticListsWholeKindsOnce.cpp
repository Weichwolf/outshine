#include <cstdio>
#include <string>

#include "Check.h"

#include "RoadHarvest.h"

int main(void) {
  using namespace outshine::Test;
  using outshine::Ground::Listed;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string list;
  CHECK(!Listed(list, "way"), "an empty list holds nothing");
  list += "motorway ";
  CHECK(Listed(list, "motorway"), "a recorded kind is found whole");
  CHECK(!Listed(list, "way"),
        "**'way' IS NOT LISTED JUST BECAUSE 'motorway' IS** -- the substring dedup "
        "suppressed the very classes the diagnostic exists to name (board:1716)");
  CHECK(!Listed(list, "motor"), "and a prefix is not a listing either");
  list += "way ";
  CHECK(Listed(list, "way"), "the whole token lands beside its superstring and is found");
  CHECK(Listed(list, "motorway"), "without unseating the first");

  Covers("I.24 a harvest diagnostic lists whole kinds once: token-bounded matching, no "
         "substring suppression -- and the dead unlaned tally left with its unreachable "
         "condition (board:1716)");
  return Report();
}
