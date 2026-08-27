#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// AN ID IS THE ONE THING ON THIS BOARD THAT MUST NEVER BE REUSED.
//
// CLAUDE.md: the number is identity. Everything the history knows about a piece of work is
// reached by it -- `git log --grep 'board:1986'` is how a reader learns what was decided and why
// after the file is gone. Issue that number twice and the query returns two unrelated bodies of
// work with no way to tell them apart, and the logbook stops being a logbook.
//
// It happened because the next id was taken from the DIRECTORY:
//
//     ls board/*.md | grep -o '[0-9]{4}' | sort -n | tail -1
//
// which only remembers ids still standing, so closing 1986 freed 1986. Seven were reused before
// anyone noticed: 1870, 1893, 1932, 1933, 1966, 1970, and board:1988 itself, which had to be
// renumbered off 1987 to stop being its own false positive. The fix is one word -- the next id
// comes from the HISTORY -- and this is what makes the word stick.
//
// Neither benchmark has this problem to answer: Unreal and RAGE both track work in databases that
// issue ids and never hand one back. A flat-file board has no such machine, so the machine is
// this walk.
//
// THE WINDOW STARTS WHERE THIS CLAIM DOES. The seven reuses are written and cannot be unwritten;
// judging them would leave a permanent red that gets ignored, and an ignored claim reads as a
// passing one. What this refuses is a reuse from here on.

namespace {

constexpr const char *kThisClaim = "test/harness/claims/ABoardIdIsIssuedOnce.cpp";

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string born =
      Ask(std::string("git log --diff-filter=A --format=%h -- ") + kThisClaim +
          " 2>/dev/null | tail -1");
  if (born.empty()) {
    Unprepared("this claim is not committed yet, so it has no window to judge");
    return Report();
  }

  const std::vector<std::string> twice = Lines(Ask(
      "git log --diff-filter=A --name-only --format='' " + born + "..HEAD 2>/dev/null | "
      "sed -n 's|^board/\\([0-9][0-9][0-9][0-9]\\)_.*|\\1|p' | sort > /tmp/outshine-ids-new; "
      "git log --diff-filter=A --name-only --format='' " + born +
      " 2>/dev/null | sed -n 's|^board/\\([0-9][0-9][0-9][0-9]\\)_.*|\\1|p' | sort -u "
      "> /tmp/outshine-ids-old; "
      "comm -12 /tmp/outshine-ids-new /tmp/outshine-ids-old"));

  const std::string highest = Ask(
      "git log --all --diff-filter=A --name-only --format='' 2>/dev/null | "
      "sed -n 's|^board/\\([0-9]*\\)_.*|\\1|p' | sort -n | tail -1");

  std::printf("WINDOW starts at %s; the history's highest id ever filed is %s\n", born.c_str(),
              highest.c_str());
  std::printf("  ids issued again since: %zu\n", twice.size());
  for (const std::string &one : twice) { std::printf("    REUSED %s\n", one.c_str()); }

  CHECK(twice.empty(),
        "**A BOARD ID IS ISSUED ONCE AND NEVER AGAIN**: the number is how the history is queried "
        "after the file is gone, so reusing it makes `git log --grep 'board:NNNN'` return two "
        "unrelated bodies of work. Take the next id from the HISTORY, which remembers every one "
        "ever filed, not from the directory, which remembers only those still standing");

  Covers("the board's identity: no id filed since this claim was written repeats one the history "
         "had already issued");
  return Report();
}
