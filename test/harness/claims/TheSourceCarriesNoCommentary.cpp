#include <cstdio>
#include <filesystem>
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

// a "//" inside a string literal is data, not prose -- Script.cpp and Style.cpp both hold
// the two characters as the thing they parse, so the walk reads literals before it judges
// board:1801, reopened: the exemption was a property of the file's BYTES, and a comment can
// spell any needle chosen for it -- `// this narration mentions Covers("IV.11") and therefore
// exempts its own file` in src/core/Span.h turned this claim green, measured. Tightening the
// needle again is the same construction; there is no string a comment cannot legally contain.
//
// The exemption is a property of the file's ROLE, and the runner owns that role: a proof is a
// translation unit `test/run.sh` builds and RUNS as a case. `run.sh --cases` prints that list
// before it builds anything, so asking costs a fifth of a second and cannot be spelled by the
// text it governs.
[[nodiscard]] std::vector<std::string> TheRunnersOwnCases() {
  std::vector<std::string> cases;
  std::FILE *const pipe = popen("sh test/run.sh --cases 2>/dev/null", "r");
  if (pipe == nullptr) { return cases; }
  char line[1024];
  while (std::fgets(line, sizeof line, pipe) != nullptr) {
    std::string one(line);
    while (!one.empty() && (one.back() == '\n' || one.back() == ' ')) { one.pop_back(); }
    if (!one.empty()) { cases.push_back(one); }
  }
  pclose(pipe);
  return cases;
}

[[nodiscard]] bool IsAProof(const std::vector<std::string> &cases, const std::string &path) {
  for (const std::string &one : cases) {
    if (one == path) { return true; }
  }
  return false;
}

