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
#include "Shell.h"

#ifndef OUTSHINE_COMPILE
#error                                                                                             \
    "OUTSHINE_COMPILE is the harness's compile command for this layer; a subject cannot be judged without it"
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
    if (line.rfind(refused, 0) == 0) { return {Expectation::Refused, line.substr(refused.size())}; }
    if (line.rfind("// ACCEPTED", 0) == 0) { return {Expectation::Accepted, {}}; }
  }
  return declared;
}

struct CompilerAnswer {
  bool Refused = false;
  std::string Diagnostics;
};

inline CompilerAnswer Compile(const std::filesystem::path &subject) {
  const std::string command =
      std::string(OUTSHINE_COMPILE) + " -fsyntax-only '" + subject.string() + "' 2>&1";
  CompilerAnswer answer;
  answer.Refused = Run(command, answer.Diagnostics) != 0;
  return answer;
}

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
      if (answer.Refused) {
        std::printf("       %s\n%s", subject.c_str(), answer.Diagnostics.c_str());
      }
      continue;
    }
    CHECK(answer.Refused, "a subject declared REFUSED does not compile");
    const bool forTheStatedReason =
        answer.Diagnostics.find(declared.Diagnostic) != std::string::npos;
    CHECK(forTheStatedReason, "a refused subject is refused for the reason it declares");
    if (!answer.Refused || !forTheStatedReason) {
      std::printf("       %s declares REFUSED: %s\n%s",
                  subject.c_str(),
                  declared.Diagnostic.c_str(),
                  answer.Diagnostics.c_str());
    }
  }
  Note("compile subjects judged", static_cast<double>(subjects.size()), "files");
}

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
