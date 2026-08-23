#include <cstdio>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::FILE *file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string into;
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return into;
}

[[nodiscard]] std::filesystem::path Resolve(const std::string &name) {
  for (const auto &entry : std::filesystem::recursive_directory_iterator("src")) {
    if (entry.is_regular_file() && entry.path().filename() == name) { return entry.path(); }
  }
  return {};
}

[[nodiscard]] std::vector<std::string> Lines(const std::filesystem::path &path) {
  std::vector<std::string> lines;
  std::string held = Slurp(path);
  size_t from = 0;
  for (size_t at = held.find('\n'); at != std::string::npos; at = held.find('\n', from)) {
    lines.push_back(held.substr(from, at - from));
    from = at + 1;
  }
  lines.push_back(held.substr(from));
  return lines;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // a colour on the map is only worth what its stated reason is worth: board:1762 found
  // TilePool red for "spells camera and LOD" when it spells neither, and a justification
  // that has gone stale teaches the reader to discount the colours that still hold.
  const std::string document = Slurp("CLAUDE.md");
  CHECK(!document.empty(), "the map is where the colours are adjudicated");

  const std::regex cited(R"(`([^`]+)`\s*\((?:([A-Za-z0-9_]+\.(?:h|cpp)))?:(\d+)(?:-(\d+))?\))");
  std::string carried;
  size_t citations = 0;
  std::vector<std::string> stale;

  for (auto found = std::sregex_iterator(document.begin(), document.end(), cited);
       found != std::sregex_iterator(); ++found) {
    const std::smatch &one = *found;
    const std::string symbol = one[1].str();
    if (!one[2].str().empty()) { carried = one[2].str(); }
    if (carried.empty()) { continue; }
    ++citations;

    const std::filesystem::path source = Resolve(carried);
    if (source.empty()) {
      stale.push_back(carried + " is cited and no file of that name stands in src/");
      continue;
    }
    const std::vector<std::string> lines = Lines(source);
    const size_t first = (size_t)std::stoul(one[3].str());
    const size_t last = one[4].str().empty() ? first : (size_t)std::stoul(one[4].str());
    if (last == 0 || last > lines.size()) {
      stale.push_back(carried + ":" + one[3].str() + " is past the end of a " +
                      std::to_string(lines.size()) + "-line file");
      continue;
    }
    bool says = false;
    for (size_t at = first; at <= last && !says; ++at) {
      says = lines[at - 1].find(symbol) != std::string::npos;
    }
    if (!says) {
      stale.push_back(carried + ":" + one[3].str() + " is cited for `" + symbol +
                      "', and that line reads: " + lines[first - 1]);
    }
  }

  Note("citations judged", (double)citations, "citations");
  for (const std::string &one : stale) { std::printf("FOUND %s\n", one.c_str()); }
  CHECK(citations >= 4, "the map cites the code it judges, and this walk found the citations");
  CHECK(stale.empty(),
        "**EVERY COLOUR CITES A LINE THAT SAYS IT**: a red node names what makes it red at "
        "file:line, and that line still spells it -- a justification that has drifted is "
        "itself a finding (board:1762)");

  Covers("IV.12 every file:line a document cites resolves to a line that carries the symbol "
         "cited, so a stale justification cannot outlive the code it judges (board:1762)");
  return Report();
}
