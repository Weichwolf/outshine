#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "BoardNames.h"
#include "Check.h"
#include "Shell.h"

namespace Board = outshine::Test::Board;

namespace {

// A commit already in the history cannot be restaged. Each row names the commit AND the items
// whose bare spelling is the defect -- board:1852: an exemption that excuses everything a
// commit touched is wider than the violation, and one that nobody counts cannot be seen to
// have died when the range moves past it.
struct Excusal {
  const char *Commit;
  const char *Items;
  const char *Why;
};

constexpr Excusal kExcused[] = {
    {"3f52567e", "board:1610 board:1826 board:1831",
     "the hourly review wrote its sharpened items as a bare list beside a verb, before its "
     "instructions carried the rule that a reference is board:NNNN"},
};

[[nodiscard]] bool Excused(const std::string &commit, unsigned item, size_t &seen) {
  for (const Excusal &one : kExcused) {
    if (commit.rfind(one.Commit, 0) != 0) { continue; }
    if (!Board::NamedIn(one.Items).Holds(item)) { continue; }
    ++seen;
    return true;
  }
  return false;
}

[[nodiscard]] size_t ItemFilesIn(const std::string &files) {
  size_t items = 0;
  for (size_t at = 0; at < files.size(); at = files.find('\n', at) + 1) {
    const size_t ends = files.find('\n', at);
    const std::string line = files.substr(at, ends == std::string::npos ? ends : ends - at);
    const size_t slash = line.rfind('/');
    if (line.rfind("board/", 0) != 0 || slash == std::string::npos) { continue; }
    items += Board::DigitsAt(line, slash + 1) && line.size() > slash + Board::kDigits + 1 &&
             line[slash + Board::kDigits + 1] == '_';
    if (ends == std::string::npos) { break; }
  }
  return items;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // The rule binds from the commit that states it: history before this claim is what it was,
  // and board:1802 records the three commits that made the case for it.
  const std::string born =
      Lines(Ask("git log --diff-filter=A --format=%H -- "
                "test/harness/claims/ACommitCarriesTheItemItNames.cpp 2>/dev/null")).empty()
          ? std::string()
          : Lines(Ask("git log --diff-filter=A --format=%H -- "
                      "test/harness/claims/ACommitCarriesTheItemItNames.cpp 2>/dev/null")).back();

  const std::string range = born.empty() ? "-1" : born + "..HEAD";

  // board:1842: this used to spawn two processes PER COMMIT and the range only grows, so the
  // fast gate paid a cost that rises with the history for ever. One log call carries the hash,
  // the message and the board files it touched, separated by bytes no message holds.
  const std::string whole =
      Ask("git log --format='%x01%H%x02%B%x03' --name-only " + range + " -- board/ 2>/dev/null");

  std::vector<std::string> carrying;
  size_t commits = 0;
  size_t excused = 0;
  size_t unreadable = 0;
  size_t overflowed = 0;
  for (size_t at = whole.find('\x01'); at != std::string::npos; at = whole.find('\x01', at + 1)) {
    const size_t opens = whole.find('\x02', at);
    const size_t closes = whole.find('\x03', opens == std::string::npos ? at : opens);
    if (opens == std::string::npos || closes == std::string::npos) { break; }
    ++commits;
    const std::string commit = whole.substr(at + 1, opens - at - 1);
    const std::string message = whole.substr(opens + 1, closes - opens - 1);
    const size_t ends = whole.find('\x01', closes);
    const std::string files =
        whole.substr(closes + 1, ends == std::string::npos ? ends : ends - closes - 1);

    const Board::Named named = Board::NamedIn(message);
    const Board::Named touched = Board::NamedIn(files);
    unreadable += ItemFilesIn(files) > touched.Count ? 1 : 0;
    overflowed += named.Overflowed || touched.Overflowed ? 1 : 0;

    for (size_t one = 0; one < touched.Count; ++one) {
      const unsigned item = touched.Items[one];
      if (named.Holds(item) || Excused(commit, item, excused)) { continue; }
      carrying.push_back(commit.substr(0, 8) + " touches board item " +
                         std::to_string(item) + " and its message names it nowhere");
    }
  }
  Note("commits this rule has bound so far", (double)commits, "commits");
  Note("exemptions the table declares", (double)(sizeof kExcused / sizeof kExcused[0]), "rows");
  Note("times one was used this run", (double)excused, "items");
  for (const Excusal &one : kExcused) {
    std::printf("EXCUSED %s for items %s -- %s\n", one.Commit, one.Items, one.Why);
  }

  for (const std::string &one : carrying) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(unreadable == 0,
        "**AND EVERY BOARD PATH THE WALK IS HANDED IS ONE IT CAN READ**: the item numbers come "
        "from three declared directory spellings, so a file that lands beside them -- "
        "board/1844_x.md, a fourth directory -- would be walked past in silence rather than "
        "judged (board:1846)");
  CHECK(overflowed == 0,
        "**AND NO COMMIT NAMES MORE ITEMS THAN THE WALK HOLDS**: the reference table is a fixed "
        "64 wide, and a message or a file list that overruns it would drop references and read "
        "as a commit that named fewer items than it did (board:1846)");
  CHECK(excused > 0 || commits < 30,
        "**AND EVERY DECLARED EXEMPTION IS STILL REACHED**: the range this walk binds is derived "
        "from its own birth commit, so a rebase or a rename moves it -- an exemption the range "
        "has passed is a dead row that reads as a live one, and nothing would say so "
        "(board:1852)");
  CHECK(carrying.empty(),
        "**A COMMIT CARRIES THE ITEM ITS MESSAGE NAMES**: `git log --grep 'board:NNNN'` is the "
        "documented way to find every commit on an item, and it is only as true as the "
        "messages. A commit that moves one item's file while its message names another was "
        "staged by sweeping rather than by what it is -- `git add -A board` while a parallel "
        "review files into the same directory is how it happens, measured three times in "
        "board:1802 and once more the hour that item was worked");

  Covers("IV.23 a commit that touches a board item names it: the log is the record and "
         "git log --grep is only as true as the messages (board:1802) -- except the commits "
         "this file's kExcused table declares, each named beside the items it excuses, counted "
         "in the notes and refused the day the walked range no longer reaches one (board:1852)");
  return Report();
}
