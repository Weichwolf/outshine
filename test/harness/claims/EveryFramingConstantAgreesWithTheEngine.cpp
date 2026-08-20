#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

namespace {

bool Slurp(const std::string &path, std::string &into) {
  std::FILE *file = std::fopen(path.c_str(), "rb");
  if (!file) { return false; }
  char block[1 << 16];
  for (size_t read = std::fread(block, 1, sizeof block, file); read > 0;
       read = std::fread(block, 1, sizeof block, file)) {
    into.append(block, read);
  }
  std::fclose(file);
  return true;
}

bool NumberAfter(const std::string &text, const std::string &name, double &out) {
  const size_t at = text.find(name);
  if (at == std::string::npos) { return false; }
  const size_t equals = text.find('=', at + name.size());
  if (equals == std::string::npos) { return false; }
  size_t from = equals + 1;
  while (from < text.size() && (text[from] == ' ' || text[from] == '\t')) { ++from; }
  size_t to = from;
  while (to < text.size() && (std::isdigit((unsigned char)text[to]) || text[to] == '.' ||
                              text[to] == '-' || text[to] == '+' || text[to] == 'e' ||
                              text[to] == 'E')) {
    ++to;
  }
  if (to == from) { return false; }
  out = std::stod(text.substr(from, to - from));
  return true;
}

struct Pair {
  const char *InTheEngine;
  const char *InThePreparer;
};

constexpr Pair kConstants[] = {
    {"kFramingAzimuthDeg", "FRAMING_AZIMUTH_DEG"},
    {"kFramingElevationDeg", "FRAMING_ELEVATION_DEG"},
    {"kFramingSensorHalfHeightMm", "FRAMING_SENSOR_HALF_HEIGHT_MM"},
    {"kFramingFocalLengthMm", "FRAMING_FOCAL_LENGTH_MM"},
    {"kFramingFill", "FRAMING_FILL"},
    {"kFramingNearFloorFraction", "FRAMING_NEAR_FLOOR_FRACTION"},
};

}

int main() {
  using namespace outshine::Test;

  std::string header, preparer;
  const bool read = Slurp("src/gltf/Framing.h", header) &&
                    Slurp("test/harness/shared/corpus/prep/in_blender_render.py", preparer);
  CHECK(read, "both statements of the framing rule are in the tree to be compared");
  if (!read) { return Report(); }

  size_t agreeing = 0;
  for (const Pair &constant : kConstants) {
    double engine = 0.0, python = 0.0;
    const bool inEngine = NumberAfter(header, constant.InTheEngine, engine);
    const bool inPreparer = NumberAfter(preparer, constant.InThePreparer, python);
    CHECK(inEngine, (std::string("src/gltf/Framing.h declares ") + constant.InTheEngine).c_str());
    CHECK(inPreparer,
          (std::string("the preparer declares ") + constant.InThePreparer).c_str());
    if (!inEngine || !inPreparer) { continue; }
    const bool same = engine == python;
    CHECK(same, (std::string(constant.InTheEngine) + " and " + constant.InThePreparer +
                 " are the same number")
                    .c_str());
    if (!same) {
      std::printf("NOTE %s = %.17g, %s = %.17g\n", constant.InTheEngine, engine,
                  constant.InThePreparer, python);
    } else {
      ++agreeing;
    }
  }
  Note("framing constants agreeing across the two statements", (double)agreeing, "constants");

  size_t declared = 0;
  for (size_t at = header.find("kFraming"); at != std::string::npos;
       at = header.find("kFraming", at + 1)) {

    const size_t line = header.find('\n', at);
    const size_t equals = header.find('=', at);
    if (equals != std::string::npos && (line == std::string::npos || equals < line)) { ++declared; }
  }
  Note("constants the header declares", (double)declared, "constants");
  CHECK(declared == sizeof kConstants / sizeof kConstants[0],
        "every constant the framing rule declares has a pair in this file, so a fifth one added to "
        "the header cannot be a number only one side knows");

  Covers("a derived camera is computed where the bounds are, and the rule's constants "
         "cannot drift between the engine and the preparer");
  return Report();
}
