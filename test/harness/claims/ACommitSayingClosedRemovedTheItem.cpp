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

namespace {


constexpr const char *kThisClaim = "test/harness/claims/ACommitSayingClosedRemovedTheItem.cpp";

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // THE WINDOW STARTS WHERE THIS CLAIM DOES, which is the same shape
  // AnItemReachesClosedThroughActive uses and for the same reason. Before it, ids were reused
  // once their file was deleted (board:1988), so `board:1986` names two unrelated bodies of work
  // and no walk can say which one a closure meant. Judging that history would report live items
  // as unperformed closures for ever. Derived, never quoted: the commit that ADDED this file.
  const std::string born =
      Ask(std::string("git log --diff-filter=A --format=%h -- ") + kThisClaim +
          " 2>/dev/null | tail -1");
  if (born.empty()) {
    Unprepared("this claim is not committed yet, so it has no window to judge");
    return Report();
  }

  const std::vector<std::string> announced = Lines(Ask(
      "git log --format='%s' " + born + "..HEAD 2>/dev/null | "
      "sed -n 's/.*board:\\([0-9][0-9]*\\) closed.*/\\1/p' | sort -u"));

  std::vector<std::string> standing;
  for (const std::string &one : announced) {
    const std::string found = Ask("ls board/" + one + "_*.md 2>/dev/null | head -1");
    if (!found.empty()) { standing.push_back(one + "  " + found); }
  }

  std::printf("WINDOW starts at %s, the commit that added this claim\n", born.c_str());
  std::printf("  %zu closure(s) announced in it, %zu still on the board\n", announced.size(),
              standing.size());
  if (announced.empty()) {
    std::printf("  NO CLOSURE IN THE WINDOW YET -- the rule has had nothing to judge\n");
  }
  for (const std::string &one : standing) { std::printf("    OPEN  %s\n", one.c_str()); }

  CHECK(standing.empty(),
        "**A COMMIT THAT SAYS `closed` HAS DELETED THE FILE**: the file IS the state on this "
        "board, so a history saying the work is done beside a directory saying it is open leaves "
        "a reader with two answers and no way to pick one");

  Covers("the board against its own history: no commit since this claim was written announces a "
         "closure the working tree has not performed");
  return Report();
}
