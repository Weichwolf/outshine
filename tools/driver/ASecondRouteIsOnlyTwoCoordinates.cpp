#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include "CurlTransport.h"
#include "Journey.h"

using outshine::Sim::Between;
using outshine::Sim::Journey;
using outshine::Sim::Ridden;
using outshine::Sim::Sink;

namespace {

constexpr double kKyotoStationLat = 34.9855;
constexpr double kKyotoStationLon = 135.7581;
constexpr double kOsakaCastleLat = 34.6873;
constexpr double kOsakaCastleLon = 135.5262;
constexpr int kZoom = 10;
constexpr double kStepS = 1.0e-3;
constexpr long kMostSteps = 10000000;

class Harness : public Sink {
public:
  void Number(const char *what, double value, const char *unit) override {
    outshine::Test::Note(what, value, unit);
  }
  void Claim(bool held, const char *why) override {
    outshine::Test::Checked(held, "the journey", why, __FILE__, __LINE__);
  }
  void Near(double got, double want, double within, const char *unit, const char *why) override {
    outshine::Test::CheckedNear(got, want, within, unit, why, "the journey", __FILE__, __LINE__);
  }
  void Say(const std::string &line) override { std::printf("%s\n", line.c_str()); }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Harness harness;
  Journey journey;
  const Between between{kKyotoStationLat, kKyotoStationLon, kOsakaCastleLat, kOsakaCastleLon};

  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const bool laid = journey.Lay(between, "tools/driver/f31.scenario", kZoom, wire, harness);
  CHECK(laid,
        "**A SECOND ROUTE IS ONLY TWO COORDINATES.** Kyoto Station to Osaka Castle -- another "
        "continent, another road network, the same code, the same declaration, and NOTHING "
        "route-specific anywhere: the planner finds the ways, the corridor is fitted through "
        "them, the ground shaped under them, exactly as route 1 of board:1524's hundred. This is "
        "board:1573's first box: worldwide means the second route costs a coordinate pair");
  if (!laid) { return Report(); }

  const auto began = std::chrono::steady_clock::now();
  Ridden rode;
  for (long step = 0; step < kMostSteps; ++step) {
    rode = journey.Ride(kStepS);
    if (!rode.Found || rode.Arrived || rode.Lost || rode.PastLimit || rode.OffTheRoad) { break; }
  }
  const double wallS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  Note("how far the car drove", rode.ReachedM / 1000.0, "km");
  Note("of a corridor this long", journey.LengthM() / 1000.0, "km");
  Note("the fastest it went", rode.TopMs * 3.6, "km/h");
  Note("hours simulated", rode.SimulatedS / 3600.0, "h");
  Note("seconds of wall clock", wallS, "s");
  Note("how much faster than real time", rode.SimulatedS / (wallS > 0.0 ? wallS : 1.0), "x");
  Note("worst deviation from the middle of its own lane", rode.WorstOffsetM, "m");

  CHECK(rode.Found && !rode.Lost && !rode.PastLimit && !rode.OffTheRoad,
        "the drive ended by arriving or by the corridor's own end, never by losing the road, "
        "breaking a contact limit or leaving the lane");
  CHECK(rode.ReachedM > journey.LengthM() * 0.999,
        "**AND THE F31 DROVE ITSELF FROM KYOTO TO OSAKA.** The same headless Ride, the same "
        "physics, the same speed plan from the geometry alone -- on ways it had never seen. "
        "Worldwide is a measurement now, not a word in a vision file");

  Covers("I.4.9 a second route anywhere on earth is declared by two coordinates alone and driven "
         "by the same code that drove the first");
  return Report();
}
