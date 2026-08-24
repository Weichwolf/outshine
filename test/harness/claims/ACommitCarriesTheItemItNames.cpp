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
  const std::vector<std::string> commits = Lines(Ask("git log --format=%H " + range));
  Note("commits this rule has bound so far", (double)commits.size(), "commits");

  std::vector<std::string> carrying;
  for (const std::string &commit : commits) {
    const std::string message = Ask("git log -1 --format=%B " + commit);
    const std::vector<std::string> named = NumbersIn(message, "board:");
    const std::vector<std::string> touched =
        NumbersIn(Ask("git show --name-only --format= " + commit + " -- board/"), "/");
    for (const std::string &one : touched) {
      bool spoken = false;
      for (const std::string &say : named) { spoken = spoken || say == one; }
      if (spoken) { continue; }
      carrying.push_back(Ask("git log -1 --format=%h" + std::string(" ") + commit) +
                         " touches board item " + one + " and its message names it nowhere");
    }
  }

  for (const std::string &one : carrying) { std::printf("FOUND %s", one.c_str()); }

  CHECK(carrying.empty(),
        "**A COMMIT CARRIES THE ITEM ITS MESSAGE NAMES**: `git log --grep 'board:NNNN'` is the "
        "documented way to find every commit on an item, and it is only as true as the "
        "messages. A commit that moves one item's file while its message names another was "
        "staged by sweeping rather than by what it is -- `git add -A board` while a parallel "
        "review files into the same directory is how it happens, measured three times in "
        "board:1802 and once more the hour that item was worked");

  Covers("IV.15 a commit that touches a board item names it: the log is the record and "
         "git log --grep is only as true as the messages (board:1802)");
  return Report();
}
