#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Ask(const std::string &cmd) {
  std::string said;
  std::FILE *const pipe = popen(cmd.c_str(), "r");
  if (pipe == nullptr) { return said; }
  char block[4096];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  pclose(pipe);
  return said;
}

[[nodiscard]] std::vector<std::string> Lines(const std::string &text) {
  std::vector<std::string> out;
  size_t from = 0;
  while (from < text.size()) {
    const size_t to = text.find('\n', from);
    const std::string one = text.substr(from, to == std::string::npos ? to : to - from);
    if (!one.empty()) { out.push_back(one); }
    if (to == std::string::npos) { break; }
    from = to + 1;
  }
  return out;
}

[[nodiscard]] std::vector<std::string> NumbersIn(const std::string &text, const char *after) {
  std::vector<std::string> out;
  const size_t step = std::string(after).size();
  for (size_t at = text.find(after); at != std::string::npos; at = text.find(after, at + 1)) {
    size_t from = at + step;
    while (from + 4 <= text.size() && std::isdigit((unsigned char)text[from])) {
      out.push_back(text.substr(from, 4));
      from += 4;
      if (from < text.size() && text[from] == ',') { ++from; continue; }
      break;
    }
  }
  return out;
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

    // board:1844: a reference is `board:NNNN`, including the comma list `board:1836,1837`. A
    // widening to "any four digits" let a MEASUREMENT stand in for a reference -- this
    // session's own messages carry 2528 (MB of corpus), 3600 (seconds in an hour) and 1181
    // (cases) -- and the board is at 1845 and climbing.
    std::vector<std::string> named = NumbersIn(message, "board:");
    for (const char *also : {"board/open/", "board/closed/", "board/active/"}) {
      for (const std::string &one : NumbersIn(message, also)) { named.push_back(one); }
    }

    // A commit already in the history cannot be restaged. board:1844 records why this one
    // stands: the hourly review writes its sharpened items as a bare list beside a verb, and
    // the reference this claim enforces is `board:NNNN`. The reviewer's instructions carry the
    // rule now; the commit that predates them is named here rather than widened around.
    const bool historical = commit.rfind("3f52567e", 0) == 0;
    for (const std::string &one : NumbersIn(files, "/")) {
      bool spoken = historical;
      for (const std::string &say : named) { spoken = spoken || say == one; }
      if (spoken) { continue; }
      carrying.push_back(commit.substr(0, 8) + " touches board item " + one +
                         " and its message names it nowhere");
    }
  }
  Note("commits this rule has bound so far", (double)commits, "commits");

  for (const std::string &one : carrying) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(carrying.empty(),
        "**A COMMIT CARRIES THE ITEM ITS MESSAGE NAMES**: `git log --grep 'board:NNNN'` is the "
        "documented way to find every commit on an item, and it is only as true as the "
        "messages. A commit that moves one item's file while its message names another was "
        "staged by sweeping rather than by what it is -- `git add -A board` while a parallel "
        "review files into the same directory is how it happens, measured three times in "
        "board:1802 and once more the hour that item was worked");

  Covers("IV.23 a commit that touches a board item names it: the log is the record and "
         "git log --grep is only as true as the messages (board:1802)");
  return Report();
}
