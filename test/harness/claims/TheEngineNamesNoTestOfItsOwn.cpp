#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

namespace {

const char *const kLibrary = "src";

}

int main() {
  using namespace outshine::Test;

  std::vector<std::string> citations;
  size_t files = 0, bytes = 0;

  std::error_code failed;
  const bool present = std::filesystem::is_directory(kLibrary, failed);
  CHECK(present,
        "the library is a directory in this repository, so what follows is a measurement "
        "over a population rather than a statement about an empty one");

  if (present) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(kLibrary, failed)) {
      if (!entry.is_regular_file()) { continue; }
      ++files;
      std::ifstream source(entry.path(), std::ios::binary);
      std::string line;
      size_t number = 0;
      while (std::getline(source, line)) {
        ++number;
        bytes += line.size();

        for (size_t at = line.find("test/"); at != std::string::npos;
             at = line.find("test/", at + 1)) {
          if (at > 0 && (std::isalnum((unsigned char)line[at - 1]) || line[at - 1] == '_')) {
            continue;
          }
          citations.push_back(entry.path().string() + ":" + std::to_string(number));
          break;
        }
      }
    }
  }

  for (const std::string &site : citations) {
    std::printf("NOTE cites a test: %s\n", site.c_str());
  }

  Note("files in the library", (double)files, "files");
  Note("bytes read", (double)bytes, "bytes");
  Note("sites naming a path under test/", (double)citations.size(), "sites");

  CHECK(files > 0, "the library holds files at all");
  CHECK(
      citations.empty(),
      "no file in the library names a path under test/ -- the engine declares what it needs from a "
      "host and everything that runs it is a test, so the citation goes the other way and travels "
      "in the work item that proves it, never here");

  Covers(
      "I.56 the engine is a library: it does not name what runs it, and a claim about it is "
      "proven by a test that cites the requirement rather than by a comment that cites the test");
  return Report();
}
