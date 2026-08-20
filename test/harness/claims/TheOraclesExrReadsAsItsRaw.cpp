#include <cstdint>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/stat.h>

#include "Check.h"

#include "PreparedRoot.h"

#include "render/Exr.h"
#include "render/RawF32.h"

using outshine::Render::Parity::Exr;
using outshine::Render::Parity::RawF32;

namespace {

const char *const kRgba[4] = {"R", "G", "B", "A"};

bool Exists(const std::string &path) {
  struct stat entry {};
  return stat(path.c_str(), &entry) == 0 && S_ISREG(entry.st_mode);
}

std::vector<std::string> PreparedCases(size_t &unpaired) {
  std::vector<std::string> cases;
  std::error_code failed;

  const std::string corpus = outshine::Test::PreparedRoot();
  if (std::filesystem::is_directory(corpus, failed)) {
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

}

int main() {
  using namespace outshine::Test;

  size_t unpaired = 0;
  std::vector<std::string> cases = PreparedCases(unpaired);
  Note("prepared cases carrying both an oracle EXR and its raw", (double)cases.size(), "cases");
  CHECK(unpaired == 0,
        "no case carries one of the pair without the other, which would be a preparer defect rather "
        "than an unprepared tree");

  if (cases.empty()) {
    Unprepared("test/harness/shared/corpus/prepare.py has produced no oracle.exr/oracle.raw pair to decode");
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
  Covers("the runner can read the oracle's EXR directly, so the flat raw beside it is a "
         "derived cache rather than an artefact that must survive");
  return Report();
}
