#pragma once

#include <string>

#include "Shell.h"

namespace outshine::Test::Board {

// The board is one flat directory and an item's State is an attribute of the item, so asking
// what state it was in at a commit is a question about the FILE's content there, not about
// which drawer held it.
[[nodiscard]] inline std::string PathAt(const std::string &commit, unsigned item) {
  char number[8] = {};
  std::snprintf(number, sizeof number, "%04u_", item);
  return Ask("git ls-tree --name-only " + commit + " board/ 2>/dev/null | grep '/" +
             std::string(number) + "'");
}

[[nodiscard]] inline std::string StateAt(const std::string &commit, unsigned item) {
  const std::string path = PathAt(commit, item);
  if (path.empty()) { return std::string(); }
  const std::string said =
      Ask("git show " + commit + ":" + path + " 2>/dev/null | sed -n 's/^State: //p' | head -1");
  return said;
}

[[nodiscard]] inline bool StoodIn(const std::string &commit, unsigned item, const char *state) {
  return StateAt(commit, item) == state;
}

}
