#include <cstdio>
#include <cstdlib>
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

[[nodiscard]] std::vector<std::string> Lines(const std::string &block) {
  std::vector<std::string> out;
  size_t at = 0;
  while (at <= block.size()) {
    const size_t stop = block.find('\n', at);
    const size_t end = stop == std::string::npos ? block.size() : stop;
    if (end > at) { out.push_back(block.substr(at, end - at)); }
    if (stop == std::string::npos) { break; }
    at = stop + 1;
  }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1793. The companion claim asks whether what STANDS in board/active is fresh, and an
  // empty drawer is a legal answer to it (board:1790). So the cheapest way to keep that claim
  // green is to never use the drawer -- and for three closures under board:1783's rule, that is
  // exactly what happened. This claim is anchored to the event the queue cannot avoid
  // producing: a file arriving under board/closed. It must have stood under board/active in
  // that same commit's parent tree.
  //
  // The window starts at THIS proof's own birth commit -- derived, not chosen. The rule binds
  // work done after the rule can be checked, and history is not rewritten to make a new gate
  // green. Until this file is committed the window is empty, and the claim says so.
  const std::string self = "test/harness/claims/AnItemReachesClosedThroughActive.cpp";
  const std::string birth =
      Ask("git log --diff-filter=A --format=%H -- " + self + " 2>/dev/null | tail -1");
  const bool born = birth.size() == 40;
  std::printf("NOTE the rule binds from %s\n", born ? birth.c_str() : "(uncommitted -- nothing yet in window)");
  CHECK(!Ask("git rev-parse --git-dir 2>/dev/null").empty(),
        "the walk can read the board's own history");

  std::vector<std::string> jumped;
  size_t closures = 0;
  if (born) {
    const std::string walk =
        Ask("git log --no-renames --diff-filter=A --name-only --format='@%H' " + birth +
            "..HEAD -- board/closed/ 2>/dev/null");
    std::string commit;
    for (const std::string &line : Lines(walk)) {
      if (line[0] == '@') {
        commit = line.substr(1);
        continue;
      }
      const size_t slash = line.rfind('/');
      if (slash == std::string::npos || commit.empty()) { continue; }
      const std::string name = line.substr(slash + 1);
      ++closures;
      const bool stood =
          !Ask("git cat-file -e " + commit + "^:board/active/" + name + " 2>/dev/null && echo y")
               .empty();
      std::printf("NOTE %.9s closed %s -- stood in board/active at its parent: %s\n",
                  commit.c_str(), name.c_str(), stood ? "yes" : "NO");
      if (!stood) {
        jumped.push_back(name + " was closed by " + commit.substr(0, 9) +
                         " straight out of board/open -- it never stood in board/active");
      }
    }
  }
  Note("closures inside the window", (double)closures, "items");

  for (const std::string &one : jumped) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(jumped.empty(),
        "**AN ITEM REACHES board/closed THROUGH board/active**: the state machine's middle "
        "state is how a second agent learns an item already has an owner, and a claim anchored "
        "to the closure cannot be satisfied by never opening the drawer -- which is how the "
        "freshness claim beside it stayed green through three violations (board:1793, 1783)");

  Covers("IV.16 every item that arrived under board/closed stood under board/active in the "
         "closing commit's parent tree, walked from this proof's own birth commit forward -- "
         "so the gate rewards using the state machine instead of rewarding abstention "
         "(board:1793)");
  return Report();
}
