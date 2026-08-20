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

const char *const kPictureLayers[] = {"src/core",       "src/corridor", "src/data",
                                      "src/generators", "src/gltf",     "src/render",
                                      "src/scenario",   "src/ui",       "src/world",
                                      "src/clients"};

struct Excused {
  const char *Named;
  const char *Why;
};

const Excused kExcused[] = {
    {"", "src/clients/Env.h is the host boundary that reads the environment, and a boundary that "
         "reads it is what lets nothing else"},
    {"FB_DAGLOG", "logging, which changes what is printed and never what is drawn"},
    {"FB_TILEWORKERS",
     "a PACE and not a picture -- how many threads stream tiles. It is excused only while the "
     "streamer's result is worker-count independent, which is a claim board:1513 owes a test for, "
     "and the default beneath it is ALREADY machine-dependent: hardware_concurrency minus two"},
};

bool Excuse(const std::string &path, const std::string &named, std::string &why) {
  if (path == "src/clients/Env.h") {
    why = kExcused[0].Why;
    return true;
  }
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
  for (const char *const layer : kPictureLayers) {
    if (!std::filesystem::exists(layer)) { continue; }
    for (const auto &entry : std::filesystem::recursive_directory_iterator(layer)) {
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
          if (Excuse(path, named, why)) {
            std::printf("NOTE %s:%d reads '%s' -- %s\n", path.c_str(), at, named.c_str(),
                        why.c_str());
          } else {
            found.push_back(Reader{path, at, named});
          }
        }
        line.clear();
      }
      std::fclose(file);
    }
  }

  Note("files of the picture path read", (double)files, "files");
  for (const Reader &one : found) {
    std::printf("FOUND %s:%d reads the environment for '%s', with no reason declared\n",
                one.File.c_str(), one.Line, one.Named.c_str());
  }
  Note("undeclared environment readers", (double)found.size(), "sites");

  CHECK(found.empty(),
        "**NO LAYER THAT DECIDES A PICTURE READS THE ENVIRONMENT.** The picture is a function of the "
        "declaration and not of the machine, so a value that steers geometry, shading or streaming "
        "arrives through a declaration a reader can see -- an engine default, a scenario, or a "
        "consumer's override, each published with the measurement. getenv is ambient: read once, "
        "appearing in no output, and a shell that set it three days ago changes today's number with "
        "nothing to grep for");

  Covers("I.17 the picture is a function of the declaration and not of the machine: no layer of the "
         "engine that decides one reads an environment variable, and the host boundary that may is "
         "named");
  return Report();
}
