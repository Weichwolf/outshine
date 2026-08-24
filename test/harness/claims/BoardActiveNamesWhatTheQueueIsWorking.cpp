#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Ask(const std::string &command) {
  std::string said;
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) { return said; }
  char block[512];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  pclose(pipe);
  while (!said.empty() && (said.back() == '\n' || said.back() == ' ')) { said.pop_back(); }
  return said;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // CLAUDE.md: "board/active/ mirrors what is being worked on right now -- always." An agent
  // opening the board to learn what is in flight is told whatever stands there, and a stale
  // entry is worse than an empty directory: it is a wrong answer to the right question.
  std::vector<std::string> standing;
  for (const auto &entry : std::filesystem::directory_iterator("board/active")) {
    if (entry.path().extension() == ".md") { standing.push_back(entry.path().filename().string()); }
  }
  Note("items standing in board/active", (double)standing.size(), "items");
  for (const std::string &one : standing) { std::printf("NOTE active: %s\n", one.c_str()); }

  CHECK(!standing.empty(),
        "**board/active/ NAMES SOMETHING**: the queue is always working an item, and a "
        "directory that names none answers 'nothing is in flight' to an agent that is about "
        "to pick one (board:1783)");

  // freshness, and the bar is derived: the hourly architect files every hour, so an item
  // that has stood in active/ across two full review rounds without a commit is not being
  // worked -- it is parked in the wrong drawer.
  constexpr int kHoursOfTwoRounds = 2;
  std::vector<std::string> stale;
  for (const std::string &one : standing) {
    const std::string when =
        Ask("git log -1 --format=%ct -- 'board/*/" + one + "' 2>/dev/null");
    const std::string now = Ask("date +%s");
    if (when.empty() || now.empty()) { continue; }
    const long hours = (std::atol(now.c_str()) - std::atol(when.c_str())) / 3600;
    std::printf("NOTE %s last moved %ld hours ago\n", one.c_str(), hours);
    if (hours > kHoursOfTwoRounds) {
      stale.push_back(one + " has stood in board/active for " + std::to_string(hours) +
                      " hours without a commit");
    }
  }

  for (const std::string &one : stale) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(stale.empty(),
        "**AND WHAT IT NAMES IS ACTUALLY IN FLIGHT**: an item untouched across two review "
        "rounds is parked, not active, and a stale entry answers the wrong thing to an agent "
        "asking what the queue is doing (board:1783)");

  Covers("IV.14 board/active/ names what the queue is working and nothing else -- non-empty, "
         "and every entry moved by a commit inside two review rounds (board:1783)");
  return Report();
}
