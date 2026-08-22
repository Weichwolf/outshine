#include <cstdio>
#include <cstdlib>
#include <string>

#include <outshine/Outshine.h>

#include "Check.h"

namespace {

std::string Planted(const char *name, const char *text) {
  const char *tmp = std::getenv("TMPDIR");
  std::string at = (tmp != nullptr ? std::string(tmp) : std::string("/tmp"));
  if (!at.empty() && at.back() == '/') { at.pop_back(); }
  at += std::string("/") + name;
  std::FILE *const file = std::fopen(at.c_str(), "wb");
  if (file == nullptr) { return std::string(); }
  std::fputs(text, file);
  std::fclose(file);
  return at;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string malformed =
      Planted("outshine-malformed.scenario", "<scenario><world radius=\"not a number\"</scenario>");
  CHECK(!malformed.empty(), "a malformed scenario is planted in the temp dir");

  outshine::Engine reader;
  CHECK(!reader.Read(malformed), "Read refuses the malformed file");
  const std::string readWhy = reader.Error();
  std::printf("NOTE Read says: %s\n", readWhy.c_str());

  outshine::Engine loader;
  CHECK(!loader.Load(malformed), "and Load refuses it too");
  std::printf("NOTE Load says: %s\n", loader.Error().c_str());
  CHECK(!readWhy.empty() && loader.Error() == readWhy,
        "**READ AND LOAD REFUSE THE SAME FILE WITH THE SAME WORDS** -- both verbs consume the "
        "one ReadInto, so a fix to the slurp or the parse cannot land in one door and silently "
        "miss the other (board:1628)");

  outshine::Engine absent;
  CHECK(!absent.Read("no/such/place.scenario") &&
            absent.Error().find("no scenario at that path") != std::string::npos,
        "a missing file refuses by naming the path, not by parsing nothing");

  Covers("I.26.11 the engine reads a scenario file once, in one place: Read and Load share "
         "ReadInto, proven by identical refusal text on the same malformed file");
  return Report();
}
