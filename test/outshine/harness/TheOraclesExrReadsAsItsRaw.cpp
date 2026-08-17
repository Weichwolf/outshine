/* THE C++ EXR READER AGAINST THE ARTEFACT IT REPLACES (board:1119).
 *
 * THE ACCEPTANCE IS BIT-EXACTNESS AND NOTHING SOFTER: reading `oracle.exr` and stacking `R,G,B,A`
 * must reproduce `oracle.raw` sample for sample. Both files are already committed products of one
 * Blender render, `oracle.raw` was written by the Python reader whose port this is, and a float that
 * differs in its last bit is a decoder that differs -- there is no rounding anywhere in this path to
 * hide behind.
 *
 * IT IS THE SAME TEST THE PYTHON READER PASSED, IN THE OTHER LANGUAGE. That reader was validated
 * against this exact artefact before it was trusted with a single new channel, and this one earns its
 * place the same way rather than by inspection of the two sources.
 *
 * IT LIVES IN `harness` BECAUSE ITS SUBJECT IS A FILE FORMAT. It needs no device, no camera, no
 * oracle comparison and no picture: `unit` decides a computation within one layer's include set,
 * `render` decides pixels against Cycles, and a decoder is neither. The harness layer links nothing
 * but the standard library and zlib, which is the whole of what this reads.
 *
 * EVERY PREPARED CASE, NOT ONE. A decoder validated on a single file is a decoder validated on one
 * image's channel order, one compression choice and one set of scanline block boundaries. The corpus
 * carries the population that actually exists, so the population is what it is run over -- and a tree
 * with nothing prepared is UNPREPARED and says so, never a silent pass over zero files. */
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "Check.h"

#include "render/Exr.h"
#include "render/RawF32.h"

using outshine::Render::Parity::Exr;
using outshine::Render::Parity::RawF32;

namespace {

/* The channels a beauty dump stacks, in the order `OSRAWF32` declares them. */
const char *const kRgba[4] = {"R", "G", "B", "A"};


bool Exists(const std::string &path) {
  struct stat entry {};
  return stat(path.c_str(), &entry) == 0 && S_ISREG(entry.st_mode);
}

/* Every `test/khronos/glTF/<feature>/<case>/` that carries BOTH products. A case with neither has not been
 * prepared; a case with one of the two is a preparer defect and is reported as a missing pair rather
 * than skipped. */
/* A CASE IS FOUND BY WHAT IT CARRIES, NOT BY HOW DEEP IT SITS (board:1196). The two-level walk this
 * replaces encoded one corpus's layout; naming a case for the model it carries put three of them a
 * level lower and the population silently lost them. */
std::vector<std::string> PreparedCases(size_t &unpaired) {
  std::vector<std::string> cases;
  std::error_code failed;
  for (const char *const corpus : {"test/khronos/glTF", "test/outshine/render"}) {
    if (!std::filesystem::is_directory(corpus, failed)) { continue; }
    for (const std::filesystem::directory_entry &one :
         std::filesystem::recursive_directory_iterator(corpus, failed)) {
      if (!one.is_directory()) { continue; }
      const std::string directory = one.path().string() + "/";
      const bool exr = Exists(directory + "oracle.exr");
      const bool raw = Exists(directory + "oracle.raw");
      if (exr && raw) {
        cases.push_back(directory);
      } else if (exr || raw) {
        ++unpaired;
      }
    }
  }
  std::sort(cases.begin(), cases.end());
  return cases;
}

} // namespace

int main() {
  using namespace outshine::Test;

  size_t unpaired = 0;
  std::vector<std::string> cases = PreparedCases(unpaired);
  Note("prepared cases carrying both an oracle EXR and its raw", (double)cases.size(), "cases");
  CHECK(unpaired == 0,
        "no case carries one of the pair without the other, which would be a preparer defect rather "
        "than an unprepared tree");

  /* A DECODER PROVED OVER ZERO FILES IS THE VACUOUS GATE. `Unprepared` is the harness's own verdict
   * for "this needs an input nobody prepared", and it is not a pass. */
  if (cases.empty()) {
    Unprepared("test/outshine/corpus/prepare.py has produced no oracle.exr/oracle.raw pair to decode");
    return Report();
  }

  size_t compared = 0;
  size_t samples = 0;
  for (const std::string &directory : cases) {
    Exr exr;
    const bool decoded = exr.ReadFile(directory + "oracle.exr");
    CHECK(decoded, (directory + "oracle.exr decodes").c_str());
    if (!decoded) {
      Note(exr.Error().c_str());
      continue;
    }
    RawF32 raw;
    const bool loaded = raw.ReadFile(directory + "oracle.raw");
    CHECK(loaded, (directory + "oracle.raw loads through the reader it was written for").c_str());
    if (!loaded) {
      Note(raw.Error().c_str());
      continue;
    }
    const bool sameShape = exr.Width() == raw.Width() && exr.Height() == raw.Height();
    CHECK(sameShape, (directory + ": the EXR and the raw cover one frame").c_str());
    if (!sameShape) { continue; }

    const std::vector<float> *plane[4] = {nullptr, nullptr, nullptr, nullptr};
    bool everyChannel = true;
    for (int channel = 0; channel < 4; ++channel) {
      plane[channel] = exr.Plane(kRgba[channel]);
      everyChannel = everyChannel && plane[channel] != nullptr;
    }
    CHECK(everyChannel, (directory + "oracle.exr carries R, G, B and A").c_str());
    if (!everyChannel) { continue; }

    /* THE COMPARISON IS `==` ON f32 AND THAT IS DELIBERATE. Both sides came from the same bytes
     * through two decoders; anything but equality would be one of them rounding. */
    size_t apart = 0;
    double worst = 0;
    for (int y = 0; y < raw.Height(); ++y) {
      for (int x = 0; x < raw.Width(); ++x) {
        for (int channel = 0; channel < 4; ++channel) {
          const float mine = (*plane[channel])[(size_t)y * (size_t)raw.Width() + (size_t)x];
          const float theirs = raw.At(x, y, channel);
          ++samples;
          if (mine == theirs) { continue; }
          ++apart;
          const double gap = (double)mine - (double)theirs;
          if (gap > worst || -gap > worst) { worst = gap < 0 ? -gap : gap; }
        }
      }
    }
    CHECK(apart == 0,
          (directory + "oracle.exr decodes to the samples oracle.raw holds, bit for bit").c_str());
    if (apart != 0) {
      Note((directory + ": samples differing").c_str(), (double)apart, "samples");
      Note((directory + ": worst difference").c_str(), worst, "absolute");
    }
    ++compared;
  }

  Note("cases decoded and held against their raw", (double)compared, "cases");
  Note("samples compared", (double)samples, "samples");
  Covers("board:1119 the runner can read the oracle's EXR directly, so the flat raw beside it is a "
         "derived cache rather than an artefact that must survive");
  return Report();
}
