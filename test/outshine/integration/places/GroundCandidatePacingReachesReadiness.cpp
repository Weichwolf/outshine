#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <thread>
#include <cstdio>

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
  CHECK(e.declare(s) && e.assemble(), "paced engine declares and assembles the ground scenario");
  auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
  int ok = 0, fail = 0;
  bool captured = false;
  while (std::chrono::steady_clock::now() < end) {
    if (e.advance()) {
      ++ok;
    } else {
      ++fail;
    }
    if (auto capture = e.beginCapture()) {
      captured = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::printf("settled=%d captured=%d ok=%d fail=%d load=%f\n",
              e.settled(),
              captured,
              ok,
              fail,
              e.loadProgress());
  CHECK(captured && e.settled(), "paced advance publishes a capturable world");
  return Report();
}
