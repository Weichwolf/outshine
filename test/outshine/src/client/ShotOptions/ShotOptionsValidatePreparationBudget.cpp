#include "src/client/ShotOptions.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine::Test;
  using outshine::Client::ReadShotOptions;
  const auto defaults = ReadShotOptions({});
  CHECK(defaults && defaults->PreloadSeconds == 15.0 && defaults->Vegetation,
        "defaults preserve existing capture behavior");
  const char *valid[] = {
      "--rows", "--measures", "--audit", "--no-vegetation", "--preload-seconds", "180.5", "Wien"};
  const auto parsed = ReadShotOptions(valid);
  CHECK(parsed && parsed->Rows && parsed->Measures && parsed->Audit && !parsed->Vegetation &&
            parsed->PreloadSeconds == 180.5 && parsed->FirstPlace == 6 && !parsed->All,
        "options preserve place names and an explicit fractional setup budget");
  for (const char *invalid : {"", "0", "-1", "nan", "inf", "1e999", "15s", " 15", "15 "}) {
    const char *arguments[] = {"--preload-seconds", invalid};
    CHECK(!ReadShotOptions(arguments),
          "invalid preparation budget rejected before engine creation");
  }
  for (const char *option : {"--unknown", "--preload-seconds"}) {
    const char *arguments[] = {option};
    CHECK(!ReadShotOptions(arguments), "unknown or incomplete option rejected");
  }
  const char *all[] = {"--no-vegetation", "--all"};
  CHECK(ReadShotOptions(all) && ReadShotOptions(all)->All, "explicit all selection supported");
  const char *ambiguous[] = {"--all", "Wien"};
  CHECK(!ReadShotOptions(ambiguous), "all does not silently discard trailing arguments");
  return Report();
}
