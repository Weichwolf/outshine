#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <string>

#include <Outshine.h>

#include "Face.h"

namespace {

struct Asked {
  std::string Scenario;
  std::string Assets;
  std::string Shipped = "src/assets";
  std::string Cases = "test";
  std::string Into;
  std::string Prepared;
  int WidthPx = 1280;
  int HeightPx = 720;
  long Frames = 0;
  bool Windowed = true;
};

void Usage() {
  std::printf(
      "outshine-viewer -- shows a scenario\n"
      "\n"
      "  --show PATH      the scenario to show\n"
      "  --assets DIR     where its asset URIs resolve\n"
      "  --shipped DIR    where outshine's own data is (default src/assets)\n"
      "  --size WxH       the surface to open (default 1280x720)\n"
      "  --cases DIR      where the case manifests are (default test)\n"
      "  --prepared DIR   where their prepared subjects are\n"
      "  --frames N       stop after N frames\n"
      "  --into DIR       keep a still of each frame here\n"
      "  --headless       stand it up without opening a window\n"
      "\n"
      "It shows what the scenario declares and nothing of its own. board:1880: its face\n"
      "becomes a scenario LAYER merged over the one it shows, so a corpus case, a drive and\n"
      "a studio subject reach it through one path.\n");
}

} // namespace

int main(int argc, char **argv) {
  Asked asked;
  for (int at = 1; at < argc; ++at) {
    const char *said = argv[at];
    const bool wants = at + 1 < argc;
    if (std::strcmp(said, "--help") == 0) {
      Usage();
      return 0;
    }
    if (std::strcmp(said, "--headless") == 0) {
      asked.Windowed = false;
    } else if (std::strcmp(said, "--show") == 0 && wants) {
      asked.Scenario = argv[++at];
    } else if (std::strcmp(said, "--assets") == 0 && wants) {
      asked.Assets = argv[++at];
    } else if (std::strcmp(said, "--shipped") == 0 && wants) {
      asked.Shipped = argv[++at];
    } else if (std::strcmp(said, "--cases") == 0 && wants) {
      asked.Cases = argv[++at];
    } else if (std::strcmp(said, "--prepared") == 0 && wants) {
      asked.Prepared = argv[++at];
    } else if (std::strcmp(said, "--into") == 0 && wants) {
      asked.Into = argv[++at];
    } else if (std::strcmp(said, "--frames") == 0 && wants) {
      asked.Frames = std::atol(argv[++at]);
    } else if (std::strcmp(said, "--size") == 0 && wants) {
      const char *size = argv[++at];
      const char *by = std::strchr(size, 'x');
      if (by == nullptr) {
        Usage();
        return 2;
      }
      asked.WidthPx = std::atoi(size);
      asked.HeightPx = std::atoi(by + 1);
    } else {
      std::printf("outshine-viewer: '%s' is not an option it knows\n", said);
      Usage();
      return 2;
    }
  }
  if (asked.Scenario.empty()) {
    Usage();
    return 2;
  }
  if (asked.Prepared.empty()) {
    const char *tmp = std::getenv("TMPDIR");
    std::string root = tmp == nullptr || tmp[0] == '\0' ? "/tmp" : tmp;
    while (root.size() > 1 && root.back() == '/') { root.pop_back(); }
    asked.Prepared = root + "/outshine-prepared";
  }

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::printf("REFUSED SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  SDL_Window *window = nullptr;
  if (asked.Windowed) {
    window = SDL_CreateWindow("outshine", asked.WidthPx, asked.HeightPx, 0);
    if (window == nullptr) {
      std::printf("REFUSED the window did not open: %s\n", SDL_GetError());
      SDL_Quit();
      return 1;
    }
  }
  outshine::Engine engine;
  engine.Under(outshine::Roots{asked.Assets, asked.Shipped, "/tmp/outshine-viewer-cache", false});
  if (!engine.DrawsInto(outshine::Canvas{{asked.WidthPx, asked.HeightPx}, window})) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    if (window != nullptr) { SDL_DestroyWindow(window); }
    SDL_Quit();
    return 1;
  }

  if (!engine.Read(asked.Scenario)) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    SDL_Quit();
    return 1;
  }
  outshine::Scenario showing = engine.Declared();
  {
    const std::vector<outshine::Viewer::Listed> cases =
        outshine::Viewer::Cases(asked.Cases, asked.Prepared);
    outshine::Viewer::Showing at;
    outshine::Surface face;
    face.Document = outshine::Viewer::Declaration(cases, at, asked.WidthPx, asked.HeightPx);
    face.Style = outshine::Viewer::Style();
    face.Z = 100;
    showing.Surfaces.push_back(face);
    std::printf("BROWSING %zu case(s) under %s\n", cases.size(), asked.Cases.c_str());
  }

  if (!engine.Declare(showing)) {
    std::printf("REFUSED %s\n", engine.Error().c_str());
    SDL_Quit();
    return 1;
  }
  if (!engine.Assemble()) { std::printf("REFUSED %s\n", engine.Error().c_str()); }

  std::printf("SHOWING %s at %dx%d%s\n", asked.Scenario.c_str(), asked.WidthPx, asked.HeightPx,
              asked.Windowed ? "" : ", headless");

  long frames = 0;
  bool closing = false;
  while (!closing && engine.Advance()) {
    ++frames;
    for (SDL_Event event; SDL_PollEvent(&event);) {
      closing = closing || event.type == SDL_EVENT_QUIT;
    }
    if (!asked.Into.empty()) {
      char named[512];
      std::snprintf(named, sizeof named, "%s/frame%03ld.png", asked.Into.c_str(), frames);
      if (!engine.Capture(named)) {
        std::printf("REFUSED %s\n", engine.Error().c_str());
        break;
      }
    }
    if (asked.Frames > 0 && frames >= asked.Frames) { break; }
  }

  if (!closing) {
    std::printf("STOPPED after Advance said no: '%s'\n", engine.Error().c_str());
  }
  std::printf("SHOWED %ld frame(s)\n", frames);
  if (window != nullptr) { SDL_DestroyWindow(window); }
  SDL_Quit();
  return 0;
}
