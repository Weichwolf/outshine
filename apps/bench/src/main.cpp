#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>

#include <SDL3/SDL.h>

#include <Outshine.h>
#include <Scenario.h>

namespace {

constexpr const char *kScenes[] = {"DamagedHelmet", "Fox",         "BrainStem",
                                   "ABeautifulGame", "VirtualCity", "Sponza"};

struct Asked {
  std::string Scene;
  std::string Scenario = "apps/driver/src/f31.scenario";
  std::string Assets = "apps/driver/src";
  std::string Shipped = "src/assets";
  std::string Cache = "/tmp/outshine-bench-cache";
  bool Offline = true;
  int WidthPx = 1280, HeightPx = 720;
  long Steps = 240;
  bool All = false;
  int Clip = 0;
  int Bodies = 0;
  bool Heap = false;
};

struct Took {
  double WallMs = 0.0;
  double StepP50 = 0.0, StepP95 = 0.0, StepP99 = 0.0;
  double DrawP50 = 0.0, DrawP95 = 0.0, DrawP99 = 0.0;
  size_t Samples = 0;
  std::vector<outshine::Measure> Rows;
  std::vector<outshine::Measure> Early;
  long Between = 0;
};

[[nodiscard]] double Percentile(std::vector<double> held, double part) {
  if (held.empty()) { return 0.0; }
  std::sort(held.begin(), held.end());
  const double at = part * (double)(held.size() - 1);
  const size_t low = (size_t)at;
  const size_t high = low + 1 < held.size() ? low + 1 : low;
  return held[low] + (at - (double)low) * (held[high] - held[low]);
}

[[nodiscard]] double Ran(const Asked &asked, bool drawing, Took *took, std::string &why) {
  outshine::Engine engine;
  engine.Under(outshine::Roots{asked.Assets, asked.Shipped, asked.Cache, asked.Offline});
  if (!engine.DrawsInto(outshine::Extent{asked.WidthPx, asked.HeightPx})) {
    why = engine.Error();
    return -1.0;
  }
  engine.Keeps((size_t)asked.Steps);
  if (asked.Scene.empty()) {
    if (!engine.Read(asked.Scenario)) {
      why = engine.Error();
      return -1.0;
    }
    outshine::Scenario declared = engine.Declared();
    declared.Render.Frame = outshine::Extent{asked.WidthPx, asked.HeightPx};
    if (!engine.Declare(declared) || !engine.Assemble()) {
      why = engine.Error();
      return -1.0;
    }
  } else {
    outshine::Scenario stands;
    stands.Render.Declared = true;
    stands.Render.Frame = outshine::Extent{asked.WidthPx, asked.HeightPx};
    stands.Render.Fill = 0.6;
    stands.Lit.Declared = true;
    stands.Lit.Key.Lux = 40000.0;
    stands.Lit.Key.ElevationDeg = 42.0;
    outshine::Asset shown;
    shown.Uri = "scene.gltf";
    shown.Kind = "gltf";
    shown.Clip = asked.Clip;
    stands.Assets.push_back(shown);
    for (int one = 0; one < asked.Bodies; ++one) {
      outshine::Body falls;
      falls.Name = "body" + std::to_string(one);
      falls.Asset = shown.Uri;
      falls.Placed = true;
      falls.MassKg = 1000.0;
      falls.WidthM = 1.0;
      falls.Stands.AtM[0] = (double)(one % 8) * 2.0;
      falls.Stands.AtM[1] = 20.0 + (double)one;
      falls.Stands.AtM[2] = (double)(one / 8) * 2.0;
      stands.Bodies.push_back(falls);
    }
    if (!engine.Declare(stands) || !engine.Assemble()) {
      why = engine.Error();
      return -1.0;
    }
  }
  const auto began = std::chrono::steady_clock::now();
  long stepped = 0;
  const long settles = asked.Steps > 4 ? 4 : 0;
  while (stepped < asked.Steps && engine.Advance()) {
    ++stepped;
    if (drawing && !engine.RenderTo(outshine::Extent{})) {
      why = engine.Error();
      return -1.0;
    }
    if (stepped == settles) { took->Early = engine.Numbers(); }
  }
  const double tookMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (stepped < asked.Steps) {
    why = "the drive stopped after " + std::to_string(stepped) + " of " +
          std::to_string(asked.Steps) + " steps: " + engine.Error();
    return -1.0;
  }
  took->WallMs = tookMs;
  took->Between = stepped - settles;
  took->Rows = engine.Numbers();
  std::vector<double> steps, pictures;
  engine.StepTimesMs(steps);
  engine.PictureTimesMs(pictures);
  took->Samples = steps.size();
  took->StepP50 = Percentile(steps, 0.50);
  took->StepP95 = Percentile(steps, 0.95);
  took->StepP99 = Percentile(steps, 0.99);
  took->DrawP50 = Percentile(pictures, 0.50);
  took->DrawP95 = Percentile(pictures, 0.95);
  took->DrawP99 = Percentile(pictures, 0.99);
  return tookMs;
}

}

