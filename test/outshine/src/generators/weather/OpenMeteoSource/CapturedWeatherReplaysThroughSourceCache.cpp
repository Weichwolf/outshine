#include "Check.h"
#include "WeatherCapture.h"
#include "SourceConfiguration.h"
#include "OfflineTransport.h"

#include <array>
#include <cstdlib>
#include <cstdint>
#include <expected>
#include <vector>
#include <filesystem>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
using namespace outshine::Data;

}

int main() {
  using namespace outshine::Test;
  const auto capture = RecordedWeatherCapture();
  CHECK(capture.has_value(), "the captured response and request metadata exist");
  if (!capture) { return Report(); }
  const outshine::Generators::Weather::OpenMeteoProvider provider(capture->Request);
  ProviderRegistry registry;
  CHECK(registry.registerProvider(provider).has_value(), "public weather factory is registered");
  const std::array declarations{capture->Declaration};
  auto directory =
      (std::filesystem::temp_directory_path() / "outshine-weather-source-XXXXXX").string();
  const bool created = mkdtemp(directory.data()) != nullptr;
  CHECK(created, "isolated existing source-cache storage created");
  if (!created) { return Report(); }
  const ContentStore::Config config{.Directory = directory};
  const Fetch request(DataKind::Weather, Address::Whole(0));
  {
    ContentStore store(config);
    SourceSet sources(store);
    std::string error;
    CHECK(RegisterSources(sources, declarations, {}, error, registry), error.c_str());
    if (sources.Count() != 1) { return Report(); }
    CapturedWeatherTransport transport(*capture);
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, transport).Take();
    CHECK(answer && answer->Bytes.size() == capture->Bytes.size() && transport.Begins() == 1 &&
              sources.Counters().ProviderStarts == 1 && store.Counters().Writes == 1,
          "ordinary SourceSet/Transport acquisition stores the original captured bytes once");
    if (answer) {
      const auto snapshot = sources.At(0).DecodeWeather(answer->Bytes, kCapturedWeatherUtcS);
      CHECK(snapshot && *snapshot && (**snapshot).Values.CloudLow == 0.0,
            "delivered original bytes produce an exact-time public snapshot");
    }
    CHECK(sources.At(0).Covers(Fetch(DataKind::Weather, Address::Whole(1))) == Coverage::Outside &&
              sources.At(0).Covers(Fetch(DataKind::Elevation, Address::Whole(0))) ==
                  Coverage::Outside,
          "a capture never claims a different address or native category");
    auto abandoned = sources.Ask(request);
    SourceSet::Abandon(abandoned, transport);
    CHECK(transport.Cancels() == 0, "unstarted queries own no transport ticket");
  }
  {
    ContentStore store(config);
    SourceSet sources(store);
    std::string error;
    CHECK(RegisterSources(sources, declarations, {}, error, registry), error.c_str());
    if (sources.Count() != 1) { return Report(); }
    OfflineTransport transport;
    auto query = sources.Ask(request);
    const auto answer = sources.Collect(query, transport).Take();
    CHECK(answer && sources.Counters().ProviderStarts == 0 && sources.Counters().FromStore == 1 &&
              store.Counters().Hits == 1,
          "fresh source ownership replays the same cache with no provider or network start");
    if (answer) {
      const auto snapshot = sources.At(0).DecodeWeather(answer->Bytes, kCapturedWeatherUtcS);
      const auto missing = sources.At(0).DecodeWeather(answer->Bytes, kCapturedWeatherUtcS + 1);
      CHECK(snapshot && *snapshot && (**snapshot).Values.CloudLow == 0.0 &&
                (**snapshot).Source.PayloadSha256 == capture->Declaration.PayloadSha256 &&
                missing && !*missing,
            "offline replay preserves provenance, known zero and exact missing-hour semantics");
    }
    CHECK(sources.Counters().Providers[static_cast<size_t>(DataKind::Weather)].Calls == 0,
          "weather has a valid independent ledger slot and cache hits perform no provider work");
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated test-owned source-cache files are removed");
  return Report();
}
