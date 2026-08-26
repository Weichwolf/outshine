#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Lines;
using outshine::Test::Run;

namespace {

// CLOSING AN ITEM IS DELETING ITS FILE, and the file must have said `State: active` when it went.
// The rule is in CLAUDE.md and until this claim was written nothing enforced it -- so an item
// could be filed and deleted in one breath, which is what happened to board:1927 in the very
// commit that filed it, by the hand that wrote this.
//
// Why the rule is worth a guard rather than a habit. `State: active` is the ONLY place the board
// says what has an owner right now. `grep -l '^State: active' board/*.md` is how anyone -- the
// architect, the stakeholder, the next session after a compaction -- answers "what is being
// worked on". An item that reaches closed without passing through active never appeared in that
// answer, so a whole piece of work happened where nobody could see it. Two people picking the
// same item is the failure this prevents, and it costs one commit to avoid.
//
// The walk: for every commit that DELETED a `board/NNNN_*.md`, read the file as it stood in that
// commit's parent. If its header did not say `State: active`, the item skipped the door.
//
// THE WINDOW BEGINS WHERE THE RULE BEGAN TO BE ENFORCED, and it finds its own start: the commit
// that added THIS FILE. Walked over all of history the count is 2579 deletions with 2513 of them
// straight from `State: open` -- the rule was written down and nothing checked it, so nobody
// followed it, including the hand that wrote this claim, in the same session. That history is
// the logbook and cannot be repaired; declaring 2513 standing reds would be a number nobody
// could ever drive to zero, and a red nobody can clear is one people learn to read past.
//
// So the window is derived, never quoted: `git log --diff-filter=A --format=%h -- <this file> |
// head -1`, the commit that most recently CREATED this path. A hardcoded hash would be exactly
// the stale control this tree has been paying for elsewhere, and anchoring on the last MODIFY
// instead would hand out an amnesty every time somebody improved a sentence in here -- the
// window would silently jump forward and every closure behind it would stop being checked.
// Creating the path is a deliberate act; editing it is not.
//
// An EMPTY window is reported and is not a failure -- a rule with nothing yet to judge has not
// failed -- but it is printed in capitals, because "no closure in the window" and "every closure
// passed" must never look alike to a reader.
constexpr const char *kThisClaim =
    "test/harness/claims/AnItemReachesClosedThroughActive.cpp";

struct Skipped {
  std::string Commit;
  std::string Item;
  std::string Said;
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string born;
  (void)Run(std::string("git log --diff-filter=A --format=%h -- ") + kThisClaim +
                " 2>/dev/null | head -1",
            born);
  while (!born.empty() && (born.back() == '\n' || born.back() == ' ')) { born.pop_back(); }
  CHECK(!born.empty(),
        "STALE WINDOW: this claim finds the start of its own window by asking git when it was "
        "committed, and git does not know this file -- so the walk below would judge either "
        "nothing or all of history, and neither is what the rule says");
  if (born.empty()) { return Report(); }

  std::string log;
  const int walked = Run("git log --diff-filter=D --name-only --format='%h' " + born +
                             "..HEAD -- 'board/*.md' 2>/dev/null",
                         log);
  CHECK(walked == 0,
        "git answers what this claim walks -- a claim that cannot read the history it judges is "
        "UNPREPARED, never green");
  if (walked != 0) { return Report(); }

  std::string at;
  std::vector<std::string> closed;
  std::vector<Skipped> skipped;
  size_t deletions = 0;
  for (const std::string &line : Lines(log)) {
    if (line.empty()) { continue; }
    if (line.compare(0, 6, "board/") != 0) {
      at = line;
      continue;
    }
    if (at.empty()) { continue; }
    ++deletions;
    std::string header;
    (void)Run("git show " + at + "^:" + line + " 2>/dev/null | head -12", header);
    if (header.empty()) { continue; }
    if (header.find("State: active") != std::string::npos) {
      closed.push_back(line);
      continue;
    }
    std::string said = "no State line at all";
    for (const std::string &row : Lines(header)) {
      if (row.compare(0, 7, "State: ") == 0) { said = row; break; }
    }
    skipped.push_back(Skipped{at, line, said});
  }

  std::printf("WINDOW starts at %s, the commit that created this claim\n", born.c_str());
  if (deletions == 0) {
    std::printf("NO CLOSURE IN THE WINDOW YET -- the rule has had nothing to judge\n");
  }
  std::printf("BOARD DELETIONS in the window %zu\n", deletions);
  std::printf("CLOSED THROUGH ACTIVE %zu, skipped the door %zu\n", closed.size(), skipped.size());
  for (const Skipped &one : skipped) {
    std::printf("  %s deleted %s which said '%s'\n", one.Commit.c_str(), one.Item.c_str(),
                one.Said.c_str());
  }

  CHECK(skipped.empty(),
        "**EVERY ITEM REACHED CLOSED THROUGH ACTIVE**: `State: active` is the only place the "
        "board says what has an owner right now, so an item deleted without passing through it "
        "was worked where nobody could see it -- and two people picking the same item is exactly "
        "what that one commit buys off");

  Covers("the board: closing an item is deleting its file, and the file said State: active when "
         "it went -- walked from the commit that added this claim to HEAD, with the window's "
         "start and its closure count printed so an empty window cannot read as a passing one");
  return Report();
}
