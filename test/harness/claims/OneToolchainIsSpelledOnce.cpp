#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::string &path) {
  std::ifstream reading(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(reading)),
                     std::istreambuf_iterator<char>());
}

// every -Wsomething and -std=something in a chunk of text, in the order it stands.
[[nodiscard]] std::vector<std::string> Flags(const std::string &text) {
  std::vector<std::string> found;
  for (size_t at = text.find('-'); at != std::string::npos; at = text.find('-', at + 1)) {
    const bool warning = text.compare(at, 2, "-W") == 0;
    const bool standard = text.compare(at, 5, "-std=") == 0;
    if (!warning && !standard) { continue; }
    size_t ends = at;
    while (ends < text.size() && text[ends] != '"' && text[ends] != ' ' && text[ends] != '\n' &&
           text[ends] != ',' && text[ends] != '\'') {
      ++ends;
    }
    found.push_back(text.substr(at, ends - at));
    at = ends;
  }
  std::sort(found.begin(), found.end());
  found.erase(std::unique(found.begin(), found.end()), found.end());
  return found;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1786: the corpus preparer compiles a generator against the library with its own
  // hard-written toolchain, and it stood at -std=c++17 in a C++23 tree until a repair to
  // something else happened to touch it. The two spellings must agree or the corpus is built by
  // a compiler the gate never uses -- and nothing said so.
  const std::string runner = Slurp("test/run.sh");
  const std::string grower = Slurp("test/harness/render/outshine/grown/prepare/grown.py");
  CHECK(!runner.empty() && !grower.empty(), "both spellings of the toolchain are readable");

  const size_t stdAt = runner.find("CXXSTD=");
  const size_t warnAt = runner.find("WARN=");
  CHECK(stdAt != std::string::npos && warnAt != std::string::npos,
        "the runner declares a standard and a warning set");
  const std::vector<std::string> runnerFlags =
      Flags(runner.substr(stdAt, runner.find('\n', stdAt) - stdAt) + " " +
            runner.substr(warnAt, runner.find('\n', warnAt) - warnAt));

  const size_t commandAt = grower.find("command = [compiler");
  CHECK(commandAt != std::string::npos, "the preparer declares a compile command");
  const std::vector<std::string> growerFlags =
      Flags(grower.substr(commandAt, grower.find("command +=", commandAt) - commandAt));

  for (const std::string &one : runnerFlags) { std::printf("NOTE the runner: %s\n", one.c_str()); }
  for (const std::string &one : growerFlags) { std::printf("NOTE the preparer: %s\n", one.c_str()); }
  Note("flags the runner declares", (double)runnerFlags.size(), "flags");
  Note("flags the preparer declares", (double)growerFlags.size(), "flags");

  std::vector<std::string> apart;
  for (const std::string &one : growerFlags) {
    if (std::find(runnerFlags.begin(), runnerFlags.end(), one) == runnerFlags.end()) {
      apart.push_back("the preparer spells " + one + " and the runner does not");
    }
  }
  for (const std::string &one : runnerFlags) {
    if (one.compare(0, 5, "-std=") != 0) { continue; }
    if (std::find(growerFlags.begin(), growerFlags.end(), one) == growerFlags.end()) {
      apart.push_back("the runner spells " + one + " and the preparer does not");
    }
  }
  for (const std::string &one : apart) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(apart.empty(),
        "**ONE TOOLCHAIN IS SPELLED ONCE**: the corpus preparer compiles a generator against "
        "the same library the gate builds, so a standard or a warning it spells differently "
        "means the corpus is made by a compiler the gate never uses -- and the preparer stood "
        "at -std=c++17 in a C++23 tree until a repair to something else happened to look "
        "(board:1786)");

  Covers("IV.20 the corpus preparer's toolchain agrees with the runner's: its standard is the "
         "runner's standard and every warning it spells is one the runner spells too "
         "(board:1786)");
  return Report();
}
