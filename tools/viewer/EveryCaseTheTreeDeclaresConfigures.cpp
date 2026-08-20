#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "Check.h"

#include "PreparedRoot.h"
#include "RenderCase.h"

namespace {

struct Listed {
  std::string Suite;
  std::string Name;
  std::string Prepared;
  bool Ready = false;

  bool Document = false;
};

std::string Flattened(std::string path) {
  for (char &c : path) {
    if (c == '/') { c = '-'; }
  }
  return path;
}

std::vector<Listed> Cases(void) {
  std::vector<Listed> found;
  std::error_code walking;
  for (std::filesystem::recursive_directory_iterator it("test", walking), end; it != end;
       it.increment(walking)) {
    if (walking) { break; }
    if (!it->is_regular_file(walking) || it->path().filename() != "manifest.json") { continue; }
    const std::string relative = it->path().parent_path().string();

    if (relative.find("/prepare") != std::string::npos) { continue; }
    Listed one;
    one.Name = it->path().parent_path().filename().string();
    one.Prepared = outshine::Test::PreparedRoot() + "/" + Flattened(relative);
    const std::string inside = relative.substr(5);
    const size_t cut = inside.rfind('/');
    one.Suite = cut == std::string::npos ? inside : inside.substr(0, cut);
    one.Ready =
        std::filesystem::exists(std::filesystem::path(one.Prepared) / "manifest.json", walking);
    std::ifstream declaration(it->path());
    const std::string text((std::istreambuf_iterator<char>(declaration)),
                           std::istreambuf_iterator<char>());
    one.Document = text.find("\"outshine/declared-case-manifest\"") != std::string::npos;
    found.push_back(std::move(one));
  }
  std::sort(found.begin(), found.end(), [](const Listed &a, const Listed &b) {
    return a.Suite == b.Suite ? a.Name < b.Name : a.Suite < b.Suite;
  });
  return found;
}

}

int main(void) {
  using namespace outshine::Test;

  const std::vector<Listed> listed = Cases();
  CHECK(!listed.empty(), "the tree declares at least one case, so this browser has something to show");

  int configured = 0, unprepared = 0, declined = 0, documents = 0;
  std::string suite;
  for (const Listed &one : listed) {
    if (one.Suite != suite) {
      suite = one.Suite;
      std::printf("SUITE %s\n", suite.c_str());
    }

    if (one.Document) {
      ++documents;
      continue;
    }
    if (!one.Ready) {
      ++unprepared;
      std::printf("  UNPREPARED %s\n", one.Name.c_str());
      continue;
    }
    ConfiguredCase held;
    std::string why;
    const bool reads = held.Read(one.Prepared, why);

    if (!reads && held.Declines()) {
      ++declined;
      std::printf("  DECLINED %s -- %s\n", one.Name.c_str(), why.c_str());
      continue;
    }

    CHECK(reads, one.Name.c_str());
    if (!reads) {
      std::printf("       %s\n", why.c_str());
      continue;
    }
    ++configured;
    std::printf("  %-38s %4d x %-4d  %d frame(s)\n", one.Name.c_str(), held.WidthPx(),
                held.HeightPx(), held.Frames());
  }
  std::printf("NOTE document cases declared, awaiting the paint layer = %d\n", documents);
  std::printf("NOTE cases the tree declares = %zu\n", listed.size());
  std::printf("NOTE cases configured = %d\n", configured);
  std::printf("NOTE cases whose preparation has not run = %d\n", unprepared);
  std::printf("NOTE cases this engine declines by their own declaration = %d\n", declined);
  Covers("a suite is a folder with manifests in it, and every case it declares configures "
         "for outshine through the one reader the runner uses");
  return Report();
}
