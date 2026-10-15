#include "Check.h"
#include "WeatherCapture.h"
#include "LowCloud.h"

#include <array>
#include <limits>
#include <string>
#include <utility>

namespace {
class WorkingTransport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin([[maybe_unused]] const std::string &url) override {
    Active = static_cast<outshine::Data::Ticket>(++Begins);
    return Active;
  }

  outshine::Data::Wire Collect([[maybe_unused]] outshine::Data::Ticket ticket) override {
    return outshine::Data::Wire::Working();
  }

  void Cancel(outshine::Data::Ticket ticket) override {
    if (ticket == Active && Active != outshine::Data::Ticket::None) {
      ++Cancels;
      Active = outshine::Data::Ticket::None;
    }
  }

  int Begins = 0;
  int Cancels = 0;
  outshine::Data::Ticket Active = outshine::Data::Ticket::None;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  const auto capture = RecordedWeatherCapture();
  CHECK(capture.has_value(), "versioned forecast fixture is available");
  if (!capture) { return Report(); }
  const OpenMeteoProvider provider(capture->Request);
  auto source = [&capture] {
    const OpenMeteoProvider temporary(capture->Request);
    return temporary.make(capture->Declaration, {});
  }();
  CHECK(source.has_value(), "owned source copies factory inputs without IO");
  if (!source) { return Report(); }
  WorkingTransport transport;
  const auto ticket = (*source)->Begin(Data::Address::Whole(0), transport);
  CHECK(ticket && transport.Begins == 1 &&
            (*source)->Collect(Data::Address::Whole(0), *ticket, transport).Where() ==
                Data::Fetched::State::Working,
        "pending acquisition stays pending under the existing transport lifetime");
  if (ticket) { (*source)->Cancel(*ticket, transport); }
  CHECK(transport.Cancels == 1, "source cancellation releases the original transport ticket");
  const auto unsupported = (*source)->DecodeElevation(WeatherBytes(*capture));
  CHECK(!unsupported && unsupported.error() == Data::DecodeFailure::Unsupported,
        "a weather provider does not pretend to decode other native products");
  for (const bool endpoint : {false, true}) {
    auto bad = capture->Declaration;
    if (endpoint) {
      bad.Endpoint = "https://different.invalid/forecast";
    } else {
      bad.PayloadSha256.clear();
    }
    CHECK(!provider.make(bad, {}),
          "invalid request endpoint or missing original-byte pin is rejected");
  }
  auto badRequest = capture->Request;
  badRequest.Requested.LatitudeDeg = std::numeric_limits<double>::quiet_NaN();
  CHECK(!OpenMeteoProvider(std::move(badRequest)).make(capture->Declaration, {}),
        "invalid typed source coordinates cannot produce a configured source");
  WeatherSnapshot declared;
  declared.Source.Origin = WeatherOrigin::ScenarioDeclaration;
  declared.Values.CloudTotal = 1.0;
  CHECK(MakeLowCloud(declared) && !*MakeLowCloud(declared),
        "known total cloud does not fill unknown low cover from declaration defaults");
  declared.Values.CloudLow = std::numeric_limits<double>::quiet_NaN();
  CHECK(!MakeLowCloud(declared) && MakeLowCloud(declared).error() == LowCloudFailure::InvalidCover,
        "nonfinite public aggregate input remains an explicit error");
  declared.Values.CloudLow = kDeclaredLowCover;
  LowCloudRecipe recipe{.BaseAslM = kDeclaredCloudBaseAslM,
                        .ThicknessM = kDeclaredCloudThicknessM,
                        .ExtinctionPerM = kDeclaredCloudExtinctionPerM,
                        .AdvectionEastMs = 0.0,
                        .AdvectionNorthMs = -kDeclaredCloudAdvectionMs};
  const auto native = MakeLowCloud(declared, recipe);
  CHECK(native && *native && (**native).Layer.WindMs[2] == kDeclaredCloudAdvectionMs,
        "explicit northward-axis advection crosses RenderFrame exactly once");
  recipe.ThicknessM = 0.0;
  CHECK(!MakeLowCloud(declared, recipe) &&
            MakeLowCloud(declared, recipe).error() == LowCloudFailure::InvalidRecipe,
        "zero-thickness geometry is rejected before density integration");
  const auto publication = (*source)->DecodeWeather(WeatherBytes(*capture), kCapturedWeatherUtcS);
  source->reset();
  CHECK(publication && *publication &&
            (**publication).Source.RequestUri == capture->Declaration.Endpoint &&
            (**publication).Source.SourceId == capture->Declaration.Dataset &&
            (**publication).Values.CloudLow == 0.0,
        "snapshot remains owned after both its temporary factory and configured source die");
  return Report();
}
