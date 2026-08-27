#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// `board/` IS A GRAPH AND A CLOSURE DELETES A NODE.
//
// `Parent` says which feature an item serves; `Depends` says what has to land before it can be
// worked. Closing an item DELETES the file -- what it said lives in the commit -- so both fields
// can outlive the thing they name. A reader following the graph then arrives at nothing and
// cannot tell whether that work was done, was withdrawn, or was never there.
//
// Unreal surfaces a `.uasset` referencing a deleted asset as a load-time warning rather than a
// silent null; RAGE's build refuses a missing dependency in its resource graph. Both report the
// dangling reference where it is found instead of following it into nothing, and the reason is
// the same in a board as in an asset graph: the cost is a WRONG READING, not a broken link.
// board:1957's last predicate said it waits on `Depends: 1950` after 1950 had closed, so a reader
// could not learn whether the wait was over -- and it was blocked by something else entirely.
//
// `Supersedes` IS DELIBERATELY NOT COUNTED. A superseded item is deleted BY the item that
// supersedes it, so a dangling `Supersedes` is the field working exactly as designed. Counting it
// would flag forty correct rows, and a claim that cries wolf gets turned off within a week.

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> dangling = Lines(Ask(
      "have=$(ls board/*.md | grep -o '[0-9]\\{4\\}' | sort -u); "
      "for f in board/*.md; do "
      "  n=$(basename \"$f\" | grep -o '^[0-9]*'); "
      "  for field in Parent Depends; do "
      "    sed -n \"s/^$field: //p\" \"$f\" | tr ',' '\\n' | tr -d ' ' | while read -r d; do "
      "      [ -n \"$d\" ] || continue; "
      "      printf '%s\\n' \"$have\" | grep -qx \"$d\" || printf '%s %s %s\\n' \"$n\" \"$field\" \"$d\"; "
      "    done; "
      "  done; "
      "done"));

  const std::string items = Ask("ls board/*.md | wc -l | tr -d ' '");
  std::printf("  %s item(s) on the board, %zu dangling Parent/Depends edge(s)\n", items.c_str(),
              dangling.size());
  for (const std::string &one : dangling) { std::printf("    %s names nothing\n", one.c_str()); }

  CHECK(dangling.empty(),
        "**A BOARD EDGE POINTS AT AN ITEM THAT STANDS**: closing an item deletes its file, so a "
        "`Parent` or `Depends` naming it becomes a reference into nothing -- and a reader "
        "following it cannot tell whether that work was done, withdrawn, or never there. The "
        "closing commit is where an edge gets cleaned up, and this is what remembers to");

  Covers("the board's own graph: every `Parent` and `Depends` names an item that is still in "
         "`board/`, so following the graph never arrives at nothing");
  return Report();
}
