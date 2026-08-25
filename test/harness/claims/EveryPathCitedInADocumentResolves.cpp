#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include "Check.h"

namespace {

std::vector<std::filesystem::path> Documents() {
  std::vector<std::filesystem::path> found;
  if (std::filesystem::exists("CLAUDE.md")) { found.emplace_back("CLAUDE.md"); }
  for (const char *const tree : {"src/assets/tables"}) {
    if (!std::filesystem::is_directory(tree)) { continue; }
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::recursive_directory_iterator(tree)) {
      if (entry.is_regular_file() && entry.path().extension() == ".md") { found.push_back(entry); }
    }
  }
  return found;
}

bool IsABareOwnDocument(const std::string &span) {
  if (span.find('/') != std::string::npos) { return false; }
  if (span.size() < 4 || span.compare(span.size() - 3, 3, ".md") != 0) { return false; }
  return span.find("NNNN") == std::string::npos;
}

bool NamesThisTree(const std::string &span) {
  static const char *const kOurs[] = {"src/", "test/", "board/"};
  for (const char *const top : kOurs) {
    if (span.compare(0, std::string(top).size(), top) == 0) { return true; }
  }
  return IsABareOwnDocument(span);
}

bool IsAPlaceholder(const std::string &span) {
  return span.find_first_of("<>* ") != std::string::npos ||
         span.find("…") != std::string::npos || span.find("://") != std::string::npos;
}

std::string WithoutLineReference(const std::string &span) {
  const size_t colon = span.rfind(':');
  if (colon == std::string::npos || colon + 1 == span.size()) { return span; }
  for (size_t at = colon + 1; at < span.size(); ++at) {
    const char character = span[at];
    const bool digit = character >= '0' && character <= '9';
    if (!digit && character != ',' && character != '-') { return span; }
  }
  return span.substr(0, colon);
}

std::vector<std::string> Expanded(const std::string &span) {
  const size_t open = span.find('{');
  const size_t close = span.find('}', open == std::string::npos ? 0 : open);
  if (open == std::string::npos || close == std::string::npos) { return {span}; }
  std::vector<std::string> spans;
  const std::string head = span.substr(0, open);
  const std::string tail = span.substr(close + 1);
  std::string part;
  for (size_t at = open + 1; at <= close; ++at) {
    if (at < close && span[at] != ',') {
      part += span[at];
      continue;
    }
    spans.push_back(head + part + tail);
    part.clear();
  }
  return spans;
}

bool Resolves(const std::string &path) {
  if (!path.empty() && path.back() == '/') { return std::filesystem::is_directory(path); }
  if (std::filesystem::exists(path)) { return true; }
  return std::filesystem::is_regular_file(path + ".cpp");
}

struct Citation {
  std::string Document;
  int Line = 0;
  std::string Path;
};

void CitationsIn(const std::filesystem::path &document, std::vector<Citation> &into) {
  std::FILE *file = std::fopen(document.c_str(), "rb");
  if (!file) { return; }
  std::string text;
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    text.append(block, read);
  }
  std::fclose(file);

  int line = 1;
  for (size_t at = 0; at < text.size(); ++at) {
    if (text[at] == '\n') {
      ++line;
      continue;
    }
    if (text[at] != '`') { continue; }
    const size_t close = text.find_first_of("`\n", at + 1);
    if (close == std::string::npos || text[close] == '\n') { continue; }
    const std::string span = text.substr(at + 1, close - at - 1);
    at = close;
    if (!NamesThisTree(span) || IsAPlaceholder(span)) { continue; }
    for (const std::string &cited : Expanded(WithoutLineReference(span))) {
      into.push_back({document.string(), line, cited});
    }
  }
}

}

int main() {
  using namespace outshine::Test;

  const std::vector<std::filesystem::path> documents = Documents();
  CHECK(!documents.empty(), "the documents are readable from where the harness runs, so there is "
                            "something to check the citations of");
  if (documents.empty()) { return Report(); }

  std::vector<Citation> cited;
  for (const std::filesystem::path &document : documents) { CitationsIn(document, cited); }
  Note("documents read", (double)documents.size(), "files");
  Note("paths cited into this tree", (double)cited.size(), "citations");

  size_t unresolved = 0;
  for (const Citation &citation : cited) {
    if (Resolves(citation.Path)) { continue; }
    ++unresolved;
    std::printf("NOTE   %s:%d cites %s, which is not in the tree\n", citation.Document.c_str(),
                citation.Line, citation.Path.c_str());
  }
  Note("citations that do not resolve", (double)unresolved, "citations");
  CHECK(unresolved == 0,
        "every path a document of this repository cites into this repository exists, so a tick that "
        "names its file is a claim with something under it");

  Covers("I.37 a document that cites a file names one that is in the tree: every backticked path in "
         "CLAUDE.md and src/assets/tables/ resolves, or the run is red");
  return Report();
}
