#include <cstdio>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"
#include "PreparedRoot.h"

namespace {

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

std::string WriteScenario(const std::string &stands) {
  const std::string path = outshine::Test::PreparedRoot() + "/four-lines.scenario";
  std::FILE *const file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::string text = "{\n";
  text += "  \"schema\": \"outshine/scenario\",\n";
  text += "  \"frame\": { \"widthPx\": 320, \"heightPx\": 240 },\n";
  text += "  \"stands\": \"" + stands + "\",\n";
  text += "  \"fps\": 30,\n";
  text += "  \"fill\": 0.9,\n";
  text += "  \"key\": { \"lux\": 40000, \"elevationDeg\": 45, \"bearingDeg\": 135 },\n";
  text += "  \"environment\": [0.05, 0.06, 0.08]\n";
  text += "}\n";
  std::fwrite(text.data(), 1, text.size(), file);
  std::fclose(file);
  return path;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string stands =
      EntryPath(PreparedRoot() + "/" + kPreparedKhronosPrefix + "BoxAnimated");
  CHECK(!stands.empty(), "the scenario's subject is in the prepared corpus");
  if (stands.empty()) { return Report(); }
  const std::string scenario = WriteScenario(stands);
  CHECK(!scenario.empty(), "the scenario is written where the client can read it");
  if (scenario.empty()) { return Report(); }
  std::printf("NOTE the scenario stands %s\n", stands.c_str());

  outshine::Engine engine;
  engine.RenderTo({1280, 720});
  const bool loaded = engine.Load(scenario);
  const bool ran = loaded && engine.Run();

  if (!loaded) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(loaded, "a client stands a scenario up out of a file, in one call and with no engine type in "
                "its hands");
  if (!ran && loaded) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  CHECK(ran, "and runs it to the end of its declared grid");

  std::printf("NOTE the run advanced %d frames of %d\n", engine.At(), engine.Frames());
  CHECK(engine.Frames() > 1, "an animated scenario declares more than one frame");
  CHECK(engine.Standing(), "and the engine is still standing when the run is over");

  outshine::Scenario declared;
  declared.Frame = {320, 240};
  declared.Stands = stands;
  declared.Fps = 30.0;
  declared.Fill = 0.9;
  declared.Key = {40000.0, 45.0, 135.0};
  outshine::Engine second;
  const bool inCode = second.Declare(declared);
  if (!inCode) { std::printf("REFUSED %s\n", second.Error().c_str()); }
  CHECK(inCode, "a scenario declared in code stands up the same way a file does");
  CHECK(!inCode || second.Frames() == engine.Frames(),
        "and it is the same scenario, so it declares the same number of frames");

  outshine::Engine empty;
  const bool nothing = empty.Advance();
  CHECK(!nothing, "an engine with no scenario refuses to advance");
  CHECK(!empty.Error().empty(), "and says why rather than returning quietly");
  std::printf("NOTE the empty engine says: %s\n", empty.Error().c_str());

  Covers("I.4 a client says where to render, loads a scenario, runs it and cleans up -- and reaches "
         "nothing of the engine but the handle it was given");
  return Report();
}
