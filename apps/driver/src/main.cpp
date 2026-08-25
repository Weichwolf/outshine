#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <outshine/Outshine.h>

namespace {

struct Asked {
  std::string Scenario = "apps/driver/src/f31.scenario";
  double FromLatDeg = 0.0;
  double FromLonDeg = 0.0;
  double ToLatDeg = 0.0;
  double ToLonDeg = 0.0;
  bool Routed = false;
  int Zoom = 14;
  int WidthPx = 1280;
  int HeightPx = 720;
  bool Headless = false;
  long Frames = 0;
  std::string Into;
};

[[nodiscard]] bool Pair(std::string_view said, double &first, double &second) {
  const size_t comma = said.find(',');
  if (comma == std::string_view::npos) { return false; }
  const std::string_view left = said.substr(0, comma), right = said.substr(comma + 1);
  return std::from_chars(left.data(), left.data() + left.size(), first).ec == std::errc() &&
         std::from_chars(right.data(), right.data() + right.size(), second).ec == std::errc();
}

[[nodiscard]] bool Number(std::string_view said, int &out) {
  return std::from_chars(said.data(), said.data() + said.size(), out).ec == std::errc();
}

void Usage() {
  std::printf(
      "outshine-driver -- drives a declared car over a route the engine reconstructs\n"
      "\n"
      "  --from LAT,LON      where the drive starts\n"
      "  --to LAT,LON        where it ends\n"
      "  --zoom N            the tile zoom the road is fetched at (default 14)\n"
      "  --scenario PATH     the declaration to read (default apps/driver/src/f31.scenario)\n"
      "  --size WxH          the frame to render (default 1280x720)\n"
      "  --headless          render without opening a window\n"
      "  --frames N          stop after N frames (default: until the drive arrives)\n"
      "  --into DIR          write stills here\n"
      "\n"
      "--from and --to are DELTAS on what the scenario declares: omit them and the drive the\n"
      "declaration carries is the one that runs.\n");
}

[[nodiscard]] bool Read(int argc, char **argv, Asked &out) {
  for (int at = 1; at < argc; ++at) {
    const std::string_view said = argv[at];
    const bool wants = at + 1 < argc;
    if (said == "--help" || said == "-h") { return false; }
    if (said == "--headless") {
      out.Headless = true;
    } else if (said == "--from" && wants) {
      if (!Pair(argv[++at], out.FromLatDeg, out.FromLonDeg)) { return false; }
      out.Routed = true;
    } else if (said == "--to" && wants) {
      if (!Pair(argv[++at], out.ToLatDeg, out.ToLonDeg)) { return false; }
      out.Routed = true;
    } else if (said == "--zoom" && wants) {
      if (!Number(argv[++at], out.Zoom)) { return false; }
    } else if (said == "--scenario" && wants) {
      out.Scenario = argv[++at];
    } else if (said == "--into" && wants) {
      out.Into = argv[++at];
    } else if (said == "--frames" && wants) {
      int held = 0;
      if (!Number(argv[++at], held)) { return false; }
      out.Frames = held;
    } else if (said == "--size" && wants) {
      const std::string_view size = argv[++at];
      const size_t by = size.find('x');
      if (by == std::string_view::npos || !Number(size.substr(0, by), out.WidthPx) ||
          !Number(size.substr(by + 1), out.HeightPx)) {
        return false;
      }
    } else {
      std::printf("outshine-driver: '%.*s' is not an option it knows\n", (int)said.size(),
                  said.data());
      return false;
    }
  }
  return true;
}

}

int main(int argc, char **argv) {
  Asked asked;
  if (!Read(argc, argv, asked)) {
    Usage();
    return 2;
  }

  outshine::Engine engine;
  if (!engine.Read(asked.Scenario)) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    return 1;
  }

  outshine::Scenario declared = engine.Declared();
  if (asked.Routed) {
    declared.Driven.Declared = true;
    declared.Driven.FromLatDeg = asked.FromLatDeg;
    declared.Driven.FromLonDeg = asked.FromLonDeg;
    declared.Driven.ToLatDeg = asked.ToLatDeg;
    declared.Driven.ToLonDeg = asked.ToLonDeg;
    declared.Driven.Zoom = asked.Zoom;
    if (!engine.Declare(declared)) {
      std::printf("REFUSED %s\n", engine.Error().c_str());
      return 1;
    }
  }

  std::printf("DRIVING %.5f,%.5f -> %.5f,%.5f at zoom %d, %dx%d%s\n",
              declared.Driven.FromLatDeg, declared.Driven.FromLonDeg, declared.Driven.ToLatDeg,
              declared.Driven.ToLonDeg, declared.Driven.Zoom, asked.WidthPx, asked.HeightPx,
              asked.Headless ? ", headless" : "");

  if (!engine.Assemble()) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    return 1;
  }

  engine.RenderTo(outshine::Extent{asked.WidthPx, asked.HeightPx});

  long frames = 0;
  while (engine.Advance()) {
    ++frames;
    if (asked.Frames > 0 && frames >= asked.Frames) { break; }
  }
  if (!engine.Error().empty()) {
    std::printf("STOPPED after %ld frames: %s\n", frames, engine.Error().c_str());
    return 1;
  }
  std::printf("DROVE %ld frames of %d\n", frames, engine.Frames());
  return 0;
}
