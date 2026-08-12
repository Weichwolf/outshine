/* WHAT THE TYPE SYSTEM AND THE INCLUDE SET MUST REFUSE, with a verdict the harness can count.
 *
 * THE POSITIVE DIRECTION IS NOT HERE AND MUST NOT BE. Every test under test/unit/<layer>/ is already
 * compiled with that layer's include set, so an illegal include is a build failure -- continuous,
 * free, and what "layering is the build, never a checker" means. The negative direction cannot be
 * had that way: proving a name has NO spelling means invoking the compiler and asserting it refuses.
 *
 * THE SUBJECT DECLARES ITS OWN REASON AND THE TEST ASSERTS IT. A gate that accepts any non-zero exit
 * is the vacuous shape wearing a compiler: a typo, a moved header or a changed flag also fails, and
 * the gate reports green over a proof it no longer carries. A subject carries one line --
 * `// REFUSED: <text>` or `// ACCEPTED` -- and a subject with neither fails the test by name.
 *
 * EACH REFUSAL CARRIES ITS OWN POSITIVE CONTROL, in the subject, as the include above the forbidden
 * one: if the layer's set stopped resolving, the control's header would be the missing one and the
 * declared diagnostic would not match. That is what keeps a refusal from passing for the wrong
 * reason without a second test asserting that a legal include is legal.
 *
 * OUTSHINE_COMPILE COMES FROM THE HARNESS -- the same compiler, the same house warning set and the
 * same include set this test itself was built with. Written down here instead, it would be a second
 * copy of the layering that drifts the first time one side moves. */
#ifndef LAYERING_H
#define LAYERING_H

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"

#ifndef OUTSHINE_COMPILE
#error "OUTSHINE_COMPILE is the harness's compile command for this layer; a subject cannot be judged without it"
#endif

namespace outshine::Test {

enum class Expectation { Accepted, Refused, Undeclared };

struct SubjectExpectation {
  Expectation What = Expectation::Undeclared;
  std::string Diagnostic;
};

inline SubjectExpectation DeclaredExpectation(const std::filesystem::path &subject) {
  std::ifstream source(subject);
  std::string line;
  SubjectExpectation declared;
  while (std::getline(source, line)) {
    const std::string refused = "// REFUSED: ";
    if (line.rfind(refused, 0) == 0) {
      return {Expectation::Refused, line.substr(refused.size())};
    }
    if (line.rfind("// ACCEPTED", 0) == 0) { return {Expectation::Accepted, {}}; }
  }
  return declared;
}

struct CompilerAnswer {
  bool Refused = false;
  std::string Diagnostics;
};

/* popen, and it is the only process this repository starts from C++. The standard library has no way
 * to run a compiler and read what it said, and what it said is the whole point of the test. */
inline CompilerAnswer Compile(const std::filesystem::path &subject) {
  const std::string command =
      std::string(OUTSHINE_COMPILE) + " -fsyntax-only '" + subject.string() + "' 2>&1";
  CompilerAnswer answer;
  std::FILE *const compiler = ::popen(command.c_str(), "r");
  if (compiler == nullptr) { return {false, "the compiler could not be started"}; }
  char chunk[4096];
  while (std::fgets(chunk, sizeof chunk, compiler) != nullptr) { answer.Diagnostics += chunk; }
  answer.Refused = ::pclose(compiler) != 0;
  return answer;
}

/* Every .cpp under `directory` is a subject and every subject is judged. A directory that holds none
 * fails: a gate over nothing is the shape this whole file exists to refuse. */
inline void EveryCompileSubjectHolds(const char *directory) {
  std::vector<std::filesystem::path> subjects;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    if (entry.path().extension() == ".cpp") { subjects.push_back(entry.path()); }
  }
  std::sort(subjects.begin(), subjects.end());
  CHECK(!subjects.empty(), "the subject directory holds subjects to judge");

  for (const auto &subject : subjects) {
    const SubjectExpectation declared = DeclaredExpectation(subject);
    if (declared.What == Expectation::Undeclared) {
      CHECK(false, "every compile subject declares whether it is accepted or refused, and why");
      std::printf("       %s carries neither // ACCEPTED nor // REFUSED:\n", subject.c_str());
      continue;
    }
    const CompilerAnswer answer = Compile(subject);
    if (declared.What == Expectation::Accepted) {
      CHECK(!answer.Refused, "a subject declared ACCEPTED compiles under the house warning set");
      if (answer.Refused) { std::printf("       %s\n%s", subject.c_str(), answer.Diagnostics.c_str()); }
      continue;
    }
    CHECK(answer.Refused, "a subject declared REFUSED does not compile");
    const bool forTheStatedReason =
        answer.Diagnostics.find(declared.Diagnostic) != std::string::npos;
    CHECK(forTheStatedReason, "a refused subject is refused for the reason it declares");
    if (!answer.Refused || !forTheStatedReason) {
      std::printf("       %s declares REFUSED: %s\n%s", subject.c_str(),
                  declared.Diagnostic.c_str(), answer.Diagnostics.c_str());
    }
  }
  Note("compile subjects judged", static_cast<double>(subjects.size()), "files");
}

/* THE ONE ESCAPE AN INCLUDE SET CANNOT CLOSE. `#include "../render/Renderer.h"` resolves relative to
 * the including file and does not consult -I at all, so a layer bounded by its include set is not
 * bounded against it. This is the one thing the deleted `-MM` closure loops caught that the build
 * does not, and it is a checker rather than a compile error -- said plainly, because the difference
 * is what decides how much it is worth. */
inline void NoIncludeClimbsOutOfItsDirectory(const char *directory) {
  int examined = 0;
  for (const auto &entry : std::filesystem::directory_iterator(directory)) {
    const std::string extension = entry.path().extension().string();
    if (extension != ".cpp" && extension != ".h") { continue; }
    ++examined;
    std::ifstream source(entry.path());
    std::string line;
    while (std::getline(source, line)) {
      const std::size_t quote = line.find("#include \"");
      if (quote == std::string::npos) { continue; }
      const std::string named = line.substr(quote + 10);
      const bool climbs = named.rfind("../", 0) == 0 || named.rfind('/', 0) == 0;
      if (climbs) {
        CHECK(false, "a quoted include names a file, never a path out of its own directory");
        std::printf("       %s: %s\n", entry.path().c_str(), line.c_str());
      }
    }
  }
  CHECK(examined > 0, "the directory whose includes are judged holds sources");
  Note("sources read for a climbing include", static_cast<double>(examined), "files");
}

} // namespace outshine::Test

#endif