int main(int count, char **args) {
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  Asked asked;
  for (int at = 1; at < count; ++at) {
    const std::string said = args[at];
    const bool wants = at + 1 < count;
    if (said == "--steps" && wants) {
      asked.Steps = std::strtol(args[++at], nullptr, 10);
    } else if (said == "--cache" && wants) {
      asked.Cache = args[++at];
    } else if (said == "--heap") {
      asked.Heap = true;
    } else if (said == "--bodies" && wants) {
      asked.Bodies = (int)std::strtol(args[++at], nullptr, 10);
    } else if (said == "--clip" && wants) {
      asked.Clip = (int)std::strtol(args[++at], nullptr, 10);
    } else if (said == "--scene" && wants) {
      asked.Scene = args[++at];
    } else if (said == "--scenario" && wants) {
      asked.Scenario = args[++at];
    } else if (said == "--size" && wants) {
      const std::string held = args[++at];
      const size_t by = held.find('x');
      if (by != std::string::npos) {
        asked.WidthPx = (int)std::strtol(held.substr(0, by).c_str(), nullptr, 10);
        asked.HeightPx = (int)std::strtol(held.substr(by + 1).c_str(), nullptr, 10);
      }
    } else if (said == "--all") {
      asked.All = true;
    } else if (said == "--online") {
      asked.Offline = false;
    } else {
      std::printf(
          "outshine-bench -- what a PICTURE costs a run of the simulation\n\n"
          "  --scene NAME        one of Khronos's own, benched from the prepared corpus:\n"
          "                      DamagedHelmet Fox BrainStem ABeautifulGame VirtualCity Sponza\n"
          "  --all               bench every one of them in turn\n"
          "  --steps N           fixed steps to take (default 240)\n"
          "  --clip N            which of the file's animations plays (default 0)\n"
          "  --bodies N          declare N freestanding bodies over the scene (default 0)\n"
          "  --heap              what each heap tag takes PER FRAME after the first four\n"
          "  --cache PATH        where fetched world data is kept (default /tmp/outshine-bench-cache)\n"
          "  --scenario PATH     the declaration to drive (default the driver's f31)\n"
          "  --size WxH          the frame to draw when drawing (default 1280x720)\n"
          "  --online            reach the network; offline by default\n\n"
          "It steps the same declaration twice -- once drawing a picture per step and once not "
          "-- and reports both wall times. CLAUDE.md: headless runs UPDATE alone and as fast as "
          "it can; a picture is what makes a run REALTIME.\n");
      return said == "--help" || said == "-h" ? 0 : 1;
    }
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("REFUSED SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  std::string why;
  std::vector<std::string> benching;
  if (asked.All) {
    for (const char *one : kScenes) { benching.push_back(one); }
  } else if (!asked.Scene.empty()) {
    benching.push_back(asked.Scene);
  }

  if (!benching.empty()) {
    const char *prepared = std::getenv("OUTSHINE_PREPARED");
    int refused = 0;
    for (const std::string &scene : benching) {
      Asked one = asked;
      one.Scene = scene;
      one.Assets = std::string(prepared != nullptr ? prepared : "/tmp") +
                   "/test-khronos-glTF-" + scene;
      Took stood;
      const double withMs = Ran(one, true, &stood, why);
      if (withMs < 0.0) {
        std::printf("REFUSED %-16s %s\n", scene.c_str(), why.c_str());
        ++refused;
        continue;
      }
      std::printf("%-16s %8.1f ms over %ld step(s)   step p50 %.3f  p95 %.3f  p99 %.3f ms\n",
                  scene.c_str(), withMs, asked.Steps, stood.StepP50, stood.StepP95, stood.StepP99);
      if (asked.Heap && stood.Between > 0) {
        const std::string under = "heap taken under ";
        for (const outshine::Measure &late : stood.Rows) {
          if (late.What.compare(0, under.size(), under) != 0) { continue; }
          double early = 0.0;
          for (const outshine::Measure &first : stood.Early) {
            if (first.What == late.What) { early = first.How; }
          }
          const double perFrame = (late.How - early) / (double)stood.Between;
          if (perFrame <= 0.0) { continue; }
          std::printf("    HEAP  %-24s %9.1f byte(s) per frame over %ld\n",
                      late.What.c_str() + under.size(), perFrame, stood.Between);
        }
      }
      if (asked.Bodies > 0) {
        double standing = 0.0;
        for (const outshine::Measure &held : stood.Rows) {
          if (held.What == "bodies standing on no route") { standing = held.How; }
        }
        std::printf("    the simulation integrates %.0f freestanding bodies\n", standing);
      }
      for (const outshine::Measure &held : stood.Rows) {
        const std::string what = held.What;
        const size_t took = what.find(", took");
        if (took == std::string::npos || held.How <= 0.0) { continue; }
        const std::string named = what.substr(0, took);
        double triangles = 0.0, draws = 0.0, surfaces = 0.0, placements = 0.0, palettes = 0.0, distinct = 0.0, layouts = 0.0, bytes = 0.0;
        for (const outshine::Measure &beside : stood.Rows) {
          if (beside.What == named + ", triangles") { triangles = beside.How; }
          if (beside.What == named + ", drew") { draws = beside.How; }
          if (beside.What == named + ", surfaces") { surfaces = beside.How; }
          if (beside.What == named + ", placements") { placements = beside.How; }
          if (beside.What == named + ", colour images") { palettes = beside.How; }
          if (beside.What == named + ", placements that differ") { distinct = beside.How; }
          if (beside.What == named + ", device bytes") { bytes = beside.How; }
          if (beside.What == named + ", vertex layouts") { layouts = beside.How; }
        }
        std::printf("    %-22s %7.3f ms  %5.0f draw(s)  %8.0f tri  %9.0f tri/ms"
                    "   %4.0f surface(s)  %4.0f placement(s)  %4.0f image(s)  %2.0f differ"
                    "  %2.0f layout(s)  %9.0f device byte(s)\n",
                    named.c_str(), held.How, draws, triangles, triangles / held.How, surfaces,
                    placements, palettes, distinct, layouts, bytes);
      }
    }
    return refused == 0 ? 0 : 1;
  }

  Took drawn, alone;
  std::vector<outshine::Measure> lastDrawn;
  const double drawnMs = Ran(asked, true, &drawn, why);
  if (drawnMs < 0.0) {
    std::printf("REFUSED the drawn run: %s\n", why.c_str());
    return 1;
  }
  const double steppedMs = Ran(asked, false, &alone, why);
  if (steppedMs < 0.0) {
    std::printf("REFUSED the headless run: %s\n", why.c_str());
    return 1;
  }

  lastDrawn = drawn.Rows;
  std::printf("STEPS                 %ld\n", asked.Steps);
  std::printf("WITH A PICTURE        %.1f ms   %.3f ms/step\n", drawnMs,
              drawnMs / (double)asked.Steps);
  std::printf("WITHOUT ONE           %.1f ms   %.3f ms/step\n", steppedMs,
              steppedMs / (double)asked.Steps);
  std::printf("THE PICTURE COSTS     %.1f%% of the drawn run\n",
              drawnMs <= 0.0 ? 0.0 : 100.0 * (drawnMs - steppedMs) / drawnMs);
  if (asked.Heap && drawn.Between > 0) {
    const std::string under = "heap taken under ";
    std::printf("\nWHAT THE FRAME TAKES FROM THE HEAP, per step after the first four:\n");
    for (const outshine::Measure &late : drawn.Rows) {
      if (late.What.compare(0, under.size(), under) != 0) { continue; }
      double early = 0.0;
      for (const outshine::Measure &first : drawn.Early) {
        if (first.What == late.What) { early = first.How; }
      }
      const double perStep = (late.How - early) / (double)drawn.Between;
      if (perStep <= 0.0) { continue; }
      std::printf("  HEAP  %-24s %12.1f byte(s) per step over %ld\n",
                  late.What.c_str() + under.size(), perStep, drawn.Between);
    }
  }
  std::printf("\nWHAT EACH STAGE DID AND WHAT IT COST -- power is work over time, so a duration\n"
              "alone is not comparable and a rate is:\n");
  for (const outshine::Measure &held : lastDrawn) {
    const std::string what = held.What;
    const size_t took = what.find(", took");
    if (took == std::string::npos) { continue; }
    const std::string named = what.substr(0, took);
    double drew = 0.0, triangles = 0.0;
    for (const outshine::Measure &beside : lastDrawn) {
      if (beside.What == named + ", drew") { drew = beside.How; }
      if (beside.What == named + ", triangles") { triangles = beside.How; }
    }
    if (held.How <= 0.0) { continue; }
    std::printf("  %-22s %8.3f ms   %5.0f draw(s)  %8.0f triangle(s)   %9.0f tri/ms\n",
                named.c_str(), held.How, drew, triangles, triangles / held.How);
  }

  std::printf("\nTHE ENGINE'S OWN SAMPLES, through the door -- it collects, this counts:\n");
  std::printf("  drawn     %zu step(s)   step p50 %.3f  p95 %.3f  p99 %.3f ms\n", drawn.Samples,
              drawn.StepP50, drawn.StepP95, drawn.StepP99);
  std::printf("                          picture p50 %.3f  p95 %.3f  p99 %.3f ms\n",
              drawn.DrawP50, drawn.DrawP95, drawn.DrawP99);
  std::printf("  headless  %zu step(s)   step p50 %.3f  p95 %.3f  p99 %.3f ms\n", alone.Samples,
              alone.StepP50, alone.StepP95, alone.StepP99);
  return steppedMs < drawnMs ? 0 : 2;
}
