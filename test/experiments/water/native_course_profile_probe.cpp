#include "WaterCourseProfile.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include <chrono>

int main(int argc, char **argv) {
  using namespace outshine;
  using namespace outshine::Generators;
  if (argc != 2) { return 2; }
  size_t ns = 0, nq = 0;
  if (!(std::cin >> ns >> nq) || ns > 8192 || nq > 100000) { return 2; }
  std::vector<WaterCourseSegment> segments(ns);
  for (auto &s : segments) {
    if (!(std::cin >> s.Points[0].EastM >> s.Points[0].NorthM >> s.Points[1].EastM >>
          s.Points[1].NorthM >> s.ModelHeightsM[0] >> s.ModelHeightsM[1])) {
      return 2;
    }
  }
  std::vector<EastNorth> points(nq);
  for (auto &p : points) {
    if (!(std::cin >> p.EastM >> p.NorthM)) { return 2; }
  }
  auto before = std::chrono::steady_clock::now();
  const auto profile = WaterCourseProfile::Build(segments);
  if (!profile) { return 3; }
  const double buildMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
  before = std::chrono::steady_clock::now();
  std::vector<WaterProfileSample> samples;
  samples.reserve(nq);
  for (auto p : points) {
    auto sample = profile->Sample(p);
    if (!sample) {
      std::cerr << "sample error " << static_cast<int>(sample.error()) << '\n';
      return 4;
    }
    samples.push_back(*sample);
  }
  const double queryMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - before).count();
  std::ofstream out(argv[1]);
  out << std::setprecision(17) << "{\"build_ms\":" << buildMs << ",\"query_ms\":" << queryMs
      << ",\"heap_bytes\":" << profile->HeapBytes() << ",\"samples\":[";
  bool first = true;
  for (auto &s : samples) {
    if (!first) { out << ','; }
    first = false;
    out << '[' << s.ModelHeightM << ',' << s.NearestSegment << ',' << s.References << ','
        << s.NodesVisited << ',' << s.SegmentTests << ']';
  }
  out << "]}";
}
