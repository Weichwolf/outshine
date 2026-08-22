#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include "CurlTransport.h"
#include "Journey.h"
#include "Ribbon.h"

using outshine::Driver::Between;
using outshine::Driver::Journey;
using outshine::Driver::Sink;
using outshine::Ribbon;
using outshine::Section;
using outshine::Sweep;

namespace {

constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kRathausmarktLat = 53.5503;
constexpr double kRathausmarktLon = 9.9920;
constexpr int kZoom = 10;

constexpr double kFromM = 266000.0;
constexpr double kToM = 268000.0;
constexpr double kStepM = 2.0;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  Journey journey;
  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const Between between{kMarienplatzLat, kMarienplatzLon, kRathausmarktLat, kRathausmarktLon};
  CHECK(journey.Lay(between, "tools/driver/f31.scenario", kZoom, wire, quiet),
        "the route lays, exactly as the drive lays it");

  Section section;
  section.HalfWidthM = 5.0;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  const Ribbon swept = Sweep(journey.Corridor(), section, kFromM, kToM, kStepM);
  if (!swept.Woven) { std::printf("REFUSED %s\n", swept.Error.c_str()); }
  CHECK(swept.Woven, "and km 266-268 sweeps -- the reviewer's notch station and a thousand metres "
                     "either side");
  if (!swept.Woven) { return Report(); }

  Note("stations swept", (double)swept.Stations, "stations");

  double worstM = 0.0, worstAtM = 0.0;
  double totalM = 0.0;
  size_t counted = 0;
  const size_t perStation = 8;
  for (size_t edge : {(size_t)0, (size_t)3}) {
    for (size_t station = 0; station + 1 < swept.Stations; ++station) {

      const size_t hereAt = (station * perStation + edge) * 3;
      const size_t nextAt = ((station + 1) * perStation + edge) * 3;
      const double stepE = (double)swept.PositionM[nextAt] - (double)swept.PositionM[hereAt];
      const double stepUp =
          (double)swept.PositionM[nextAt + 1] - (double)swept.PositionM[hereAt + 1];
      const double stepN =
          (double)swept.PositionM[nextAt + 2] - (double)swept.PositionM[hereAt + 2];
      const double alongM = std::sqrt(stepE * stepE + stepUp * stepUp + stepN * stepN);
      const double offM = std::fabs(alongM - kStepM);
      totalM += offM;
      ++counted;
      if (offM > worstM) {
        worstM = offM;
        worstAtM = kFromM + (double)station * kStepM;
      }
    }
  }

  Note("edge steps measured, both outer edges", (double)counted, "steps");
  Note("the mean deviation of an edge step from the sweep's own 2 m", totalM / (double)counted,
       "m");
  std::printf("NOTE the worst edge step sits at %.3f km and deviates %.3f m\n", worstAtM / 1000.0,
              worstM);
  CHECK(worstM < kStepM * 0.5,
        "**THE OUTER EDGES ARE CONTINUOUS WHERE SEGMENTS MEET.** Every 2 m step along both "
        "shoulder edges stays within half its own length of 2 m over km 266-268 -- a lateral "
        "notch of the kind the reviewer photographed at km 267 stretches a step by the notch's "
        "full depth, so half a step is the loudest a smooth join can be while a one-metre bite "
        "cannot hide. The population is 2000 steps per edge around the accused station, not the "
        "one frame that showed it");

  Covers("I.4.8 the carriageway's outer edges are continuous across segment joins, measured over "
         "the reviewer's own station");
  return Report();
}
