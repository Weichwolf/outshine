#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include <Outshine.h>

#include "Check.h"
#include "Json.h"

namespace {

std::string Slurp(const std::string &path, bool &found) {
  std::ifstream file(path, std::ios::binary);
  found = file.good();
  std::stringstream held;
  held << file.rdbuf();
  return held.str();
}

}

int main(int argc, char **argv) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string under = argc > 1 ? argv[1] : std::string();
  CHECK(!under.empty(),
        "the runner was given the case directory it is to score, which is its only argument");
  if (under.empty()) { return Report(); }
  std::printf("CASE %s\n", under.c_str());

  bool haveManifest = false;
  const std::string manifestText = Slurp(under + "/manifest.json", haveManifest);
  CHECK(haveManifest, "the case carries the manifest that names its subject and its report");
  if (!haveManifest) { return Report(); }

  outshine::Json manifest;
  CHECK(manifest.Parse(manifestText.c_str(), manifestText.size()),
        "the manifest parses, so what it declares can be read");
  if (!manifest.Ok()) { return Report(); }

  const auto subject = manifest.Root()["subjects"][(size_t)0];
  const std::string entry = subject["entry"].Str();
  CHECK(!entry.empty(), "the subject names the file this case stands up");

  std::string reported;
  const auto files = subject["files"];
  for (size_t at = 0; at < files.Size(); ++at) {
    if (files[at]["role"].StrEquals("report")) { reported = files[at]["as"].Str(); }
  }
  CHECK(!reported.empty(), "the subject carries the validator report that is this case's oracle");
  if (entry.empty() || reported.empty()) { return Report(); }

  bool haveReport = false;
  const std::string reportText = Slurp(under + "/" + reported, haveReport);
  CHECK(haveReport, "the report was prepared beside the asset");
  if (!haveReport) { return Report(); }

  outshine::Json report;
  CHECK(report.Parse(reportText.c_str(), reportText.size()), "Khronos's report parses");
  if (!report.Ok()) { return Report(); }

  const int errors = report.Root()["issues"]["numErrors"].Int(-1);
  CHECK(errors >= 0, "the report states how many errors the validator found");
  if (errors < 0) { return Report(); }
  std::printf("VALIDATOR %d error(s), %d warning(s)\n", errors,
              report.Root()["issues"]["numWarnings"].Int(0));

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start, so no reader can be stood up to judge");
    return Report();
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{under, "src/assets", "/tmp/outshine-refuse-cache", true});
  if (!engine.DrawsInto(outshine::Extent{64, 64})) {
    Unprepared("the device did not stand a canvas, so nothing can be read into it");
    return Report();
  }

  outshine::Scenario stands;
  stands.Render.Declared = true;
  stands.Render.Frame = outshine::Extent{64, 64};
  outshine::Asset shown;
  shown.Uri = entry;
  shown.Kind = "gltf";
  stands.Assets.push_back(shown);

  const bool stood = engine.Declare(stands) && engine.Assemble();
  const std::string why = engine.Error();
  std::printf("OUTSHINE %s%s%s\n", stood ? "stood" : "refused", stood ? "" : ": ",
              stood ? "" : why.c_str());

  if (errors > 0) {
    CHECK(!stood,
          "**AN ASSET THE VALIDATOR ERRORS ON IS REFUSED**: glTF 2.0 is the only content surface "
          "and a reader that stands a malformed asset up has decided the spec does not apply to "
          "it -- Khronos's own report is the oracle and it does not depend on our design");
    CHECK(!stood || !why.empty(), "a refusal carries a reason a caller could act on");
  } else {
    CHECK(stood,
          "**AN ASSET THE VALIDATOR PASSES STANDS**: a reader that refuses a conformant asset "
          "fails the same spec from the other side, and a corpus that only proves refusals "
          "would be satisfied by refusing everything");
  }

  Covers("gltf-2.0 conformance at the door: an asset Khronos's validator errors on is refused "
         "with a reason, and one it passes stands");
  return Report();
}
