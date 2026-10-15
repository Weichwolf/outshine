#include "EngineHeld.h"
#include "CloudLayer.h"

#include <cmath>
#include <optional>

namespace outshine {

void Engine::State::UpdateWeather() {
  if (!Picture.Standing) { return; }
  const auto &weather = Session.Declared.Ground.Sky;
  CloudLayer layer;
  bool known = false;
  bool inside = false;
  if (Session.Declared.Ground.Declared && weather.CloudCover > 0.0) {
    const LongitudeLatitude reference{.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                      .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg};
    std::optional<double> height = World.Sheets.HeightMAt(reference);
    if (!height && World.Stack.Opened()) {
      height = World.Stack.Ground().Resident(reference).AslM();
    }
    known = height && std::isfinite(*height);
    if (known) {
      const double baseM = *height + weather.CloudBaseAglM;
      constexpr double metresPerKm = 1000.0;
      constexpr double atmosphereHeightM =
          (kEarthAir.TopRadiusKm - kEarthAir.BottomRadiusKm) * metresPerKm;
      inside = std::isfinite(baseM) && baseM >= -layer.ThicknessM &&
               baseM <= atmosphereHeightM - layer.ThicknessM;
      if (inside) {
        layer.BaseM = baseM;
        layer.Cover = weather.CloudCover;
        layer.WindMs = CloudWindFrom({.FromDeg = weather.WindDeg, .SpeedMs = weather.WindMs});
      }
    }
  }
  Published.RecordMetric("weather: reference ground height known", known ? 1.0 : 0.0, "boolean");
  Published.RecordMetric("weather: cloud shell within atmosphere", inside ? 1.0 : 0.0, "boolean");
  Picture.Standing->SetClouds(layer, Ticking.ElapsedS);
}

}
