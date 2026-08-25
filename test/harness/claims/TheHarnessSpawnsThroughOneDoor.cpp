#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

namespace {

constexpr std::string_view kDoor = "Shell.h";

// The needle is assembled rather than written, because a file that spells the thing it forbids
// would find itself -- and the exemption a self-finding walk needs ("skip my own path") is the
// construction board:1799 refuses: a rule that stops applying to its own author.
[[nodiscard]] std::string TheSpawn() { return std::string("po") + "pen("; }

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::ifstream reading(path, std::ios::binary);
  std::ostringstream all;
  all << reading.rdbuf();
  return all.str();
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<std::string> spelling;
  size_t walked = 0;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("test")) {
    if (!entry.is_regular_file()) { continue; }
    const std::string extension = entry.path().extension().string();
    if (extension != ".cpp" && extension != ".h") { continue; }
    ++walked;
    if (entry.path().filename().string() == kDoor) { continue; }
    const std::string text = Slurp(entry.path());
    if (text.find(TheSpawn()) == std::string::npos) { continue; }
    spelling.push_back(entry.path().string());
  }

  Note("harness sources walked", (double)walked, "files");
  Note("sources spelling popen outside the door", (double)spelling.size(), "files");
  for (const std::string &one : spelling) { std::printf("FOUND %s spawns a process itself\n", one.c_str()); }

  CHECK(walked > 0, "the walk reached the tree it judges");
  CHECK(spelling.empty(),
        "**THE HARNESS SPAWNS THROUGH ONE DOOR**: a process the harness starts is a boundary, "
        "and a boundary written nine times is nine contracts -- board:1854 measured 512 and 4096 "
        "byte blocks, answers trimmed and untrimmed, verdicts kept and dropped, all under two "
        "verb names. test/harness/shared/Shell.h is the door; a case that spawns a process itself "
        "has forked the contract its neighbours read");

  Covers("IV.32 the harness spawns processes through test/harness/shared/Shell.h and nowhere "
         "else: one Run, one Ask, one Lines, so a case cannot fork the contract by writing its "
         "own (board:1854)");
  return Report();
}
