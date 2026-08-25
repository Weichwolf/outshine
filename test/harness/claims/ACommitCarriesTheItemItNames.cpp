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

    std::vector<std::string> named;
    for (size_t scan = 0; scan + 4 <= message.size(); ++scan) {
      bool four = true;
      for (size_t step = 0; step < 4; ++step) {
        four = four && std::isdigit((unsigned char)message[scan + step]);
      }
      if (!four) { continue; }
      if (scan > 0 && std::isdigit((unsigned char)message[scan - 1])) { continue; }
      if (scan + 4 < message.size() && std::isdigit((unsigned char)message[scan + 4])) { continue; }
      named.push_back(message.substr(scan, 4));
    }

    for (const std::string &one : NumbersIn(files, "/")) {
      bool spoken = false;
      for (const std::string &say : named) { spoken = spoken || say == one; }
      if (spoken) { continue; }
      carrying.push_back(commit.substr(0, 8) + " touches board item " + one +
                         " and its message names it nowhere");
    }
  }
  Note("commits this rule has bound so far", (double)commits, "commits");
  Note("processes the walk spawns", 2.0, "popen");

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
