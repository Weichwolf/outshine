#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Check.h"

namespace {

[[nodiscard]] std::string Ask(const std::string &command, int &verdict) {
  std::string said;
  std::FILE *const pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) {
    verdict = -1;
    return said;
  }
  char block[4096];
  while (std::fgets(block, sizeof block, pipe) != nullptr) { said += block; }
  verdict = pclose(pipe);
  return said;
}

[[nodiscard]] size_t Times(const std::string &haystack, const std::string &needle) {
  size_t count = 0;
  for (size_t at = haystack.find(needle); at != std::string::npos;
       at = haystack.find(needle, at + needle.size())) {
    ++count;
  }
  return count;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // board:1797: the corpus lives in the system temp dir, because artefacts never live in the
  // tree, and the machine sweeps that directory -- it took both outshine-prepared and
  // outshine-content between two gate runs. The tree's way back is ONE command, and until this
  // item that command crashed on two thirds of the corpus: `frame_grid()` read
  // `self.scene.animation` while the fetched families (wpt, test262) declare no scene at all.
  // A preparer that cannot plan the corpus is not a way back.
  size_t declared = 0;
  for (const auto &entry : std::filesystem::recursive_directory_iterator("test")) {
    if (entry.path().filename() == "manifest.json") { ++declared; }
  }
  Note("manifests standing in the tree", (double)declared, "manifests");
  CHECK(declared > 0, "the tree declares a corpus at all");

  // stderr goes to a FILE and not into the pipe: the preparer's stdout is block-buffered when
  // piped while its notices are not, so a merged stream lands a notice INSIDE a JSON line and
  // cuts a token in half -- which is how this claim first read 1180 of 1181 (board:1797).
  int verdict = 0;
  const std::string notices =
      std::filesystem::temp_directory_path().string() + "/outshine-preparer-notices";
  const std::string plan =
      Ask("python3 test/harness/shared/corpus/prepare.py dry-run --every-case 2>" + notices,
          verdict);
  const size_t planned = Times(plan, "\"manifest\":");
  Note("manifests the preparer planned", (double)planned, "manifests");
  std::printf("NOTE the preparer exited %d\n", verdict);
  if (verdict != 0) {
    std::string said;
    if (std::FILE *const reading = std::fopen(notices.c_str(), "r"); reading != nullptr) {
      char block[4096];
      while (std::fgets(block, sizeof block, reading) != nullptr) { said += block; }
      std::fclose(reading);
    }
    const size_t at = said.find("Traceback");
    std::printf("NOTE it said: %.400s\n",
                at == std::string::npos ? said.c_str() : said.c_str() + at);
  }
  std::error_code why;
  std::filesystem::remove(notices, why);

  CHECK(verdict == 0,
        "**THE CORPUS IS PLANNED BY ONE COMMAND**: the preparer is offline by design and stays "
        "so, but the tree's only way back from a swept temp dir has to work -- and a traceback "
        "on the manifests that carry no scene is not a plan (board:1797)");
  CHECK(planned == declared,
        "**AND THAT ONE COMMAND SEES EVERY MANIFEST**: a case with no scene renders nothing, "
        "which is a fact about the case and not an error, so the fetched families plan beside "
        "the rendered ones instead of stopping the walk at the first of them (board:1797)");

  Covers("IV.17 the corpus is planned, and therefore rebuildable, by one command over every "
         "manifest in the tree -- including those that declare no scene, whose render grid is "
         "empty rather than absent (board:1797)");
  return Report();
}
