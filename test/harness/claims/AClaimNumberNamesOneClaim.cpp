#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Check.h"

namespace {

[[nodiscard]] std::string Slurp(const std::filesystem::path &path) {
  std::string out;
  std::FILE *const file = std::fopen(path.string().c_str(), "rb");
  if (file == nullptr) { return out; }
  char block[8192];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { out.append(block, read); }
  std::fclose(file);
  return out;
}

// A Covers( string is written as adjacent literals across several lines. What the claim is
// about is the whole of it, so the reader joins them the way the compiler does.
[[nodiscard]] std::string Joined(const std::string &text, size_t from) {
  std::string said;
  size_t at = from;
  while (at < text.size()) {
    if (text[at] == '\\' && at + 1 < text.size()) {
      said += text[at + 1];
      at += 2;
      continue;
    }
    if (text[at] == '"') {
      size_t next = at + 1;
      while (next < text.size() && (text[next] == ' ' || text[next] == '\t' || text[next] == '\n')) {
        ++next;
      }
      if (next < text.size() && text[next] == '"') {
        at = next + 1;
        continue;
      }
      break;
    }
    said += text[at];
    ++at;
  }
  return said;
}

[[nodiscard]] std::string Tidied(const std::string &said) {
  std::string out;
  bool space = false;
  for (const char one : said) {
    if (one == ' ' || one == '\t' || one == '\n') {
      space = true;
      continue;
    }
    if (space && !out.empty()) { out += ' '; }
    space = false;
    out += one;
  }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::map<std::string, std::set<std::string>> saying;
  std::map<std::string, std::set<std::string>> proving;
  size_t walked = 0, covers = 0;

  for (const auto &entry : std::filesystem::recursive_directory_iterator("test")) {
    if (!entry.is_regular_file() || entry.path().extension() != ".cpp") { continue; }
    ++walked;
    const std::string text = Slurp(entry.path());
    for (size_t at = text.find("Covers(\""); at != std::string::npos;
         at = text.find("Covers(\"", at + 1)) {
      const std::string whole = Tidied(Joined(text, at + 8));
      const size_t space = whole.find(' ');
      if (space == std::string::npos) { continue; }
      const std::string number = whole.substr(0, space);
      if (number.empty() || number.find('.') == std::string::npos) { continue; }
      ++covers;
      saying[number].insert(whole.substr(space + 1, 60));
      proving[number].insert(entry.path().filename().string());
    }
  }

  std::vector<std::string> colliding;
  for (const auto &[number, sentences] : saying) {
    if (sentences.size() < 2) { continue; }
    colliding.push_back(number + " carries " + std::to_string(sentences.size()) +
                        " different sentences over " + std::to_string(proving[number].size()) +
                        " cases");
  }
  std::sort(colliding.begin(), colliding.end());

  Note("sources walked", (double)walked, "files");
  Note("Covers statements found", (double)covers, "claims");
  Note("distinct claim numbers", (double)saying.size(), "numbers");
  Note("numbers carrying more than one sentence", (double)colliding.size(), "numbers");
  for (const std::string &one : colliding) { std::printf("FOUND %s\n", one.c_str()); }

  CHECK(covers > 200, "the walk found this tree's claims, not a corner of them");
  CHECK(colliding.empty(),
        "**A CLAIM NUMBER NAMES ONE CLAIM**: the number is how a scorer, a trailer and a reader "
        "tie a case to what it proves, and CLAUDE.md's rule that every number carries its "
        "origin holds one level up -- a claim number's origin is the claim it names. Two cases "
        "may share a number when they prove the SAME sentence; two different sentences under "
        "one number mean the tree cannot be asked what that number proves (board:1841)");

  Covers("IV.25 a claim number names one claim: two cases may share a number only when they "
         "carry the same sentence, so the number stays an identity a reader can resolve "
         "(board:1841)");
  return Report();
}
