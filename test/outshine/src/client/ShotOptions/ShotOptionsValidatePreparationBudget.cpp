#include "src/client/ShotOptions.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine::Test;
  using outshine::Client::ReadShotOptions;
  const auto defaults = ReadShotOptions({});
  CHECK(defaults && defaults->PreloadSeconds == 15.0 && defaults->Vegetation &&
            !defaults->Offline && defaults->CacheDirectory == "/tmp/outshine-drive-cache",
        "defaults preserve existing capture behavior");
  const char *valid[] = {"--rows",
                         "--stats",
                         "--measures",
                         "--audit",
                         "--no-vegetation",
                         "--offline",
                         "--cache-dir",
                         "/tmp/outshine-isolated-cache",
                         "--preload-seconds",
                         "180.5",
                         "Wien"};
  const auto parsed = ReadShotOptions(valid);
  CHECK(parsed && parsed->Rows && parsed->Stats && parsed->Measures && parsed->Audit &&
            !parsed->Vegetation && parsed->Offline &&
            parsed->CacheDirectory == "/tmp/outshine-isolated-cache" &&
            parsed->PreloadSeconds == 180.5 && parsed->FirstPlace == 10 && !parsed->All,
        "options preserve place names and an explicit fractional setup budget");
  for (const char *invalid : {"", "0", "-1", "nan", "inf", "1e999", "15s", " 15", "15 "}) {
    const char *arguments[] = {"--preload-seconds", invalid};
    CHECK(!ReadShotOptions(arguments),
          "invalid preparation budget rejected before engine creation");
  }
  for (const char *option : {"--unknown", "--preload-seconds", "--cache-dir"}) {
    const char *arguments[] = {option};
    CHECK(!ReadShotOptions(arguments), "unknown or incomplete option rejected");
  }
  const char *all[] = {"--no-vegetation", "--all"};
  CHECK(ReadShotOptions(all) && ReadShotOptions(all)->All, "explicit all selection supported");
  const char *ambiguous[] = {"--all", "Wien"};
  CHECK(!ReadShotOptions(ambiguous), "all does not silently discard trailing arguments");
  for (const char *bad : {"", "--offline"}) {
    const char *arguments[] = {"--cache-dir", bad};
    CHECK(!ReadShotOptions(arguments), "empty or option-like cache path is rejected");
  }
  return Report();
}
