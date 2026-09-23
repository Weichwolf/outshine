#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {
constexpr std::array<std::string_view, 23> kProductMeasures{
    "restand: the geometry handed over, digested",
    "ground candidate: source sheets digest, low half",
    "ground candidate: source sheets digest, high half",
    "ground candidate: pad stamps digest, low half",
    "ground candidate: pad stamps digest, high half",
    "ground candidate: lake stamps digest, low half",
    "ground candidate: lake stamps digest, high half",
    "ground candidate: corridor stamps digest, low half",
    "ground candidate: corridor stamps digest, high half",
    "ground: the sheets' digest, low half",
    "ground: the sheets' digest, high half",
    "class field: the structure's digest, low half",
    "class field: the structure's digest, high half",
    "the geometry the world built, high half",
    "and its low half",
    "the triangles handed to the renderer",
    "in this many parts",
    "ground: height pages standing",
    "ground: tiles the lattice draws",
    "ground: sheets NOT drawn for want of nodes",
    "ground: rims copied for want of a neighbour",
    "ground candidate: OSM tile order, low half",
    "ground candidate: OSM tile order, high half"};

using ProductSignature = std::array<double, kProductMeasures.size()>;

outshine::Scenario::Document Scenario() {
  outshine::Scenario::Document scenario;
  scenario.Ground.Declared = true;
  scenario.Ground.VegetationEnabled = false;
  scenario.Ground.Origin.LatitudeDeg = 49.3777;
  scenario.Ground.Origin.LongitudeDeg = 10.179;
  scenario.Ground.PatienceS = 3;
  scenario.Ground.SightM = 8000;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {160, 90};
  scenario.Render.Audits = true;
  scenario.Lit.Declared = true;
  outshine::Scenario::View view;
  view.Id = "v";
  view.Person = "first";
  view.Placement = outshine::Scenario::CameraPlacement::Geodetic;
  view.Geographic.Geodetic.LatitudeDeg = 49.3777;
  view.Geographic.Geodetic.LongitudeDeg = 10.179;
  view.Geographic.Geodetic.HeightM = 1.7;
  view.Geographic.SamplesHeight = false;
  view.Sees.FovDeg = 55;
  scenario.Views.push_back(view);
  return scenario;
}

std::optional<double> Measure(const outshine::Engine &engine, std::string_view name) {
  for (const outshine::DiagnosticSample &sample : engine.measures()) {
    if (sample.Name == name) { return sample.Value; }
  }
  return std::nullopt;
}

