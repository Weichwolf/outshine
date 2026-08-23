#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include <outshine/Outshine.h>

#include "Check.h"
#include "Heap.h"
#include "PreparedRoot.h"

namespace {

// [SET] the transition population: a hundred door walks is a play session's worth of
// interiors, enough for a p99 and for a leak to show
constexpr int kTransitions = 100;

std::string EntryPath(const std::string &prepared) {
  const std::string manifest = prepared + "/manifest.json";
  std::FILE *const file = std::fopen(manifest.c_str(), "rb");
  if (file == nullptr) { return std::string(); }
  std::string text;
  char block[4096];
  size_t read = 0;
  while ((read = std::fread(block, 1, sizeof block, file)) > 0) { text.append(block, read); }
  std::fclose(file);
  const size_t entry = text.find("\"entry\"");
  if (entry == std::string::npos) { return std::string(); }
  const size_t open = text.find('"', text.find(':', entry));
  const size_t close = text.find('"', open + 1);
  if (open == std::string::npos || close == std::string::npos) { return std::string(); }
  return prepared + "/" + text.substr(open + 1, close - open - 1);
}

std::string Planted(const char *name, const std::string &stands, const char *scenarioName) {
  const std::string path = outshine::Test::PreparedRoot() + "/" + name;
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::string text = R"(<?xml version="1.0" encoding="utf-8"?>
<scenario name=")";
  text += scenarioName;
  text += R"(" version="1">
  <render widthPx="320" heightPx="240" fps="30" fill="0.9"/>
  <lighting><key lux="200" elevationDeg="90" bearingDeg="0"/></lighting>
  <assets><asset uri=")";
  text += stands;
  text += R"(" kind="gltf"/></assets>
</scenario>
)";
  std::fwrite(text.data(), 1, text.size(), file);
  std::fclose(file);
  return path;
}

struct Distribution {
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0, MaxMs = 0.0;
};

[[nodiscard]] Distribution Over(std::vector<double> &samples) {
  Distribution out;
  if (samples.empty()) { return out; }
  std::sort(samples.begin(), samples.end());
  const auto at = [&samples](double fraction) {
    return samples[(size_t)(fraction * (double)(samples.size() - 1) + 0.5)];
  };
  out.P50Ms = at(0.50);
  out.P95Ms = at(0.95);
  out.P99Ms = at(0.99);
  out.MaxMs = samples.back();
  return out;
}

[[nodiscard]] double NowMs() {
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string stands =
      EntryPath(PreparedRoot() + "/" + kPreparedKhronosPrefix + "BoxAnimated");
  CHECK(!stands.empty(), "the transition's subject is in the prepared corpus");
  if (stands.empty()) { return Report(); }
  const std::string exterior = Planted("door-exterior.scenario", stands, "outside");
  const std::string interior = Planted("door-interior.scenario", stands, "inside");
  CHECK(!exterior.empty() && !interior.empty(), "both doors are written");

  outshine::Engine engine;
  const bool outside = engine.Load(exterior);
  if (!outside) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(outside, "the exterior stands");
  if (!outside) { return Report(); }

  // stand the interior once, so both doors exist: from here every walk is
  // park-the-standing, resume-the-other
  bool walked = engine.Park() && engine.Load(interior);
  CHECK(walked, "the interior stands and the exterior waits behind its door");

  std::vector<double> parkMs, resumeMs;
  std::vector<size_t> liveBytes;
  for (int transition = 0; transition < kTransitions && walked; ++transition) {
    double from = NowMs();
    walked = engine.Park();
    parkMs.push_back(NowMs() - from);
    if (!walked) { break; }
    from = NowMs();
    walked = engine.Resume(transition % 2 == 0 ? "outside" : "inside");
    resumeMs.push_back(NowMs() - from);
    liveBytes.push_back(outshine::Heap::LiveBytes());
  }
  if (!walked) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(walked, "a hundred transitions walk both ways through the door");

  Distribution park = Over(parkMs);
  Distribution resume = Over(resumeMs);
  std::printf("NOTE park ms    p50 %.4f  p95 %.4f  p99 %.4f  max %.4f\n", park.P50Ms,
              park.P95Ms, park.P99Ms, park.MaxMs);
  std::printf("NOTE resume ms  p50 %.4f  p95 %.4f  p99 %.4f  max %.4f  (stand the other "
              "door, park it, resume the first)\n",
              resume.P50Ms, resume.P95Ms, resume.P99Ms, resume.MaxMs);
  CHECK(park.P99Ms < 50.0,
        "**A PARK IS BOOKKEEPING**: the declaration moves aside in under 50 ms at p99 -- "
        "residency was never the park's to carry (board:1485)");

  // memory over a hundred transitions: the engine returns to a steady live byte count --
  // the last ten transitions sit within [SET] 1 MB of the tenth, so nothing rides along
  CHECK(liveBytes.size() >= 20, "the byte ledger followed the walk");
  const size_t settled = liveBytes[9];
  size_t worstDrift = 0;
  for (size_t at = liveBytes.size() - 10; at < liveBytes.size(); ++at) {
    const size_t drift = liveBytes[at] > settled ? liveBytes[at] - settled : settled - liveBytes[at];
    worstDrift = drift > worstDrift ? drift : worstDrift;
  }
  Note("the worst live-byte drift after settling", (double)worstDrift / 1024.0, "KiB");
  CHECK(worstDrift < 1024 * 1024,
        "**A HUNDRED TRANSITIONS LEAK NOTHING**: the live byte count settles by the tenth "
        "walk and stays within a megabyte of it -- a door is not a slow leak "
        "(board:1485)");

  Covers("VI.4 the parked door's transition is measured: park at p50/p95/p99 under its "
         "bookkeeping bound, the stand-park-resume round published beside it, and the live "
         "byte count steady over a hundred walks (board:1485)");
  return Report();
}
