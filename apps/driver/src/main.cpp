#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <Outshine.h>

namespace {

struct Asked {
  std::string Scenario = "apps/driver/src/f31.scenario";
  double FromLatDeg = 0.0;
  double FromLonDeg = 0.0;
  double ToLatDeg = 0.0;
  double ToLonDeg = 0.0;
  bool Routed = false;
  int WidthPx = 1280;
  int HeightPx = 720;
  bool Headless = false;
  long Frames = 0;
  long Stills = 10;
  bool Every = false;
  std::string Into;
  std::string Assets;
  std::string Shipped = "src/assets";
  std::string Cache = "/tmp/outshine-drive-cache";
  bool Offline = false;
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
      "  --scenario PATH     the declaration to read (default apps/driver/src/f31.scenario)\n"
      "  --size WxH          the frame to render (default 1280x720)\n"
      "  --headless          render without opening a window\n"
      "  --frames N          stop after N frames (default: until the drive arrives)\n"
      "  --into DIR          write stills here -- ten of them, evenly along the drive\n"
      "  --stills N          how many (default 10)\n"
      "  --every             draw EVERY frame, not only the stills -- physics runs a long\n"
      "                      route fast, graphics runs a short one slowly\n"
      "  --assets DIR        where a scenario's asset URIs resolve\n"
      "  --shipped DIR       where outshine's own data is (default src/assets)\n"
      "  --cache DIR         where fetched tiles are kept\n"
      "  --offline           refuse anything that would have to be fetched\n"
      "\n"
      "--from and --to are DELTAS on what the scenario declares: omit them and the drive the\n"
      "declaration carries is the one that runs.\n");
}

enum class Reading { Ran, Asked, Wrong };

[[nodiscard]] Reading Read(int argc, char **argv, Asked &out) {
  for (int at = 1; at < argc; ++at) {
    const std::string_view said = argv[at];
    const bool wants = at + 1 < argc;
    if (said == "--help" || said == "-h") { return Reading::Asked; }
    if (said == "--offline") {
      out.Offline = true;
    } else if (said == "--headless") {
      out.Headless = true;
    } else if (said == "--from" && wants) {
      if (!Pair(argv[++at], out.FromLatDeg, out.FromLonDeg)) { return Reading::Wrong; }
      out.Routed = true;
    } else if (said == "--to" && wants) {
      if (!Pair(argv[++at], out.ToLatDeg, out.ToLonDeg)) { return Reading::Wrong; }
      out.Routed = true;
    } else if (said == "--scenario" && wants) {
      out.Scenario = argv[++at];
    } else if (said == "--into" && wants) {
      out.Into = argv[++at];
    } else if (said == "--every") {
      out.Every = true;
    } else if (said == "--stills" && wants) {
      int held = 0;
      if (!Number(argv[++at], held)) { return Reading::Wrong; }
      out.Stills = held;
    } else if (said == "--assets" && wants) {
      out.Assets = argv[++at];
    } else if (said == "--shipped" && wants) {
      out.Shipped = argv[++at];
    } else if (said == "--cache" && wants) {
      out.Cache = argv[++at];
    } else if (said == "--frames" && wants) {
      int held = 0;
      if (!Number(argv[++at], held)) { return Reading::Wrong; }
      out.Frames = held;
    } else if (said == "--size" && wants) {
      const std::string_view size = argv[++at];
      const size_t by = size.find('x');
      if (by == std::string_view::npos || !Number(size.substr(0, by), out.WidthPx) ||
          !Number(size.substr(by + 1), out.HeightPx)) {
        return Reading::Wrong;
      }
    } else {
      std::printf("outshine-driver: '%.*s' is not an option it knows\n", (int)said.size(),
                  said.data());
      return Reading::Wrong;
    }
  }
  return Reading::Ran;
}

}

