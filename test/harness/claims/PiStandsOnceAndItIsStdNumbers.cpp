#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "Check.h"

namespace {

bool Slurp(const std::filesystem::path &path, std::string &into) {
  std::FILE *file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

// The MSL raw strings keep their literals (board:1634/1580 own that seam): a span between
// R"( and its )" is the device's text, not this tree's C++.
std::vector<std::pair<size_t, size_t>> RawSpans(const std::string &text) {
  std::vector<std::pair<size_t, size_t>> spans;
  for (size_t at = text.find("R\"("); at != std::string::npos; at = text.find("R\"(", at)) {
    size_t end = text.find(")\"", at + 3);
    if (end == std::string::npos) { end = text.size(); }
    spans.push_back({at, end});
    at = end;
  }
  return spans;
}

bool Inside(size_t at, const std::vector<std::pair<size_t, size_t>> &spans) {
  for (const auto &span : spans) {
    if (at >= span.first && at < span.second) { return true; }
  }
  return false;
}

const char *kDigits[] = {"3.1415", "6.2831", "1.5707", "0.01745", "57.295", "0.7853", "2.3999"};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  size_t files = 0;
  size_t mslSites = 0;
  std::vector<std::string> spelt;
  std::vector<std::string> macro;
  std::vector<std::string> alias;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (!entry.is_regular_file()) { continue; }
    const std::string ext = entry.path().extension().string();
    if (ext != ".h" && ext != ".cpp" && ext != ".msl") { continue; }
    std::string text;
    CHECK(Slurp(entry.path(), text), "a source the walk found can be read");
    ++files;
    const auto spans = RawSpans(text);
    for (const char *needle : kDigits) {
      for (size_t at = text.find(needle); at != std::string::npos;
           at = text.find(needle, at + 1)) {
        if (Inside(at, spans)) { ++mslSites; continue; }
        spelt.push_back(entry.path().string() + " spells " + needle);
      }
    }
    for (size_t at = text.find("M_PI"); at != std::string::npos; at = text.find("M_PI", at + 1)) {
      const char before = at == 0 ? ' ' : text[at - 1];
      if (std::isalnum((unsigned char)before) || before == '_') { continue; }
      macro.push_back(entry.path().string());
      break;
    }
    for (size_t at = text.find("double kPi ="); at != std::string::npos;
         at = text.find("double kPi =", at + 1)) {
      alias.push_back(entry.path().string());
    }
  }

  Note("sources walked", (double)files, "files");
  Note("pi digits inside MSL raw strings", (double)mslSites, "sites");

  for (const std::string &one : spelt) { std::printf("NOTE %s\n", one.c_str()); }
  CHECK(spelt.empty(),
        "**NO PI STANDS AS DIGITS OUTSIDE AN MSL RAW STRING.** Every pi in src/, the .msl "
        "shader files included (board:1651) -- tau, half-pi, "
        "deg2rad, rad2deg, quarter-pi, the golden angle -- derives from std::numbers; a digit "
        "spelling is a second origin for the one constant and it drifts (board:1630)");

  for (const std::string &one : macro) { std::printf("NOTE %s\n", one.c_str()); }
  CHECK(macro.empty(),
        "**AND NO SOURCE SPELLS THE POSIX MACRO**: M_PI is not C++ and not this tree's origin "
        "for pi (board:1630)");

  CHECK(alias.size() == 1 && alias[0] == std::filesystem::path("src/core/Units.h").string(),
        "the kPi alias stands ONCE, in src/core/Units.h -- a second alias is the duplicate this "
        "item removed from TileMath.h");

  Covers("pi stands once and it is std::numbers: every pi in src/ derives from "
         "std::numbers::pi, the alias lives in one header, and only the MSL raw strings keep "
         "their literals (board:1630)");
  return Report();
}
