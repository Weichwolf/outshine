/* THE BOARD DECLARES SEVEN INVARIANTS AND, UNTIL THIS FILE, CHECKED NONE OF THEM. They were run by hand,
 * from a shell, by whoever happened to be looking — which is the shape the board exists to replace, one
 * level up: a pass that has to be remembered is a pass that will not happen. Cost was never the reason;
 * the whole battery measures 0.46 s over 1 117 files by hand (board:1124).
 *
 * IT IS ALSO WHAT HOLDS `board:0038`, and that is not a stretch. That item was *the todo describes a
 * tree three commits ago*; the file is gone, so nothing could prove it, and it sat closed and
 * unproven — caught by this file's own last invariant. Its concern survives its subject: the board
 * can go stale exactly as the todo did, and THIS is the thing that would notice. A carve-out for
 * *items closed by subject deletion* would have been the first exemption in a design that has
 * refused every other one.
 *
 * THE QUERY IS NOT HERE AND MUST NOT BE ADDED. *What is ready to start* — open items whose every
 * dependency is closed — is a QUERY. A board with nothing ready is a legitimate state, and a suite that
 * went red on it would teach people to keep a fake item open. `CLAUDE.md` says so; this comment is here
 * because the helpful addition is the one a later round makes without reading that.
 *
 * IT WALKS `test/` ITSELF RATHER THAN SHELLING OUT. `test/render/.gitignore` ignores everything and then
 * re-includes the runner's own sources at its root, so a case directory carries only its manifest. Git
 * honours that negation and some greps do not — so a recursive grep silently skips `Parity.cpp`, the
 * largest test file in the tree, and an item proven only by a render case reads as unproven. A directory
 * walk has no ignore-file behaviour to disagree about. */
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Check.h"

namespace {

struct Item {
  std::string Id;
  std::string State;   /* the directory, because the path IS the state */
  std::string Type;
  std::string Parent;
  std::vector<std::string> Depends;
  std::filesystem::path File;
};

std::string Trimmed(const std::string &text) {
  const size_t first = text.find_first_not_of(" \t\r");
  if (first == std::string::npos) { return {}; }
  return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

/* `Depends: 1123, 0078` is one field naming several ids, and each is its own edge. */
std::vector<std::string> Ids(const std::string &value) {
  std::vector<std::string> ids;
  size_t at = 0;
  while (at <= value.size()) {
    const size_t comma = value.find(',', at);
    const std::string one = Trimmed(value.substr(at, comma - at));
    if (!one.empty()) { ids.push_back(one); }
    if (comma == std::string::npos) { break; }
    at = comma + 1;
  }
  return ids;
}

std::string HeaderField(const std::string &text, const char *name) {
  const std::string key = std::string("\n") + name + ":";
  const std::string head = "\n" + text;                    /* so a first-line field matches too */
  const size_t at = head.find(key);
  if (at == std::string::npos) { return {}; }
  const size_t from = at + key.size();
  const size_t end = head.find('\n', from);
  return Trimmed(head.substr(from, end - from));
}

std::string Read(const std::filesystem::path &path) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) { return {}; }
  std::string text;
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    text.append(block, read);
  }
  std::fclose(file);
  return text;
}

std::vector<Item> Board() {
  std::vector<Item> items;
  for (const char *const state : {"open", "active", "closed"}) {
    const std::filesystem::path directory = std::filesystem::path("board") / state;
    if (!std::filesystem::is_directory(directory)) { continue; }
    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(directory)) {
      if (!entry.is_regular_file() || entry.path().extension() != ".md") { continue; }
      const std::string name = entry.path().filename().string();
      if (name.size() < 4) { continue; }
      const std::string text = Read(entry.path());
      Item item;
      item.Id = name.substr(0, 4);
      item.State = state;
      item.Type = HeaderField(text, "Type");
      item.Parent = HeaderField(text, "Parent");
      item.Depends = Ids(HeaderField(text, "Depends"));
      item.File = entry.path();
      items.push_back(item);
    }
  }
  return items;
}

/* Every id any source or test file cites, walked rather than grepped — see the header comment. Markers
 * live in comments, so only the text a compiler or a shell would read is opened. */
std::set<std::string> CitedFrom(const char *tree) {
  std::set<std::string> ids;
  if (!std::filesystem::is_directory(tree)) { return ids; }
  for (const std::filesystem::directory_entry &entry :
       std::filesystem::recursive_directory_iterator(tree)) {
    if (!entry.is_regular_file()) { continue; }
    const std::string extension = entry.path().extension().string();
    const bool textual = extension == ".cpp" || extension == ".h" || extension == ".sh" ||
                         extension == ".py" || extension == ".metal" || extension == ".json";
    if (!textual) { continue; }
    const std::string text = Read(entry.path());
    for (size_t at = text.find("board:"); at != std::string::npos; at = text.find("board:", at + 1)) {
      const std::string id = text.substr(at + 6, 4);
      if (id.size() == 4 && id.find_first_not_of("0123456789") == std::string::npos) {
        ids.insert(id);
      }
    }
  }
  return ids;
}

bool Reaches(const std::map<std::string, const Item *> &by, const std::string &from,
             const std::string &target, std::set<std::string> &seen) {
  const auto found = by.find(from);
  if (found == by.end() || !seen.insert(from).second) { return false; }
  for (const std::string &next : found->second->Depends) {
    if (next == target || Reaches(by, next, target, seen)) { return true; }
  }
  return false;
}

} // namespace

