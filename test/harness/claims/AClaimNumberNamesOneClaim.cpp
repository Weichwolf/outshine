#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Check.h"
#include "Shell.h"

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

  // board:1844: the floor this replaced was `covers > 200` -- a bare number under a count the
  // walk prints one line earlier, satisfied by 201 of 209 and meaningless the day a case is
  // deleted. git is the independent witness: it knows every versioned source, the walk knows
  // every source it opened, and a walk that read a corner of the tree is exactly the walk whose
  // two counts disagree.
  const size_t versioned =
      outshine::Test::Lines(outshine::Test::Ask("git ls-files 'test/*.cpp' 'test/**/*.cpp'"))
          .size();

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
      // board:1844: comparing a 60-character prefix let IV.15 carry three different sentences
      // that all begin the same way -- the walk printed 0 collisions over the only live one in
      // the tree. A claim is its WHOLE sentence.
      saying[number].insert(whole.substr(space + 1));
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

  Note("sources git carries under test/", (double)versioned, "files");
  CHECK(walked == versioned,
        "**AND THE WALK READ THE TREE, NOT A CORNER OF IT**: this case judges what it opened, so "
        "a wrong working directory, a filter that lost a directory or an unversioned source "
        "smuggled in beside the cases would leave it green over whatever it happened to see. "
        "git's index is the witness that does not come from the walk (board:1844)");
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
