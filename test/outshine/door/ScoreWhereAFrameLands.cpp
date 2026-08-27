#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <Outshine.h>
#include <Scenario.h>

#include "Check.h"

namespace {

// A CLIENT THAT NAMES A PATH HAS SAID WHERE THE FRAME GOES.
//
// `Engine::Capture(path)` encoded the picture, opened the file, and refused when the open
// failed -- which it does whenever the directory the caller named is not already there:
//
//   REFUSED the screenshot could not be opened for writing at DIR/along01.png   exit 1
//
// Three architecture reviews in a row opened with `mkdir`, on a run whose whole purpose is to
// be reproducible from the command CLAUDE.md prints. The refusal was accurate and useless: it
// named a condition the caller cannot distinguish from a permission failure or a full disk, and
// it threw away a frame that had already been rendered and encoded.
//
// A door that writes a file at a path it was given makes that path work. `create_directories`
// is the whole of it, and it stays a REFUSAL when the path cannot be made -- what changed is
// that a directory which merely does not exist yet is no longer a reason to lose a picture.
//
// The case renders one frame into a path three levels deep that no test has ever made, then
// asserts the file is there and carries a PNG. The three levels matter: a single missing
// component could be made by accident by something else in the process, and a chain of three
// cannot.
constexpr int kFramePx = 32;

[[nodiscard]] bool LooksLikeAPng(const std::string &path) {
  std::FILE *const file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) { return false; }
  unsigned char head[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  const size_t got = std::fread(head, 1, sizeof head, file);
  (void)std::fclose(file);
  static const unsigned char kMagic[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  if (got != sizeof head) { return false; }
  for (size_t at = 0; at < sizeof head; ++at) {
    if (head[at] != kMagic[at]) { return false; }
  }
  return true;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const char *nest = std::getenv("OUTSHINE_NEST");
  if (nest == nullptr || *nest == 0) {
    Unprepared("this case writes its frame into the runner's nest and was given none");
    return Report();
  }
  const std::string under = nest;
  const std::string deep = under + "/landing/three/levels";
  std::error_code why;
  std::filesystem::remove_all(under + "/landing", why);
  if (std::filesystem::exists(deep)) {
    Unprepared("the directory this case needs absent is present and could not be removed");
    return Report();
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so nothing can be rendered");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-door-cache", true});
  if (!engine.DrawsInto(outshine::Extent{kFramePx, kFramePx})) {
    Unprepared("the device stood no canvas");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{kFramePx, kFramePx};
  stands.Ground.Declared = true;
  stands.Ground.Lat = 48.1372;
  stands.Ground.Lon = 11.5756;
  stands.Lit.Declared = true;
  stands.Lit.Key.Lux = 40000.0;
  stands.Lit.Key.ElevationDeg = 42.0;
  if (!engine.Declare(stands) || !engine.Advance()) {
    Unprepared(("nothing stood to be captured: " + engine.Error()).c_str());
    return Report();
  }

  const std::string named = deep + "/frame.png";
  const bool kept = engine.Capture(named);
  std::printf("  captured into a path three levels deep that did not exist: %s\n",
              kept ? "kept" : ("REFUSED -- " + engine.Error()).c_str());
  std::printf("  the directory now stands: %s\n",
              std::filesystem::exists(deep) ? "yes" : "no");
  std::printf("  the file carries a png header: %s\n",
              LooksLikeAPng(named) ? "yes" : "no");

  CHECK(kept,
        "**A DOOR THAT WRITES A FILE AT A PATH IT WAS GIVEN MAKES THAT PATH WORK**: the frame is "
        "rendered and encoded before the file is opened, so refusing on a directory that merely "
        "does not exist yet throws away a picture that already existed. Three architecture "
        "reviews in a row began with mkdir on a run whose purpose is to be reproducible from the "
        "one command the map prints");

  CHECK(std::filesystem::exists(deep),
        "and it makes the WHOLE path: three levels, so a single component appearing by accident "
        "cannot be mistaken for the door having made it");

  CHECK(LooksLikeAPng(named),
        "and what lands there is a picture: the first eight bytes are PNG's own signature, so "
        "the case is judging a frame rather than a file that happens to have a name");

  Covers("the door: a frame lands at the path the caller named, and a directory that does not "
         "exist yet is made rather than being a reason to lose a picture that is already "
         "rendered");
  return Report();
}
