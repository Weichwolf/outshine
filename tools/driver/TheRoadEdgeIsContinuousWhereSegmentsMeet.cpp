#include <cmath>
#include <cstdio>
#include <string>

#include "Check.h"

#include "CurlTransport.h"
#include <outshine/Assembled.h>
#include <outshine/Column.h>
#include <outshine/Store.h>

#include "Assembly.h"
#include "DriveAssembly.h"
#include "DeclaredSources.h"
#include "GroundStack.h"
#include "ScenarioRead.h"
#include "Ribbon.h"

using outshine::Sim::AssembleDrive;
using outshine::Sim::DriveProduct;
using outshine::Ground::GroundStack;
using outshine::Sink;
using outshine::Ribbon;
using outshine::Section;
using outshine::Sweep;

namespace {

constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kRathausmarktLat = 53.5503;
constexpr double kRathausmarktLon = 9.9920;
constexpr int kZoom = 10;

constexpr double kWindows[][2] = {{266000.0, 268000.0}, {16800.0, 18800.0}, {707000.0, 709000.0}};
constexpr double kStepM = 2.0;

class Quiet : public Sink {
public:
  void Number(const char *, double, const char *) override {}
  void Claim(bool, const char *) override {}
  void Near(double, double, double, const char *, const char *) override {}
  void Say(const std::string &) override {}
};

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Quiet quiet;
  GroundStack stack;
  DriveProduct drive;
  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  std::string scenarioText;
  {
    std::FILE *const file = std::fopen("tools/driver/f31.scenario", "rb");
    if (file != nullptr) {
      int one = 0;
      while ((one = std::fgetc(file)) != EOF) { scenarioText.push_back((char)one); }
      std::fclose(file);
    }
  }
  outshine::Scenario declared;
  std::string readError;
  if (!outshine::ReadScenario(scenarioText.data(), scenarioText.size(), declared, readError)) {
    std::printf("REFUSED %s\n", readError.c_str());
    return Report();
  }
  declared.Driven.Declared = true;
  declared.Driven.FromLatDeg = kMarienplatzLat;
  declared.Driven.FromLonDeg = kMarienplatzLon;
  declared.Driven.ToLatDeg = kRathausmarktLat;
  declared.Driven.ToLonDeg = kRathausmarktLon;
  declared.Driven.Zoom = kZoom;
  outshine::Store scene;
  outshine::Column<outshine::Vehicle> vehicles;
  outshine::Column<outshine::Drive> drives;
  outshine::Column<outshine::Traits> kinds;
  outshine::Assembled stood;
  if (!scene.Open(outshine::AssembledCapacity(declared)) || !vehicles.Open(scene) ||
      !drives.Open(scene) ||
      !kinds.Open(scene) ||
      !outshine::Assemble(declared, scene, vehicles, drives, kinds, stood, readError)) {
    std::printf("REFUSED %s\n", readError.c_str());
    return Report();
  }
  CHECK(AssembleDrive(scene, stood, vehicles, drives, declared.Ground, stack, wire,
      outshine::Sim::Provision{"/tmp/outshine-drive-cache", "src/assets", {outshine::Data::ShippedProviders().begin(), outshine::Data::ShippedProviders().end()}}, quiet, drive),
        "the route lays, exactly as the drive lays it");

  Section section;
  section.HalfWidthM = 5.0;
  section.ShoulderM = 2.5;
  section.ThicknessM = 0.35;
  for (const auto &window : kWindows) {
    const Ribbon swept = Sweep(drive.Way.Line, section, window[0], window[1], kStepM);
    if (!swept.Woven) { std::printf("REFUSED %s\n", swept.Error.c_str()); }
    CHECK(swept.Woven, "the window sweeps");
    if (!swept.Woven) { continue; }

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
          worstAtM = window[0] + (double)station * kStepM;
        }
      }
    }
    std::printf("NOTE window %.0f-%.0f km: %zu steps, mean %.4f m, worst %.3f m at %.3f km\n",
                window[0] / 1000.0, window[1] / 1000.0, counted, totalM / (double)counted, worstM,
                worstAtM / 1000.0);
    CHECK(worstM < kStepM * 0.5,
          "**THE OUTER EDGES ARE CONTINUOUS AT EVERY ACCUSED STATION.** km 266-268 was round "
          "two's notch, km 16.8-18.8 is round ten's seven-tear sawtooth, km 707-709 is the "
          "vanishing-point zigzag three cameras agree on -- if any of them lives in the SWEPT "
          "polyline, this window says so with a number; if all three windows are smooth, the "
          "defects live in the GRADED VERGE against a smooth edge, which is the other half of "
          "board:1568's statement");
  }

    Covers("I.4.8 the carriageway's outer edges are continuous across segment joins, measured over "
         "the reviewer's own station");
  return Report();
}
