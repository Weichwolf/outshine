#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

// `board/` IS WHAT IS TRUE NOW, and a path in an item is a claim that the path exists.
//
// An item citing `src/engine/GltfStudio.cpp` says that file is there. A reader who greps for it
// and finds nothing reads the item as already fixed. board:1547 was exactly that: it counted ten
// conversion sites in a file that had since been renamed, so the count read as gone. Re-measured,
// it was fourteen, in two other files -- the defect had GROWN while its citation rotted.
//
// Unreal surfaces a .uasset referencing a deleted asset as a load-time warning and RAGE's build
// refuses a missing dependency. Both report a reference into nothing where it is found. What to
// do about it HERE is a choice neither answers, because a board item is prose and prose may
// legitimately quote the past -- "this used to live in X" is a true sentence about a file that is
// gone. Forbidding that would push items into vagueness, which is worse than a stale path.
//
// So the count is DECLARED and may only fall, the same instrument as TheEngineNamesNoSubject and
// --audit-access. An item that repairs a citation lowers it in the commit that repaired it; one
// that adds a stale citation is caught here. Four refactors account for nearly all of today's:
// the `src/engine/Sim.cpp` split, the `include/outshine/` flattening, `src/scene` moving behind
// the door, and the generator tier moving to `src/generators`.
//
// THIS NUMBER WAS WRONG TWICE. First 19 -- the walk that produced it ended in a `head -24`, so
// the count was whatever fitted on a screen. Then 30 -- and the extra ten were board:1987
// ITSELF, whose first draft listed each dead path in full, so a walk looking for dead paths in
// `board/*.md` found them there and counted them. The measure was inside what it measured, which
// is why that item lists directories now and not files.
//
// A declared ceiling is quoted rather than re-derived. That is what it is FOR, and why it has to
// be right before it is written down.

namespace {

// What the board held when this claim was written. It may only fall.
constexpr size_t kCitationsIntoNothing = 20;

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> stale = Lines(Ask(
      "for f in board/*.md; do "
      "  n=$(basename \"$f\" | grep -o '^[0-9]*'); "
      "  grep -oE '(src|include|apps|test)/[A-Za-z0-9_/.-]+[.](cpp|h|sh|md)' \"$f\" | sort -u | "
      "  while read -r p; do [ -e \"$p\" ] || printf '%s %s\\n' \"$n\" \"$p\"; done; "
      "done"));

  std::printf("  citations into nothing: %zu, declared %zu\n", stale.size(),
              kCitationsIntoNothing);
  for (const std::string &one : stale) { std::printf("    %s\n", one.c_str()); }

  CHECK(stale.size() <= kCitationsIntoNothing,
        "**AN ITEM CITES A FILE THAT STANDS**: `board/` is what is true NOW, so a path in an item "
        "is a claim the path exists -- and a reader who greps for it and finds nothing reads the "
        "item as already fixed. board:1547 counted ten sites in a renamed file; it was fourteen, "
        "in two others");
  CHECK(stale.size() >= kCitationsIntoNothing,
        "**A COUNT THAT FELL IS RECORDED WHERE IT FELL**: not a failure -- the claim asking for "
        "its own number to be lowered in the commit that repaired the citation, so the next "
        "reader sees what the board holds rather than what it once held");

  Covers("the board's own citations: no item gains a path into nothing, and every one that is "
         "repaired lowers the declared count in the commit that repaired it");
  return Report();
}