std::optional<ProductSignature> Builds(bool preload) {
  using namespace outshine::Test;
  outshine::Engine engine;
  if (!engine.setRoots({"src/assets/drive", "src/assets", "/tmp/outshine-paced", false}) ||
      !engine.drawsInto({160, 90})) {
    Unprepared("the offscreen target did not start");
    return std::nullopt;
  }
  const auto declared = engine.declare(Scenario());
  CHECK(declared.has_value(), declared ? "scenario declared" : declared.error().c_str());
  if (!declared) { return std::nullopt; }
  const auto assembled = engine.assemble();
  CHECK(assembled.has_value(), assembled ? "scenario assembled" : assembled.error().c_str());
  if (!assembled) { return std::nullopt; }
  if (preload) {
    const auto loaded = engine.preload(15.0);
    CHECK(loaded.has_value(), loaded ? "playable preload completed" : loaded.error().c_str());
    if (!loaded) { return std::nullopt; }
  }
  const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!engine.settled(outshine::WorldQuality::Refined) &&
         std::chrono::steady_clock::now() < end) {
    const auto advanced = engine.advance();
    CHECK(advanced.has_value(), advanced ? "frame advanced" : advanced.error().c_str());
    if (!advanced) { return std::nullopt; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  if (!engine.settled(outshine::WorldQuality::Refined)) {
    const outshine::Loading loading = engine.loading();
    Note("refined timeout ground arrived", static_cast<double>(loading.GroundArrived), "tiles");
    Note("refined timeout ground wanted", static_cast<double>(loading.GroundWanted), "tiles");
    Note("refined timeout vectors arrived", static_cast<double>(loading.VectorArrived), "tiles");
    Note("refined timeout vectors wanted", static_cast<double>(loading.VectorWanted), "tiles");
    Note("refined timeout work outstanding", static_cast<double>(loading.Outstanding), "jobs");
    if (const auto mismatch = Measure(engine, "ground candidate: revision mismatch mask")) {
      Note("ground candidate revision mismatch mask", *mismatch, "bits");
    }
    if (const auto starts = Measure(engine, "ground candidate: starts")) {
      Note("ground candidate starts", *starts, "candidates");
    }
    if (const auto quality = Measure(engine, "ground publication: quality")) {
      Note("ground publication quality", *quality, "0=playable 1=refined");
    }
    if (const auto progress = Measure(engine, "ground candidate: progress")) {
      Note("ground candidate progress", *progress, "stage index");
    }
    for (const char *name : {"ground candidate: structure tiles to certify",
                             "ground candidate: structure field ingested",
                             "buildings: tiles posted to the bake",
                             "buildings: tiles landed from it",
                             "buildings: tiles in the bake right now",
                             "buildings: tiles deferred for ground"}) {
      if (const auto value = Measure(engine, name)) { Note(name, *value, "diagnostic"); }
    }
  }
  const auto retired = engine.advance();
  CHECK(retired.has_value(), retired ? "retirement frame advanced" : retired.error().c_str());
  if (!retired) { return std::nullopt; }
  auto capture = engine.beginCapture();
  CHECK(capture.has_value() && engine.settled(outshine::WorldQuality::Refined),
        "paced advance publishes a refined capturable world");
  if (!capture || !engine.settled(outshine::WorldQuality::Refined)) { return std::nullopt; }
  Note(preload ? "preloaded published products" : "paced published products");
  for (const char *name : {"ground candidate: starts",
                           "buildings: tiles handed to the arena as pieces",
                           "buildings: triangles the tiles handed over",
                           "buildings: tiles posted to the bake",
                           "buildings: tiles landed from it",
                           "buildings: stale tiles discarded"}) {
    if (const auto value = Measure(engine, name)) { Note(name, *value, "diagnostic"); }
  }
  const auto peakBytes = Measure(engine, "ground candidate: direct CPU product peak");
  const auto retainedBytes =
      Measure(engine, "ground candidate: CPU products retained for retirement");
  const auto retirementMs = Measure(engine, "ground retirement time, most");
  CHECK(peakBytes && retainedBytes && *retainedBytes < *peakBytes,
        "completed earthworks release their scratch storage before publication");
  CHECK(retirementMs && *retirementMs >= 0.0,
        "published candidate storage enters bounded retirement on the next frame");
  ProductSignature signature{};
  for (size_t at = 0; at < kProductMeasures.size(); ++at) {
    const auto measured = Measure(engine, kProductMeasures[at]);
    CHECK(measured.has_value(), "published native product measure exists");
    if (!measured) { return std::nullopt; }
    signature[at] = *measured;
  }
  return signature;
}
}

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start");
    return Report();
  }
  const auto paced = Builds(false);
  const auto flushed = Builds(true);
  const auto repeated = Builds(false);
  CHECK(flushed.has_value() && paced.has_value(), "both pacing paths publish native products");
  CHECK(paced.has_value() && repeated.has_value() && *paced == *repeated,
        "repeated paced builds publish identical native products");
  if (flushed && paced) {
    for (size_t at = 0; at < kProductMeasures.size(); ++at) {
      const std::string claim = std::string(kProductMeasures[at]) + " is pacing-independent";
      CHECK((*flushed)[at] == (*paced)[at], claim.c_str());
      if ((*flushed)[at] != (*paced)[at]) {
        Note((std::string(kProductMeasures[at]) + " preloaded").c_str(), (*flushed)[at], "");
        Note((std::string(kProductMeasures[at]) + " paced").c_str(), (*paced)[at], "");
      }
    }
  }
  SDL_Quit();
  return Report();
}
