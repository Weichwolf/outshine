#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

struct Reader {
  std::string File;
  int Line;
  std::string Named;
};

struct Excused {
  const char *Named;
  const char *Why;
};

const Excused kExcused[] = {
    {"OUTSHINE_DAGLOG", "logging, which changes what is printed and never what is drawn"},
};

bool Excuse(const std::string &named, std::string &why) {
  for (const Excused &one : kExcused) {
    if (named == one.Named && named[0] != 0) {
      why = one.Why;
      return true;
    }
  }
  return false;
}

std::string Quoted(const std::string &line, size_t from) {
  const size_t open = line.find('"', from);
  if (open == std::string::npos) { return std::string(); }
  const size_t close = line.find('"', open + 1);
  if (close == std::string::npos) { return std::string(); }
  return line.substr(open + 1, close - open - 1);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::vector<Reader> found;
  size_t files = 0;
  size_t layers = 0;
  {
    for (const auto &layer : std::filesystem::directory_iterator("src")) {
      if (layer.is_directory()) { ++layers; }
    }
    for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
      if (!entry.is_regular_file()) { continue; }
      const std::string path = entry.path().string();
      const std::string suffix = entry.path().extension().string();
      if (suffix != ".cpp" && suffix != ".h") { continue; }
      ++files;
      std::FILE *const file = std::fopen(path.c_str(), "rb");
      if (file == nullptr) { continue; }
      std::string line;
      int at = 0;
      int c = 0;
      while ((c = std::fgetc(file)) != EOF) {
        if (c != '\n') {
          line.push_back((char)c);
          continue;
        }
        ++at;
        const size_t reads = line.find("getenv");
        if (reads != std::string::npos) {
          const std::string named = Quoted(line, reads);
          std::string why;
          if (Excuse(named, why)) {
            std::printf(
                "NOTE %s:%d reads '%s' -- %s\n", path.c_str(), at, named.c_str(), why.c_str());
          } else {
            found.push_back(Reader{path, at, named});
          }
        }
        line.clear();
      }
      std::fclose(file);
    }
  }

  Note("layers of the library walked", (double)layers, "directories");
  Note("files of the picture path read", (double)files, "files");
  CHECK(layers >= 10 && files >= 200,
        "**AND THE LAYERS ARE WALKED AND NEVER LISTED.** A list of directories in a checker is a "
        "second index over the tree: it went green once here while omitting src/world, which is "
        "exactly where the two surviving readers live. What passes on its first run is what to "
        "distrust");
  for (const Reader &one : found) {
    std::printf("FOUND %s:%d reads the environment for '%s', with no reason declared\n",
                one.File.c_str(),
                one.Line,
                one.Named.c_str());
  }
  Note("undeclared environment readers", (double)found.size(), "sites");

  CHECK(
      found.empty(),
      "**NO LAYER THAT DECIDES A PICTURE READS THE ENVIRONMENT.** The picture is a function of the "
      "declaration and not of the machine, so a value that steers geometry, shading or streaming "
      "arrives through a declaration a reader can see -- an engine default, a scenario, or a "
      "consumer's override, each published with the measurement. getenv is ambient: read once, "
      "appearing in no output, and a shell that set it three days ago changes today's number with "
      "nothing to grep for");

  Covers(
      "I.17 the picture is a function of the declaration and not of the machine: no layer of the "
      "engine that decides one reads an environment variable, and the host boundary that may is "
      "named");
  return Report();
}
