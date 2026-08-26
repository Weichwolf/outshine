#include <cstdio>
#include <string>
#include <map>
#include <set>
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
// A DELETION IS NOT ALWAYS A CLOSURE, and the first version of this claim said it was. Two
// reviewers filing into the same number block in the same minute renumber one of them, and a
// renumber is a delete here and an add there -- in two separate commits, so git's own rename
// detection cannot pair them at any threshold. It fired on `71e5679f` and called a renumber a
// closure that skipped the door.
//
// So a deletion is judged as a closure only when the item SURVIVES NOWHERE, and getting that
// right took three tries -- each wrong version is recorded because the next reader will reach
// for the same two:
//
//   "its title stands at HEAD"        excuses nothing it should, but a renumbered item closed
//                                     properly a few hours later leaves no title at HEAD and its
//                                     earlier MOVE reads as a door-skip again
//   "its title was added in the window"  excuses EVERYTHING: an item filed and closed in one
//                                     window has its own title in that set, and six real
//                                     closures were waved through
//
// The distinction is ORDER. A move's add comes AFTER its delete; a file-then-close comes before.
// So every commit in the window is numbered (git log prints newest first, so a SMALLER index is
// later), and a deletion is a move only when the same ITEM NUMBER was filed at a strictly later
// commit.
//
// AN ITEM LEAVES IN TWO WAYS. A CLOSURE says a defect was real and is gone, and it names the
// proving test. A WITHDRAWAL says the defect was never there -- the premise was mine and wrong --
// and it names what was misread. Both are legitimate exits and both are recorded in the file before
// it goes, so this claim accepts `State: active` or `State: withdrawn` and refuses `State: open`,
// which is a file that left without anyone saying which of the two it was.
//
// KEYED ON THE NUMBER OR THE TITLE, because an item can move in two ways and each keeps one of them:
//
//   RETITLED   the number stays and the title changes -- board:1966, reframed in one commit
//   RENUMBERED the title stays and the number changes -- board:1932 became 1937 after a number
//              collision with the stakeholder's worktree
//
// A rule that watched only the title called the first a closure that skipped `active`; a rule that
// watched only the number called the second one. A genuine closure refiles NEITHER, so watching
// both excuses exactly the two kinds of move and nothing else.
//
// AT THE SAME COMMIT OR LATER, because a rename happens in ONE commit -- git records a delete and an
// add and nothing distinguishes them by time. That does not reopen the hole the older rule had: a
// number is identity and never repeats, so a same-commit number match can only be a rename; and a
// same-commit TITLE match is a renumber, which is the other move. Neither can be a closure, because
// a closure files nothing.
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

[[nodiscard]] std::string Numbered(const std::string &path) {
  const size_t slash = path.rfind('/');
  const size_t at = slash == std::string::npos ? 0 : slash + 1;
  if (path.size() < at + 4) { return std::string(); }
  for (size_t step = 0; step < 4; ++step) {
    if (path[at + step] < '0' || path[at + step] > '9') { return std::string(); }
  }
  return path.substr(at, 4);
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

  // Every title the window FILED, under any number. A deletion whose title is in here was moved.
  std::string added;
  (void)Run("git log --diff-filter=A --name-only --format='%h' " + born +
                "..HEAD -- 'board/*.md' 2>/dev/null",
            added);
  std::string ordered;
  (void)Run("git log --format='%h' " + born + "..HEAD 2>/dev/null", ordered);
  std::map<std::string, size_t> whenCommitted;
  {
    size_t index = 0;
    for (const std::string &line : Lines(ordered)) {
      if (!line.empty()) { whenCommitted[line] = index++; }
    }
  }

  std::map<std::string, size_t> filedAt;
  {
    std::string commit;
    for (const std::string &line : Lines(added)) {
      if (line.empty()) { continue; }
      if (line.compare(0, 6, "board/") != 0) {
        commit = line;
        continue;
      }
      if (commit.empty()) { continue; }
      const auto placed = whenCommitted.find(commit);
      if (placed == whenCommitted.end()) { continue; }
      std::string title;
      (void)Run("git show " + commit + ":" + line + " 2>/dev/null | sed -n 's|^# ||p' | head -1",
                title);
      while (!title.empty() && (title.back() == '\n' || title.back() == ' ')) { title.pop_back(); }
      for (const std::string &key : {Numbered(line), title}) {
        if (key.empty()) { continue; }
        const auto held = filedAt.find(key);
        if (held == filedAt.end() || placed->second < held->second) {
          filedAt[key] = placed->second;
        }
      }
    }
  }

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
  size_t deletions = 0, moved = 0;
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

    std::string titled;
    (void)Run("git show " + at + "^:" + line + " 2>/dev/null | sed -n 's|^# ||p' | head -1", titled);
    while (!titled.empty() && (titled.back() == '\n' || titled.back() == ' ')) { titled.pop_back(); }
    bool wasMoved = false;
    for (const std::string &key : {Numbered(line), titled}) {
      if (key.empty()) { continue; }
      const auto refiled = filedAt.find(key);
      const auto deleted = whenCommitted.find(at);
      if (refiled != filedAt.end() && deleted != whenCommitted.end() &&
          refiled->second <= deleted->second) {
        wasMoved = true;
      }
    }
    if (wasMoved) {
      ++moved;
      continue;
    }

    if (header.find("State: active") != std::string::npos ||
        header.find("State: withdrawn") != std::string::npos) {
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
  std::printf("BOARD DELETIONS in the window %zu, of which %zu were MOVES -- the same title was "
              "filed again at a LATER commit\n", deletions, moved);
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
