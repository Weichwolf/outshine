#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include "Journey.h"

using outshine::Driver::Between;
using outshine::Driver::Journey;
using outshine::Driver::Ridden;
using outshine::Driver::Sink;

namespace {

constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kRathausmarktLat = 53.5503;
constexpr double kRathausmarktLon = 9.9920;
constexpr int kZoom = 10;
constexpr double kStepS = 1.0e-3;
constexpr long kMostSteps = 40000000;

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
  const Between between{kMarienplatzLat, kMarienplatzLon, kRathausmarktLat, kRathausmarktLon};

  const bool laid = journey.Lay(between, "tools/driver/f31.scenario", kZoom, harness);
  CHECK(laid, "**THE ROAD FROM MARIENPLATZ TO RATHAUSMARKT IS LAID.** A route over ways fetched "
              "live, a corridor fitted through them, the real ground under it shaped to each road "
              "class's own grade, and the declared F31 standing on it -- and every one of those "
              "numbers reported through a SINK, so the windowed driver runs the identical code and "
              "judges nothing");
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
  Note("where that was", rode.WorstOffsetAtM / 1000.0, "km");
  Note("worst share of a contact's grip used", rode.WorstRatio, "of it");
  Note("most mounts off the ground at once", (double)rode.MostAirborne, "of 4");
  Note("where a contact first went past its limit", rode.BrokeAtM / 1000.0, "km");
  Note("where a wheel first left the carriageway", rode.LeftTheRoadAtM / 1000.0, "km");

  CHECK(!rode.Lost, "the car never left the corridor's own window");
  CHECK(!rode.PastLimit,
        "**AND NO CONTACT WENT PAST ITS DECLARED LIMIT.** A crash on this route is READ -- a load "
        "past what the link carries -- and there was none");
  CHECK(rode.LeftTheRoadAtM <= 0.0,
        "and no wheel ever left the carriageway, which is the road's declared width and not a line");
  CHECK(rode.Arrived,
        "**THE F31 DROVE ITSELF FROM MARIENPLATZ TO RATHAUSMARKT.** Two coordinates in, a route "
        "planned over live OSM ways, a corridor fitted through them, the real ground under it, and "
        "the declared car carried the whole way by four compliant contacts and nothing else");

  journey.Close();

  Covers("I.4.5 the F31 drives itself from Marienplatz to Rathausmarkt over ways fetched live from "
         "the declared OSM source and ground from the declared elevation source, on four compliant "
         "contacts, headless with no renderer linked at all -- and the driving is a shared "
         "translation unit that reports through a sink, so a windowed driver runs the same code");
  return Report();
}
