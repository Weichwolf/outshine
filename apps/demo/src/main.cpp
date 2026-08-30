#include <cmath>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include <Outshine.h>

namespace {

constexpr int kWidthPx = 1280;
constexpr int kHeightPx = 720;
constexpr int kMixRate = 48000;
constexpr int kMixFrames = 1024;

[[nodiscard]] outshine::Geometry Blob() {
  outshine::Geometry made;
  outshine::Material lit;
  lit.BaseColour[0] = 0.55f;
  lit.BaseColour[1] = 0.62f;
  lit.BaseColour[2] = 0.95f;
  lit.Roughness = 0.35f;
  lit.Metalness = 0.1f;
  const outshine::MaterialInstance wears = made.addSurface("blob", lit);

  const int part = made.addPart("blob", wears);
  std::vector<float> positionsM, normals;
  std::vector<uint32_t> triangles;
  constexpr int kRings = 24, kSegments = 48;
  for (int ring = 0; ring <= kRings; ++ring) {
    const double down = std::numbers::pi * (double)ring / (double)kRings;
    for (int segment = 0; segment <= kSegments; ++segment) {
      const double round = 2.0 * std::numbers::pi * (double)segment / (double)kSegments;
      const float x = (float)(std::sin(down) * std::cos(round));
      const float y = (float)std::cos(down);
      const float z = (float)(std::sin(down) * std::sin(round));
      positionsM.insert(positionsM.end(), {x, y, z});
      normals.insert(normals.end(), {x, y, z});
    }
  }
  for (int ring = 0; ring < kRings; ++ring) {
    for (int segment = 0; segment < kSegments; ++segment) {
      const uint32_t at = (uint32_t)(ring * (kSegments + 1) + segment);
      const uint32_t below = at + (uint32_t)kSegments + 1u;
      triangles.insert(triangles.end(), {at, below, at + 1u});
      triangles.insert(triangles.end(), {at + 1u, below, below + 1u});
    }
  }
  (void)made.setPositions(part, positionsM);
  (void)made.setNormals(part, normals);
  (void)made.setTriangles(part, triangles);
  return made;
}

[[nodiscard]] outshine::Scenario Declared() {
  outshine::Scenario made;
  made.Room = 8;

  made.Lit.Declared = true;
  made.Lit.Key.Lux = 12000.0;
  made.Lit.Key.ElevationDeg = 38.0;
  made.Lit.Key.BearingDeg = 210.0;

  outshine::Body blob;
  blob.Name = "blob";
  blob.MassKg = 12.0;
  blob.Placed = true;
  blob.Stands.AtM[1] = 6.0;
  made.Bodies.push_back(blob);

  made.Buses.push_back(outshine::Bus{.Id = "master", .Into = "", .GainDb = 0.0});

  outshine::Sound hum;
  hum.Id = "hum";
  hum.Bus = "master";
  hum.On = "blob";
  hum.Loops = true;
  hum.GainDb = -6.0;
  hum.Heard.Positional = true;
  hum.Heard.By = outshine::Falls::Inverse;
  hum.Heard.RefM = 2.0;
  hum.Heard.Rolloff = 1.0;

  outshine::Voice tone;
  tone.Id = "tone";
  tone.Does = outshine::Makes::Oscillator;
  tone.Parameters.push_back(outshine::Setting{"frequency", "110"});
  tone.Parameters.push_back(outshine::Setting{"shape", "saw"});
  hum.Graph.push_back(tone);

  outshine::Voice air;
  air.Id = "air";
  air.Does = outshine::Makes::Noise;
  hum.Graph.push_back(air);

  outshine::Voice quiet;
  quiet.Id = "quiet";
  quiet.Does = outshine::Makes::Gain;
  quiet.From.push_back("air");
  quiet.Parameters.push_back(outshine::Setting{"gain", "0.08"});
  hum.Graph.push_back(quiet);

  outshine::Voice both;
  both.Id = "both";
  both.Does = outshine::Makes::Mix;
  both.From.push_back("tone");
  both.From.push_back("quiet");
  hum.Graph.push_back(both);

  outshine::Voice warmed;
  warmed.Id = "warmed";
  warmed.Does = outshine::Makes::Biquad;
  warmed.From.push_back("both");
  warmed.Parameters.push_back(outshine::Setting{"frequency", "900"});
  hum.Graph.push_back(warmed);

  made.Sounds.push_back(hum);
  return made;
}

} // namespace

int main(int argc, char *argv[]) {
  bool headless = false;
  int frames = 0;
  for (int at = 1; at < argc; ++at) {
    const std::string said = argv[at];
    if (said == "--headless") { headless = true; }
    if (said == "--frames" && at + 1 < argc) { frames = std::atoi(argv[++at]); }
  }

  if (!SDL_Init(headless ? SDL_INIT_AUDIO : (SDL_INIT_VIDEO | SDL_INIT_AUDIO))) {
    std::printf("SDL did not start: %s\n", SDL_GetError());
    return 1;
  }

  outshine::Engine engine;
  outshine::Renderer renderer = engine.renderer();
  if (!engine.declare(Declared())) {
    std::printf("REFUSED %s\n", engine.error().c_str());
    return 1;
  }
  if (!engine.assemble()) {
    std::printf("REFUSED %s\n", engine.error().c_str());
    return 1;
  }
  if (!engine.setGeometry(Blob())) {
    std::printf("REFUSED %s\n", engine.error().c_str());
    return 1;
  }

  SDL_Window *window = nullptr;
  if (!headless) {
    window = SDL_CreateWindow("outshine demo", kWidthPx, kHeightPx, 0);
    if (window == nullptr || !engine.drawsInto(window)) {
      std::printf("REFUSED %s\n", engine.error().c_str());
      return 1;
    }
  }

  const SDL_AudioSpec want{SDL_AUDIO_F32, 2, kMixRate};
  SDL_AudioStream *const speaking =
      SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &want, nullptr, nullptr);
  if (speaking != nullptr) { SDL_ResumeAudioStreamDevice(speaking); }

  std::vector<float> stereo((size_t)kMixFrames * 2u, 0.0f);
  int drawn = 0, mixed = 0;
  double loudest = 0.0;
  for (bool going = true; going && (frames == 0 || drawn < frames); ++drawn) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) { going = false; }
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) { going = false; }
    }
    if (!engine.advance()) { break; }
    if (!headless && !renderer.render(outshine::Extent{kWidthPx, kHeightPx})) { break; }
    if (speaking == nullptr || SDL_GetAudioStreamQueued(speaking) < kMixRate) {
      if (!engine.mix(stereo, kMixRate)) {
        std::printf("REFUSED %s\n", engine.error().c_str());
        break;
      }
      ++mixed;
      for (const float one : stereo) {
        loudest = std::fabs((double)one) > loudest ? std::fabs((double)one) : loudest;
      }
      if (speaking == nullptr) { continue; }
      SDL_PutAudioStreamData(speaking, stereo.data(), (int)(stereo.size() * sizeof(float)));
    }
  }

  std::printf("DEMO %d frame(s), %d mix(es) of %d frames, loudest %.4f\n",
              drawn,
              mixed,
              kMixFrames,
              loudest);
  SDL_Quit();
  return 0;
}
