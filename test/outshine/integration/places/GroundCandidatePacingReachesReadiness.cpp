#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <thread>

int main() {
  using namespace outshine::Test;
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared("SDL did not start");
    return Report();
  }
  outshine::Engine e;
  if (!e.setRoots({"src/assets/drive", "src/assets", "/tmp/outshine-paced", false}) ||
      !e.drawsInto({160, 90})) {
    Unprepared("the offscreen target did not start");
    return Report();
  }
  outshine::Scenario::Document s;
  s.Ground.Declared = true;
  s.Ground.VegetationEnabled = false;
  s.Ground.Origin.LatitudeDeg = 49.3777;
  s.Ground.Origin.LongitudeDeg = 10.179;
  s.Ground.PatienceS = 3;
  s.Ground.SightM = 8000;
  s.Render.Declared = true;
  s.Render.Frame = {160, 90};
  s.Lit.Declared = true;
  outshine::Scenario::View v;
  v.Id = "v";
  v.Person = "first";
  v.Placement = outshine::Scenario::CameraPlacement::Geodetic;
  v.Geographic.Geodetic.LatitudeDeg = 49.3777;
  v.Geographic.Geodetic.LongitudeDeg = 10.179;
  v.Geographic.Geodetic.HeightM = 1.7;
  v.Geographic.SamplesHeight = false;
  v.Sees.FovDeg = 55;
  s.Views.push_back(v);
  const auto declared = e.declare(s);
  CHECK(declared.has_value(), declared ? "scenario declared" : declared.error().c_str());
  if (!declared) { return Report(); }
  const auto assembled = e.assemble();
  CHECK(assembled.has_value(), assembled ? "scenario assembled" : assembled.error().c_str());
  if (!assembled) { return Report(); }
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);

  bool captured = false;
  while (std::chrono::steady_clock::now() < end) {
    const auto advanced = e.advance();
    CHECK(advanced.has_value(), advanced ? "frame advanced" : advanced.error().c_str());
    if (!advanced) { return Report(); }
    if (auto capture = e.beginCapture()) {
      captured = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(captured && e.settled(), "paced advance publishes a capturable world");
  return Report();
}
