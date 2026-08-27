#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// A CLIENT'S LINE COUNT IS THE ONE MEASUREMENT OF THE DOOR THIS TREE HAS, so the instrument must
// stand and must be complete.
//
// CLAUDE.md states the rule: a client is almost no code, and when a client needs much code the
// interface is the finding, never the client. That makes `apps/` a measuring device pointed at
// `include/` -- and an instrument that quietly loses a channel reads better than one that does
// not. A client added to `apps/` and absent from the Clients table would be a product whose cost
// to write nobody is counting, which is the one thing this measure exists to prevent.
//
// Unreal has no equivalent claim because it has no equivalent measure: its sample projects are
// content plus a launcher and the engine module is not judged by their size. RAGE's game IS its
// client, so the ratio is undefined there. Neither benchmark answers this, and the item says so --
// the number has no outside reference and only its own direction, which is that it must fall.
//
// What this refuses: a client under `apps/` with no row on the page, and a row that reports no
// line count. What it deliberately does NOT refuse is the number itself. A ceiling here would be
// this tree agreeing with itself about how big a client may be; the direction is the item's job
// (board:1947 names 120 lines for the viewer and 100 for the driver) and this is the instrument
// that lets the item be judged at all.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> built =
      Lines(Ask("find apps -mindepth 1 -maxdepth 1 -type d | sed 's|^apps/||' | sort"));
  const std::vector<std::string> page =
      Lines(Ask("sed -n '/^## Clients/,/^## Corpora/p' STATE.md | "
                "awk -F'|' '/apps\\// { gsub(/[ `]/, \"\", $2); gsub(/[ `]/, \"\", $4); "
                "sub(/^apps\\//, \"\", $4); if ($2 ~ /^[0-9]+$/) print $4, $2 }'"));

  std::printf("  clients under apps/: %zu   rows on the page: %zu\n", built.size(), page.size());
  for (const std::string &row : page) { std::printf("    %s\n", row.c_str()); }

  size_t missing = 0;
  for (const std::string &one : built) {
    bool found = false;
    for (const std::string &row : page) {
      if (row.rfind(one + " ", 0) == 0) { found = true; break; }
    }
    if (!found) {
      ++missing;
      std::printf("    MISSING  apps/%s is built and the Clients table does not count it\n",
                  one.c_str());
    }
  }

  CHECK(!built.empty(),
        "**THERE IS AT LEAST ONE CLIENT**: `apps/` is where a product stands on this engine, and "
        "a tree with no client has nothing measuring its own door");
  CHECK(missing == 0,
        "**EVERY CLIENT IS COUNTED ON THE PAGE**: a client is almost no code and its line count "
        "is the measurement of the door, so a program under `apps/` with no row in STATE.md's "
        "Clients table is a product whose cost to write nobody is watching");

  Covers("the door's own instrument: every program under apps/ carries a line count in STATE.md, "
         "so the one measure this tree has of its public interface cannot lose a channel unseen");
  return Report();
}
