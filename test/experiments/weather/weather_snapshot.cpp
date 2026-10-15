#include "WeatherCapture.h"
#include "SourceConfiguration.h"
#include "OfflineTransport.h"
#include "LowCloud.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>

namespace {
void Number(const std::optional<double> &value) {
  if (value) {
    std::print("{:.17g}", *value);
  } else {
    std::print("null");
  }
}

int Consume(const outshine::Data::SourceSet &sources,
            const outshine::Data::Delivery::Answer &answer,
            int64_t utcS) {
  const auto snapshot = sources.At(0).DecodeWeather(answer.Bytes, utcS);
  if (!snapshot) { return 2; }
  if (!*snapshot) { return 3; }
  const auto low = outshine::Generators::Weather::MakeLowCloud(**snapshot);
  if (!low) { return 2; }
  std::optional<double> optical;
  if (*low) {
    constexpr double probeOriginAslM = 2000.0;
    optical = outshine::CloudOpticalDepth(
        (**low).Layer, {{0.0, probeOriginAslM, 0.0}}, {{0.0, 1.0, 0.0}});
  }
  std::print(R"({{"valid_utc_s":{},"source_sha256":"{}","low_cloud_cover":)",
             (**snapshot).Values.ValidUtcS,
             (**snapshot).Source.PayloadSha256);
  Number((**snapshot).Values.CloudLow);
  std::print(",\"native_cloud_optical_depth\":");
  Number(optical);
  std::print(",\"provider_starts\":{},\"source_cache_hits\":{}}}\n",
             sources.Counters().ProviderStarts,
             sources.Counters().FromStore);
  return 0;
}
}

int main(int argc, char **argv) {
  if ((argc != 3 && argc != 4) || std::string_view(argv[1]).empty() ||
      (argc == 4 && std::string_view(argv[3]) != "prepare")) {
    std::println(stderr, "usage: weather-snapshot <isolated-cache-dir> <exact-unix-utc> [prepare]");
    return 2;
  }
  int64_t utcS = 0;
  const std::string_view time(argv[2]);
  const auto parsed = std::from_chars(time.data(), time.data() + time.size(), utcS);
  if (parsed.ec != std::errc{} || parsed.ptr != time.data() + time.size()) { return 2; }
  const bool prepare = argc == 4;
  const auto capture = outshine::Test::RecordedWeatherCapture(prepare);
  if (!capture) { return 2; }
  const outshine::Generators::Weather::OpenMeteoProvider provider(capture->Request);
  outshine::Data::ProviderRegistry registry;
  if (!registry.registerProvider(provider)) { return 2; }
  outshine::Data::ContentStore store({.Directory = argv[1]});
  outshine::Data::SourceSet sources(store);
  std::string error;
  const std::array declarations{capture->Declaration};
  if (!outshine::Data::RegisterSources(sources, declarations, {}, error, registry)) {
    std::println(stderr, "{}", error);
    return 2;
  }
  sources.Seal();
  outshine::Test::CapturedWeatherTransport replay(*capture);
  outshine::Data::OfflineTransport offline;
  outshine::Data::Transport &transport = prepare
                                             ? static_cast<outshine::Data::Transport &>(replay)
                                             : static_cast<outshine::Data::Transport &>(offline);
  auto query = sources.Ask(
      outshine::Data::Fetch(outshine::Data::DataKind::Weather, outshine::Data::Address::Whole(0)));
  const auto answer = sources.Collect(query, transport).Take();
  if (!answer) { return 4; }
  return Consume(sources, *answer, utcS);
}
