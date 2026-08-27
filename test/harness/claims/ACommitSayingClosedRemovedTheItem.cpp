#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// THE FILE IS THE STATE, so the commit that announces a closure must be the commit that performs
// it.
//
// CLAUDE.md: closing is DELETING the file, and what it said lives in the commit that removed it.
// A commit whose subject reads `board:NNNN closed` while `board/NNNN_*.md` still stands produces
// the one thing a flat-file board cannot survive -- a history saying the work is done and a
// directory saying it is open. Whichever a reader trusts, the other lies to them.
//
// Unreal's tracker and RAGE's both make the resolved state and the open query ONE thing; neither
// keeps two lists in step by hand. Here there is no database, so the mechanism has to be this
// walk. Two real failures sat behind one symptom when it was written: board:1963, whose commit
// added the missing asset and never touched the item, and board:1881, whose commit ran AHEAD of
// four unticked predicates. A closure announced and not performed, and a closure announced too
// early -- which is why a habit was never going to catch it.
//
// The subject line only. A closure names its number in the subject by convention, and a body may
// legitimately discuss other items ("1805, 1864, 1915 sharpened") -- reading bodies would flag
// every commit that mentions a closure it did not perform, which is most of them.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> announced = Lines(Ask(
      "git log --format='%s' 2>/dev/null | "
      "sed -n 's/.*board:\\([0-9][0-9]*\\) closed.*/\\1/p' | sort -u"));

  std::vector<std::string> standing;
  for (const std::string &one : announced) {
    const std::string found = Ask("ls board/" + one + "_*.md 2>/dev/null | head -1");
    if (!found.empty()) { standing.push_back(one + "  " + found); }
  }

  std::printf("  %zu closure(s) announced in a commit subject, %zu still on the board\n",
              announced.size(), standing.size());
  for (const std::string &one : standing) { std::printf("    %s\n", one.c_str()); }

  CHECK(!announced.empty(),
        "**THE WALK FOUND CLOSURES TO JUDGE**: an empty history reads the same as a clean one, so "
        "the count is printed before the verdict rather than after it");
  CHECK(standing.empty(),
        "**A COMMIT THAT SAYS `closed` HAS DELETED THE FILE**: the file IS the state on this "
        "board, so a history saying the work is done beside a directory saying it is open leaves "
        "a reader with two answers and no way to pick one");

  Covers("the board against its own history: no commit subject announces a closure the working "
         "tree has not performed");
  return Report();
}
