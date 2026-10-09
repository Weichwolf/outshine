#include "src/client/ShotOptions.h"
#include "src/client/SourceCache.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine::Test;
  using outshine::Client::ReadShotOptions;
  const auto defaults = ReadShotOptions({});
  CHECK(defaults && defaults->PreloadSeconds == 10.0 && !defaults->Offline &&
            defaults->CacheDirectory.empty(),
        "default cache is resolved from persistent platform storage at client startup");
  const auto first = outshine::Client::WithSourceCache({});
  const auto second = outshine::Client::WithSourceCache({});
  CHECK(first && second && !first->Cache.empty() && first->Cache == second->Cache,
        "separate client roots select the same persistent source cache");
  const auto explicitRoots = outshine::Client::WithSourceCache(
      {.Assets = "assets", .Shipped = "shipped", .Cache = "/tmp/explicit-cache", .Offline = true});
  CHECK(explicitRoots && explicitRoots->Assets == "assets" && explicitRoots->Shipped == "shipped" &&
            explicitRoots->Cache == "/tmp/explicit-cache" && explicitRoots->Offline,
        "explicit cache, library roots and offline policy are preserved");
  const char *valid[] = {"--rows",
                         "--stats",
                         "--measures",
                         "--audit",
                         "--offline",
                         "--cache-dir",
                         "/tmp/outshine-isolated-cache",
                         "--preload-seconds",
                         "180.5",
                         "Wien"};
  const auto parsed = ReadShotOptions(valid);
  CHECK(parsed && parsed->Rows && parsed->Stats && parsed->Measures && parsed->Audit &&
            parsed->Offline && parsed->CacheDirectory == "/tmp/outshine-isolated-cache" &&
            parsed->PreloadSeconds == 180.5 && parsed->FirstPlace == 9 && !parsed->All,
        "options preserve place names and an explicit fractional setup budget");
  for (const char *invalid : {"", "0", "-1", "nan", "inf", "1e999", "15s", " 15", "15 "}) {
    const char *arguments[] = {"--preload-seconds", invalid};
    CHECK(!ReadShotOptions(arguments),
          "invalid preparation budget rejected before engine creation");
  }
  for (const char *option : {"--unknown", "--preload-seconds", "--cache-dir", "--no-vegetation"}) {
    const char *arguments[] = {option};
    CHECK(!ReadShotOptions(arguments), "unknown or incomplete option rejected");
  }
  const char *all[] = {"--all"};
  CHECK(ReadShotOptions(all) && ReadShotOptions(all)->All, "explicit all selection supported");
  const char *overrides[] = {"--scenario-overrides", "{\"world\":{\"vegetation\":false}}", "Wien"};
  const auto overridden = ReadShotOptions(overrides);
  CHECK(overridden && overridden->FirstPlace == 2 && overridden->ScenarioOverrides == overrides[1],
        "scenario overrides remain borrowed and preserve place selection");
  for (const char *bad : {"", "[]", "--offline"}) {
    const char *arguments[] = {"--scenario-overrides", bad};
    CHECK(!ReadShotOptions(arguments), "override option requires a JSON object");
  }
  const char *duplicate[] = {"--scenario-overrides", "{}", "--scenario-overrides", "{}"};
  CHECK(!ReadShotOptions(duplicate),
        "duplicate override arguments do not silently replace each other");
  const char *ambiguous[] = {"--all", "Wien"};
  CHECK(!ReadShotOptions(ambiguous), "all does not silently discard trailing arguments");
  for (const char *bad : {"", "--offline"}) {
    const char *arguments[] = {"--cache-dir", bad};
    CHECK(!ReadShotOptions(arguments), "empty or option-like cache path is rejected");
  }
  return Report();
}
