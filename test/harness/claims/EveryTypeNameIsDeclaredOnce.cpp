#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

using outshine::Test::Ask;
using outshine::Test::Lines;

namespace {

// NO PRIMITIVE THIS TREE DECLARES ITSELF MAY BE DECLARED TWICE -- no second `Slip`, no second
// `Body`, no second `kMaxNodes`. A type or a constant is a word, and a word that means two things
// is a word nobody can read: `Rigging` used to copy `CorneringNPerRad` into `StiffnessNPerRad`
// field by field, which is one quantity wearing two names, and `Prismatic` stood byte-identical
// in the door and in the physics until the day this claim was written.
//
// Unreal declares each of these once and everything uses it: one `FVector`, one `FMatrix`, one
// `UE_PI`, one `FBodyInstance`. RAGE the same with `Vec3V`, `Mat34V` and one `PI`. Neither engine
// carries a private spelling of a type its own subsystems already have -- that is what a module's
// public header is FOR.
//
// THE COUNT IS DECLARED AND ANY MOVE IS REFUSED, which is the same shape as `--audit-access`. A
// number that may only fall silently falls back up when nobody is looking; a number that refuses
// on every move means each name that leaves the list leaves in a commit that says so, and each
// new one arrives the same way. Both directions are information.

constexpr size_t kTypeNamesTwice = 12;
constexpr size_t kConstantNamesTwice = 1;

[[nodiscard]] std::string Word(const std::string &from, size_t at) {
  while (at < from.size() && from[at] == ' ') { ++at; }
  size_t to = at;
  while (to < from.size() && (isalnum((unsigned char)from[to]) || from[to] == '_')) { ++to; }
  return from.substr(at, to - at);
}

[[nodiscard]] std::map<std::string, std::set<std::string>> Declared(const std::string &pattern,
                                                                    size_t skipWords) {
  std::map<std::string, std::set<std::string>> out;
  const std::vector<std::string> hits =
      Lines(Ask("grep -rnE '" + pattern + "' src/ include/ --include=*.h 2>/dev/null | grep -v ';$'"));
  for (const std::string &hit : hits) {
    const size_t colon = hit.find(':');
    if (colon == std::string::npos) { continue; }
    const std::string file = hit.substr(0, colon);
    const std::string body = hit.substr(hit.find(':', colon + 1) + 1);
    size_t at = 0;
    std::string named;
    for (size_t step = 0; step <= skipWords; ++step) {
      named = Word(body, at);
      at = body.find(named, at) + named.size();
    }
    if (named.empty()) { continue; }
    out[named].insert(file);
  }
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t types = 0, constants = 0;
  for (const auto &one : Declared("^(struct|class) [A-Za-z_]", 1)) {
    if (one.second.size() < 2) { continue; }
    ++types;
    std::printf("TWICE  %-22s %zu file(s)\n", one.first.c_str(), one.second.size());
  }
  std::map<std::string, std::set<std::string>> named;
  for (const std::string &hit :
       Lines(Ask("grep -rnE 'constexpr [a-z0-9_:]+ k[A-Za-z0-9_]+ =' src/ include/ "
                 "--include=*.h 2>/dev/null"))) {
    const size_t colon = hit.find(':');
    if (colon == std::string::npos) { continue; }
    const std::string body = hit.substr(hit.find(':', colon + 1) + 1);
    const size_t equals = body.find(" =");
    if (equals == std::string::npos) { continue; }
    size_t from = equals;
    while (from > 0 && (isalnum((unsigned char)body[from - 1]) || body[from - 1] == '_')) { --from; }
    named[body.substr(from, equals - from)].insert(hit.substr(0, colon));
  }
  for (const auto &one : named) {
    if (one.second.size() < 2) { continue; }
    ++constants;
    std::printf("TWICE  %-22s %zu file(s)\n", one.first.c_str(), one.second.size());
  }

  Note("type names declared in more than one file", (double)types, "names");
  Note("constant names declared in more than one file", (double)constants, "names");

  CHECK(types == kTypeNamesTwice,
        "**A TYPE NAME IS DECLARED ONCE**, and the count of those that are not is DECLARED here so "
        "that it moves only in a commit that says why. A word meaning two things is a word nobody "
        "can read, and both engines keep exactly one of each");
  CHECK(constants == kConstantNamesTwice,
        "and a named constant the same: one definition, or the two spellings drift and the day "
        "they disagree nothing says which was meant");

  Covers("the tree: every type and every named constant it declares itself has ONE definition, "
         "and the count of those that do not is declared rather than discovered");
  return Report();
}
