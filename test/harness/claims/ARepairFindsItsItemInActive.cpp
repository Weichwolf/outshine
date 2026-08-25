#include <cstdio>
#include <string>
#include <vector>

#include "BoardNames.h"
#include "Check.h"
#include "Shell.h"

namespace Board = outshine::Test::Board;

using outshine::Test::Ask;
using outshine::Test::Lines;

namespace {

// CLAUDE.md is the MAP, not code worked under an item: the hourly review rewrites it in the
// same commit it files into board/, and that is its job rather than a repair. Everything else
// outside board/ is the tree.
[[nodiscard]] bool Touches(const std::string &files, bool outsideTheBoard) {
  for (const std::string &line : Lines(files)) {
    if (line == "CLAUDE.md" || line.rfind(".claude/", 0) == 0) { continue; }
    const bool board = line.rfind("board/", 0) == 0;
    if (board != outsideTheBoard) { return true; }
  }
  return false;
}

[[nodiscard]] bool StoodIn(const std::string &where, const std::string &commit, unsigned item) {
  char number[8] = {};
  std::snprintf(number, sizeof number, "%04u_", item);
  return !Ask("git ls-tree -r --name-only " + commit + " " + where + " 2>/dev/null | grep '/" +
              number + "'")
              .empty();
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string self = "test/harness/claims/ARepairFindsItsItemInActive.cpp";
  const std::string birth =
      Ask("git log --diff-filter=A --format=%H -- " + self + " 2>/dev/null | tail -1");
  const bool born = birth.size() == 40;
  std::printf("NOTE the rule binds from %s\n",
              born ? birth.c_str() : "(uncommitted -- nothing yet in window)");
  CHECK(!Ask("git rev-parse --git-dir 2>/dev/null").empty(),
        "the walk can read the board's own history");
  CHECK(born || Ask("git ls-files " + self).empty(),
        "**AND A WALK ANCHORED TO ITS OWN BIRTH FOUND THAT BIRTH**: this proof is versioned, so "
        "an empty window means the anchor did not parse, not that there is nothing to judge -- "
        "and a walk over nothing collides with nothing and reports PASS, which is how "
        "board:1854 silenced this claim for eleven commits (board:1857)");

  std::vector<std::string> jumped;
  std::vector<unsigned> judged;
  size_t repairs = 0;
  size_t named = 0;
  if (born) {
    const std::string whole = Ask("git log --no-renames --format='%x01%H%x02%B%x03' --name-only " +
                                  birth + "..HEAD 2>/dev/null");
    for (size_t at = whole.find('\x01'); at != std::string::npos;
         at = whole.find('\x01', at + 1)) {
      const size_t opens = whole.find('\x02', at);
      const size_t closes = whole.find('\x03', opens == std::string::npos ? at : opens);
      if (opens == std::string::npos || closes == std::string::npos) { break; }
      const std::string commit = whole.substr(at + 1, opens - at - 1);
      const std::string message = whole.substr(opens + 1, closes - opens - 1);
      const size_t ends = whole.find('\x01', closes);
      const std::string files =
          whole.substr(closes + 1, ends == std::string::npos ? ends : ends - closes - 1);

      if (!Touches(files, true)) { continue; }
      // The SUBJECT is the assignment; the body explains, and an explanation cites its
      // neighbours -- "board:1854's control was blind" is a reference, not a claim to be
      // working on 1854. IV.23 reads the whole message because there every file touched must
      // be named somewhere; here the question is which item the commit is FOR.
      const std::string subject = message.substr(0, message.find('\n'));
      const Board::Named names = Board::NamedIn(subject);
      if (names.Count == 0) { continue; }
      ++repairs;
      for (size_t one = 0; one < names.Count; ++one) {
        const unsigned item = names.Items[one];
        // IV.16's rule, for the same reason: what matters is an item's LATEST work, not every
        // commit it ever carried. An item worked out of turn and then moved through
        // board/active really does pass through the drawer -- that is compliance, and walking
        // every commit instead would leave a violation red for ever with no repair available
        // but rewriting history.
        bool spoken = false;
        for (const unsigned already : judged) { spoken = spoken || already == item; }
        if (spoken) { continue; }
        judged.push_back(item);
        // An item filed and worked in one commit exists in this tree and not its parent's;
        // asking only the parent would let that shape through unjudged.
        if (!StoodIn("board/", commit + "^", item) && !StoodIn("board/", commit, item)) {
          continue;
        }
        ++named;
        if (StoodIn("board/active/", commit + "^", item) ||
            StoodIn("board/active/", commit, item)) {
          continue;
        }
        jumped.push_back(commit.substr(0, 9) + " repairs code under board:" +
                         std::to_string(item) + ", which stood outside board/active");
      }
    }
  }

  Note("commits that changed code under an item's name", (double)repairs, "commits");
  Note("item namings those commits carried", (double)named, "namings");
  for (const std::string &one : jumped) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(jumped.empty(),
        "**A REPAIR FINDS ITS ITEM IN board/active**: CLAUDE.md says board/active mirrors what "
        "is being worked on RIGHT NOW, and a drawer that fills at closing time answers nobody "
        "-- IV.16 guards the state machine's exit, and three items were repaired in src/ and "
        "test/ straight out of board/open before anything guarded its entry (board:1856)");

  Covers("IV.33 a commit that changes code under an item's name finds that item in "
         "board/active, in its own tree or its parent's -- the entry to the state machine "
         "IV.16 guards at the exit (board:1856)");
  return Report();
}