int main() {
  using namespace outshine::Test;

  const std::vector<Item> items = Board();
  CHECK(!items.empty(), "the board is readable from where the harness runs, so there is something to "
                        "check the invariants of");
  if (items.empty()) { return Report(); }

  std::map<std::string, const Item *> by;
  size_t duplicates = 0;
  for (const Item &item : items) {
    if (!by.emplace(item.Id, &item).second) { ++duplicates; }
  }
  Note("work items", (double)items.size(), "files");
  Note("duplicate ids", (double)duplicates, "ids");
  CHECK(duplicates == 0, "an id names exactly one work item, so a marker in source resolves to one file");

  /* Every edge resolves. This is the citation test's rule in the board's own graph: an edge naming an
   * id that is not there is a claim about a work item that does not exist. */
  size_t unresolved = 0;
  size_t edges = 0;
  for (const Item &item : items) {
    for (const std::string &id : item.Depends) {
      ++edges;
      if (by.count(id) == 0) {
        ++unresolved;
        std::printf("NOTE   %s depends on %s, which is not a work item\n", item.Id.c_str(), id.c_str());
      }
    }
    if (item.Parent.empty()) { continue; }
    ++edges;
    if (by.count(item.Parent) == 0) {
      ++unresolved;
      std::printf("NOTE   %s names parent %s, which is not a work item\n", item.Id.c_str(),
                  item.Parent.c_str());
    }
  }
  Note("edges", (double)edges, "edges");
  Note("edges that do not resolve", (double)unresolved, "edges");
  CHECK(unresolved == 0, "every id in an edge resolves to a work item, so the graph has no dangling arm");

  /* Two levels and no third: a feature is the top, a task belongs to exactly one, a bug stands alone. */
  size_t featureWithParent = 0, taskWithoutParent = 0, parentNotAFeature = 0;
  for (const Item &item : items) {
    if (item.Type == "feature" && !item.Parent.empty()) { ++featureWithParent; }
    if (item.Type != "task") { continue; }
    if (item.Parent.empty()) {
      ++taskWithoutParent;
      continue;
    }
    const auto parent = by.find(item.Parent);
    if (parent != by.end() && parent->second->Type != "feature") { ++parentNotAFeature; }
  }
  Note("features carrying a parent", (double)featureWithParent, "items");
  Note("tasks with no parent", (double)taskWithoutParent, "items");
  Note("tasks parented to something that is not a feature", (double)parentNotAFeature, "items");
  CHECK(featureWithParent == 0, "a feature is the top of the hierarchy, so it carries no parent");
  CHECK(taskWithoutParent == 0, "a task belongs to a feature, so it names exactly one parent");
  CHECK(parentNotAFeature == 0,
        "a task's parent is a feature and never another task, so there are two levels and no third");

  size_t cycles = 0;
  for (const Item &item : items) {
    std::set<std::string> seen;
    if (Reaches(by, item.Id, item.Id, seen)) {
      ++cycles;
      std::printf("NOTE   %s depends on itself through the graph\n", item.Id.c_str());
    }
  }
  Note("items on a dependency cycle", (double)cycles, "items");
  CHECK(cycles == 0, "the dependency graph is acyclic, so a build order exists");

  /* What is shipped cannot rest on what is unbuilt, in either relation. */
  size_t closedOnOpen = 0, closedFeatureOpenChild = 0;
  for (const Item &item : items) {
    if (item.State != "closed") { continue; }
    for (const std::string &id : item.Depends) {
      const auto other = by.find(id);
      if (other != by.end() && other->second->State != "closed") { ++closedOnOpen; }
    }
    if (item.Type != "feature") { continue; }
    for (const Item &child : items) {
      if (child.Parent == item.Id && child.State != "closed") { ++closedFeatureOpenChild; }
    }
  }
  Note("closed items depending on one that is not closed", (double)closedOnOpen, "edges");
  Note("closed features with an open child", (double)closedFeatureOpenChild, "children");
  CHECK(closedOnOpen == 0, "a closed item does not rest on an unbuilt one");
  CHECK(closedFeatureOpenChild == 0,
        "a closed feature has no open child, so a ticked headline cannot hide unbuilt work under it");

  /* The strongest one, and the only one read from a tree other than the board: a closed item nothing
   * under `test/` cites is a claim with no evidence. */
  const std::set<std::string> proven = CitedFrom("test");
  size_t unproven = 0, closed = 0;
  for (const Item &item : items) {
    if (item.State != "closed") { continue; }
    ++closed;
    if (proven.count(item.Id) == 0) {
      ++unproven;
      std::printf("NOTE   %s is closed and nothing under test/ cites board:%s\n", item.Id.c_str(),
                  item.Id.c_str());
    }
  }
  Note("closed items", (double)closed, "items");
  Note("ids cited from test/", (double)proven.size(), "ids");
  Note("closed items nothing under test/ cites", (double)unproven, "items");
  CHECK(unproven == 0,
        "a closed work item is cited by something under test/, so what is claimed done has evidence "
        "read from the tree that runs rather than from the board that claims it");

  Covers("board:1124 the seven invariants CLAUDE.md declares over board/ are checked by the harness "
         "rather than by hand, and the readiness query is deliberately absent");
  return Report();
}