int main(int argc, char **argv) {
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  Asked asked;
  const Reading read = Read(argc, argv, asked);
  if (read != Reading::Ran) {
    Usage();
    return read == Reading::Asked ? 0 : 2;
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("REFUSED SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  outshine::Engine engine;
  engine.Under(outshine::Roots{asked.Assets, asked.Shipped, asked.Cache, asked.Offline});
  if (!engine.DrawsInto(outshine::Extent{asked.WidthPx, asked.HeightPx})) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    return 1;
  }

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
  }
  if (!engine.Declare(declared)) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    return 1;
  }

  std::printf("DRIVING %.5f,%.5f -> %.5f,%.5f, %dx%d%s\n", declared.Driven.FromLatDeg,
              declared.Driven.FromLonDeg, declared.Driven.ToLatDeg, declared.Driven.ToLonDeg,
              asked.WidthPx, asked.HeightPx, asked.Headless ? ", headless" : "");

  const bool assembled = engine.Assemble();
  for (const std::string &said : engine.Carried()) { std::printf("  CARRIES %s\n", said.c_str()); }
  if (!assembled) { std::printf("REFUSED %s\n", engine.Error().c_str()); }
  std::printf("%s\n", engine.Whole() > 0.0 ? "ROUTED the declared drive"
                       : assembled     ? "NO DRIVE DECLARED"
                                       : "NO DRIVE -- the picture is what stood without it");

  if (!assembled && !asked.Into.empty()) {
    char named[512];
    std::snprintf(named, sizeof named, "%s/refused.png", asked.Into.c_str());
    const bool stood = engine.Advance();
    if (!stood) { std::printf("STILL %s\n", engine.Error().c_str()); }
    if (engine.Capture(named)) {
      std::printf("KEPT %s -- a failure is loud, and something is always drawn\n", named);
    }
    return 1;
  }
  if (!assembled) { return 1; }

  const double routeM = engine.Whole();
  if (routeM <= 0.0 && asked.Frames <= 0) {
    if (!asked.Into.empty()) {
      char named[512];
      std::snprintf(named, sizeof named, "%s/standing.png", asked.Into.c_str());
      if (engine.Capture(named)) { std::printf("KEPT %s\n", named); }
    }
    std::printf(
        "REFUSED a drive that arrives is what ends this loop and this scenario declares none -- "
        "declare a <drive> or name --frames N\n");
    return 1;
  }
  long frames = 0;
  long kept = 0;
  long nextStill = 0;
  while (engine.Advance()) {
    ++frames;
    if (asked.Every && !engine.RenderTo(outshine::Extent{})) {
      std::printf("REFUSED %s\n", engine.Error().c_str());
      return 1;
    }
    const double alongM = engine.Along();
    const bool wanted =
        !asked.Into.empty() && nextStill < asked.Stills &&
        (asked.Frames > 0
             ? 2 * frames * asked.Stills >= (long)(2 * nextStill + 1) * asked.Frames
             : routeM > 0.0 && 2.0 * alongM * (double)asked.Stills >=
                                   (double)(2 * nextStill + 1) * routeM);
    if (wanted) {
      ++nextStill;
      char named[512];
      std::snprintf(named, sizeof named, "%s/along%02ld.png", asked.Into.c_str(), nextStill);
      if (!engine.Capture(named)) {
        std::printf("REFUSED %s\n", engine.Error().c_str());
        return 1;
      }
      ++kept;
    }
    if (asked.Frames > 0 && frames >= asked.Frames) { break; }
  }
  for (const outshine::Measure &held : engine.Numbers()) {
    std::printf("  MEASURES %s = %.6g %s\n", held.What.c_str(), held.How, held.Unit.c_str());
  }
  if (!engine.Error().empty()) {
    std::printf("STOPPED after %ld frames: %s\n", frames, engine.Error().c_str());
    return 1;
  }
  SDL_Quit();
  std::printf("DROVE %ld frames over %.3f of %.3f km, kept %ld still(s)", frames,
              engine.Along() / 1000.0, routeM / 1000.0, kept);
  if (!asked.Into.empty()) { std::printf(" into %s", asked.Into.c_str()); }
  std::printf("\n");
  return 0;
}
