#ifndef OUTSHINE_TEST_CLIENTSHOT_H
#define OUTSHINE_TEST_CLIENTSHOT_H

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "Check.h"

// THE CASE DRIVES THE SAME COMMAND A PERSON DOES. `build/outshine-client shots --rows <place>` is
// what `make shots` runs; a case that reached around it into the library would be scoring a path
// nobody uses. So the binary is run, its machine-readable row is read, and the ORACLES are applied
// here -- the instrument is the library's, the judgement is the test's, and neither is the other's.
namespace outshine::Test {

struct ClientRow {
  std::string Name;
  std::string Digest;
  std::string Why;
  bool Kept = false;
  double P50Ms = 0.0, P95Ms = 0.0, P99Ms = 0.0;
  double Frames = 0.0, OverBudget = 0.0, WorstAt = 0.0;
  double Triangles = 0.0, BareTiles = 0.0, Variation = 0.0;
  bool Preloaded = false;
  bool Read = false;
};

inline int ScorePlace(const char *place) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  const std::string command =
      std::string("build/outshine-client shots --rows ") + place + " 2>&1";
  std::FILE *const running = popen(command.c_str(), "r");
  if (running == nullptr) {
    Unprepared("the client did not start");
    return Report();
  }
  double control = -1.0;
  ClientRow row;
  char line[4096];
  while (std::fgets(line, sizeof line, running) != nullptr) {
    std::printf("%s", line);
    std::vector<std::string> field;
    for (const char *at = line, *from = line;; ++at) {
      if (*at != '\t' && *at != '\n' && *at != '\0') { continue; }
      field.emplace_back(from, (std::size_t)(at - from));
      from = at + 1;
      if (*at != '\t') { break; }
    }
    if (field.size() >= 2 && field[0] == "CONTROL") { control = std::atof(field[1].c_str()); }
    if (field.size() >= 15 && field[0] == "ROW") {
      row.Read = true;
      row.Name = field[1];
      row.Digest = field[2];
      row.Kept = field[3] == "1";
      row.P50Ms = std::atof(field[4].c_str());
      row.P95Ms = std::atof(field[5].c_str());
      row.P99Ms = std::atof(field[6].c_str());
      row.Frames = std::atof(field[7].c_str());
      row.OverBudget = std::atof(field[8].c_str());
      row.WorstAt = std::atof(field[9].c_str());
      row.Triangles = std::atof(field[10].c_str());
      row.BareTiles = std::atof(field[11].c_str());
      row.Variation = std::atof(field[12].c_str());
      row.Preloaded = field[13] == "1";
      row.Why = field[14];
    }
  }
  const int status = pclose(running);
  if (!row.Read) {
    Unprepared((std::string(place) + " left no row: the client exited " + std::to_string(status) +
                ". `make` builds it")
                   .c_str());
    return Report();
  }
  if (row.Why != "-") {
    Unprepared(row.Why.c_str());
    return Report();
  }

  // THE MEASURE'S OWN NEGATIVE CONTROL, and without it the bar below is a number nobody checked. A
  // bare ellipsoid under a sky IS a vertical gradient, so the statistic is run over one first -- if
  // THAT does not come in far under the bar, the bar separates nothing and every green is worthless.
  CHECK(control >= 0.0 && control < 0.5,
        "**THE BLANK-FRAME BAR SEPARATES SOMETHING**: a picture of nothing is a vertical gradient "
        "and a vertical gradient has no horizontal variation at all, so this has to read far below "
        "the bar of 1.0 a real frame is held to");

  // A PICTURE OF NOTHING PASSED THIS CASE FOR MONTHS. A bare tile means the elevation never
  // arrived, so the ground and everything on it is drawn at sea level: the picture is of the
  // STREAMING and not of the place.
  if (row.BareTiles > 0.0) {
    Unprepared((std::string(place) + " stood " + std::to_string((long)row.BareTiles) +
                " tile(s) BARE on the ellipsoid -- the elevation never arrived, so the ground and "
                "everything on it is drawn at sea level")
                   .c_str());
    return Report();
  }
  if (row.Triangles > 0.0 && row.Variation < 1.0) {
    Unprepared((std::string(place) + " meshed " + std::to_string((long)row.Triangles) +
                " building triangle(s) and its picture varies by " + std::to_string(row.Variation) +
                " of 255 along its rows -- the frame holds the sky and the ground and NONE of the "
                "geometry that was built for it")
                   .c_str());
    return Report();
  }

  CHECK(row.Kept,
        "**THERE IS A PICTURE**: the only thing this case refuses on. A place that declares, "
        "composes, advances and writes its frame has done its whole job here -- what the frame "
        "SHOWS is the owner's to judge, and no number invented here may stand in for that");

  Covers("a declared place on Earth stands, advances and leaves a picture in build/shots for an "
         "eye -- driven through build/outshine-client, the same command a person runs");
  return Report();
}

}
#endif
