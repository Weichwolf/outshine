#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"

#include "Json.h"

namespace {

using outshine::Json;

struct IndexPass {
  const char *Product;
  const char *Indexed;
};

constexpr IndexPass kIndexPasses[] = {{"materialIndexRaw", "materials"},
                                      {"objectIndexRaw", "objects"}};

bool Slurp(const std::filesystem::path &path, std::string &into) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) { return false; }
  into.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return !stream.bad();
}

std::vector<std::filesystem::path> PreparedCases() {
  std::vector<std::filesystem::path> cases;
  std::error_code failed;

  const std::string corpus = outshine::Test::PreparedRoot();
  if (std::filesystem::is_directory(corpus, failed)) {
    for (const std::filesystem::directory_entry &one :
         std::filesystem::recursive_directory_iterator(corpus, failed)) {
      if (one.is_directory() && std::filesystem::is_regular_file(one.path() / "provenance.json")) {
        cases.push_back(one.path());
      }
    }
  }
  std::sort(cases.begin(), cases.end());
  return cases;
}

struct Census {
  size_t Rows = 0;
  size_t Cached = 0;
  size_t WithAnIndexPass = 0;
  size_t Named = 0;
  size_t Refusing = 0;
};

bool MappingIsUsable(const Json::Ref &row, const char *indexed, std::string &why) {
  const Json::Ref entries = row["provenance"]["quantities"]["indices"][indexed];
  if (entries.GetKind() != Json::Kind::Array) {
    why = "the row records no index mapping under quantities.indices.";
    why += indexed;
    return false;
  }
  if (entries.Size() == 0) {
    why = "the row's mapping for ";
    why += indexed;
    why += " is empty, so every integer of the pass names nothing";
    return false;
  }
  for (size_t at = 0; at < entries.Size(); ++at) {
    if (entries[at]["name"].Str("").empty() || entries[at]["passIndex"].Int(-1) < 1) {
      why = "an entry of the mapping for ";
      why += indexed;
      why += " carries no name or no pass index";
      return false;
    }
  }
  return true;
}

Census ReadCase(const std::filesystem::path &directory, const Json &document) {
  Census tally;
  const Json::Ref rows = document.Root()["report"]["render"];
  for (size_t at = 0; at < rows.Size(); ++at) {
    const Json::Ref row = rows[at];
    ++tally.Rows;
    if (row["cache"].Str("") == "hit") { ++tally.Cached; }
    for (const IndexPass &pass : kIndexPasses) {
      if (row["products"][pass.Product]["path"].Str("").empty()) { continue; }
      ++tally.WithAnIndexPass;
      std::string why;
      if (MappingIsUsable(row, pass.Indexed, why)) {
        ++tally.Named;
        continue;
      }
      ++tally.Refusing;
      outshine::Test::Note(
          (directory.string() + " recipe " + row["recipe"].Str("?") + ": " + why).c_str());
    }
  }
  return tally;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<std::filesystem::path> cases = PreparedCases();
  Note("prepared cases carrying a provenance document", (double)cases.size(), "cases");
  if (cases.empty()) {
    Unprepared(
        "test/harness/shared/corpus/prepare.py has produced no provenance.json to read a mapping "
        "out of");
    return Report();
  }

  Census all;
  for (const std::filesystem::path &directory : cases) {
    std::string text;
    if (!Slurp(directory / "provenance.json", text)) { continue; }
    Json document;
    if (!document.Parse(text.c_str(), text.size())) {
      CHECK(false, (directory.string() + "/provenance.json parses").c_str());
      continue;
    }
    const Census one = ReadCase(directory, document);
    all.Rows += one.Rows;
    all.Cached += one.Cached;
    all.WithAnIndexPass += one.WithAnIndexPass;
    all.Named += one.Named;
    all.Refusing += one.Refusing;
  }

  Note("render rows across the prepared corpus", (double)all.Rows, "rows");
  Note("render rows served from the content store", (double)all.Cached, "rows");
  Note("index passes placed by those rows", (double)all.WithAnIndexPass, "passes");
  Note("index passes whose mapping came back with them", (double)all.Named, "passes");

  if (all.Rows == 0) {
    Unprepared("the prepared corpus carries no render row: nothing has rendered it on this "
               "machine, and `make test` is what does -- suspended until the engine stands on "
               "the reference design (board:2153)");
    return Report();
  }

  CHECK(all.Refusing == 0,
        "every index pass on disk carries the mapping its integers are read through, whether the "
        "render that produced it ran in this run or was served from the store");

  CHECK(all.WithAnIndexPass > 0,
        "the corpus places index passes at all, so a green count here is not a count over nothing");

  Covers("the pass-index mapping is a keyed product of the render and comes back from the "
         "store with the bytes it describes, so a cached oracle still names its own indices");
  return Report();
}