[[nodiscard]] int CommentLine(const std::string &text) {
  int line = 1;
  size_t at = 0;
  while (at < text.size()) {
    const char c = text[at];
    if (c == '\n') {
      ++line;
      ++at;
      continue;
    }
    if (c == 'R' && at + 1 < text.size() && text[at + 1] == '"') {
      const size_t open = text.find('(', at + 2);
      if (open != std::string::npos) {
        const std::string tag = text.substr(at + 2, open - (at + 2));
        const std::string close = ")" + tag + "\"";
        const size_t end = text.find(close, open);
        const size_t stop = end == std::string::npos ? text.size() : end + close.size();
        for (size_t scan = at; scan < stop; ++scan) {
          if (text[scan] == '\n') { ++line; }
        }
        at = stop;
        continue;
      }
    }
    if (c == '"' || c == '\'') {
      const char quote = c;
      ++at;
      while (at < text.size() && text[at] != quote) {
        if (text[at] == '\\') { ++at; }
        if (at < text.size() && text[at] == '\n') { ++line; }
        ++at;
      }
      ++at;
      continue;
    }
    if (c == '/' && at + 1 < text.size() && (text[at + 1] == '/' || text[at + 1] == '*')) {
      return line;
    }
    ++at;
  }
  return 0;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::vector<std::string> cases = TheRunnersOwnCases();
  Note("cases the runner declares", (double)cases.size(), "cases");
  CHECK(cases.size() > 200,
        "the runner named its cases -- this walk decides what a proof is by asking it, and a "
        "list it could not read would exempt everything (board:1801)");

  size_t walked = 0;
  std::vector<std::string> narrating;
  for (const char *root : {"src", "include", "tools", "apps"}) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) { continue; }
      const std::string suffix = entry.path().extension().string();
      if (suffix != ".cpp" && suffix != ".h" && suffix != ".msl") { continue; }
      ++walked;
      if (IsAProof(cases, entry.path().string())) { continue; }
      const int line = CommentLine(Slurp(entry.path()));
      if (line > 0) {
        narrating.push_back(entry.path().string() + ":" + std::to_string(line));
      }
    }
  }

  // board:1776: shader and script sources live as FILES in the tree. An embedded blob is a
  // second home for a language the compiler in this translation unit does not check, and
  // both survivors were introduced by a closure rather than by the original code.
  std::vector<std::string> embedded;
  size_t scanned = 0;
  for (const char *root : {"src", "include", "tools", "apps"}) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) { continue; }
      const std::string suffix = entry.path().extension().string();
      if (suffix != ".cpp" && suffix != ".h") { continue; }
      ++scanned;
      const std::string text = Slurp(entry.path());
      for (size_t at = text.find("R\"("); at != std::string::npos;
           at = text.find("R\"(", at + 1)) {
        const size_t ends = text.find(")\"", at);
        const std::string blob = text.substr(at, ends == std::string::npos ? 400 : ends - at);
        const bool shaderish = blob.find("metal_stdlib") != std::string::npos ||
                               blob.find("using namespace metal") != std::string::npos ||
                               blob.find("constant float") != std::string::npos ||
                               blob.find("#version") != std::string::npos ||
                               blob.find("[[stage_in]]") != std::string::npos;
        if (shaderish) {
          size_t line = 1;
          for (size_t scan = 0; scan < at; ++scan) { line += text[scan] == '\n' ? 1 : 0; }
          embedded.push_back(entry.path().string() + ":" + std::to_string(line));
        }
      }
    }
  }
  Note("sources scanned for an embedded shader", (double)scanned, "files");
  for (const std::string &one : embedded) { std::printf("FOUND %s embeds a shader\n", one.c_str()); }
  CHECK(embedded.empty(),
        "**A SHADER LIVES IN A FILE**: src/, include/, tools/ and apps/ hold no MSL or GLSL in a "
        "string literal -- an embedded blob is a second home for a language this translation "
        "unit's compiler never checks (board:1776)");

  // board:1654, regressed by board:1787's own repair: a static_assert message is prose the
  // compiler carries, and "(board:1787)" stood in one. The comment walk deliberately steps
  // over string literals -- so the rule it names in its own Covers could not be enforced by
  // it. A work item's number lives in the board and in the commit, never in the source, and
  // that holds for the text inside a literal as much as for a line above a function.
  // the walk covers all three roots its Covers names. A PROOF may cite the item it proves,
  // wherever it lives -- and a proof is any source carrying Covers(", while every source that
  // does not is bound. Nine live citations stood under tools/ while this walk read two roots
  // and claimed three.
  std::vector<std::string> numbered;
  for (const char *root : {"src", "include", "tools", "apps"}) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
      if (!entry.is_regular_file()) { continue; }
      const std::string suffix = entry.path().extension().string();
      if (suffix != ".cpp" && suffix != ".h" && suffix != ".msl") { continue; }
      const std::string text = Slurp(entry.path());
      if (IsAProof(cases, entry.path().string())) { continue; }
      for (size_t at = text.find("board:"); at != std::string::npos;
           at = text.find("board:", at + 1)) {
        size_t line = 1;
        for (size_t scan = 0; scan < at; ++scan) { line += text[scan] == '\n' ? 1 : 0; }
        numbered.push_back(entry.path().string() + ":" + std::to_string(line));
      }
    }
  }
  for (const std::string &one : numbered) {
    std::printf("FOUND %s names a board item\n", one.c_str());
  }
  CHECK(numbered.empty(),
        "**THE SOURCE NAMES NO WORK ITEM**: a number's origin lives in its board item and "
        "its commit -- and that holds inside a string literal too, where the comment walk "
        "cannot see it and a static_assert message can hide one (board:1654)");

  Note("source files walked", (double)walked, "files");
  for (const std::string &one : narrating) { std::printf("FOUND %s narrates\n", one.c_str()); }
  CHECK(walked >= 300, "the walk saw the tree, not a corner of it");
  CHECK(narrating.empty(),
        "**THE SOURCE CARRIES NO COMMENTARY**: src/, include/, tools/ and apps/ hold no // and "
        "no /* -- names and structure carry the meaning, a number's origin lives in its board "
        "item and its commit, and test/ is the one place prose may stand because a proof "
        "explains what it proves");

  Covers("IV.11 no comment stands in src/, include/, tools/ or apps/ -- the rule is a walk, "
         "not a habit repeated per file (board:1763)");
  return Report();
}
