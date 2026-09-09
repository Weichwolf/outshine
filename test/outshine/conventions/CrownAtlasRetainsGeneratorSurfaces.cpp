#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <SDL3/SDL.h>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include "CrownAtlas.h"
#include "CrownPieces.h"
#include "Live.h"
#include "math/Units.h"
#include "Tasks.h"
#include "Digest.h"
#include "CrownCache.h"
#include "Sha256.h"
#include <latch>
#include <thread>
#include <algorithm>
#include "Image.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::ifstream input("src/assets/world/species/birch.json");
  const std::string text{std::istreambuf_iterator<char>(input), {}};
  TreeSpecies species;
  CHECK(species.Parse(text.data(), text.size()), "the atlas uses the shipped birch generator");
  const auto tree = TreePrototype::Grow(species);
  CHECK(tree.has_value(), "the atlas has a grown source tree");
  if (!tree) { return Report(); }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    Unprepared(SDL_GetError());
    return Report();
  }
  std::string error;
  CHECK(!CrownAtlas::Bake(*tree, {.Pixels = 2, .Views = 4}, error),
        "an atlas with no interior pixels is refused");
  const auto started = std::chrono::steady_clock::now();
  Geometry control;
  Material white;
  white.BaseColour = {{1, 1, 1, 1}};
  white.Unlit = true;
  const int controlPart = control.addPart("control", control.addSurface("white", white));
  CHECK(control.setPositions(controlPart, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            control.setTriangles(controlPart, std::array<uint32_t, 3>{0, 1, 2}),
        "foreground control geometry stands");
  Engine foreground;
  Scenario::Document foregroundScene;
  foregroundScene.Render.Declared = true;
  foregroundScene.Render.Frame = {1280, 720};
  foregroundScene.Render.Outputs = {"sceneLinear"};
  Scenario::View foregroundCamera;
  foregroundCamera.Id = "control";
  foregroundCamera.Person = "first";
  foregroundCamera.Sees.Placed = true;
  foregroundCamera.Sees.Stands.AtM = {{0, 0, 3}};
  foregroundScene.Views.push_back(foregroundCamera);
  CHECK(foreground.drawsInto({1280, 720}) && foreground.declare(foregroundScene) &&
            foreground.setGeometry(control) && foreground.assemble() && foreground.advance(),
        "independent foreground renderer stands before the bake");
  std::vector<float> foregroundReference, frame;
  CHECK(foreground.renderer().render({}) &&
            foreground.renderer().readPixels(Buffer::Linear, foregroundReference),
        "foreground reference pixels are readable");
  if (foregroundReference.empty()) { return Report(); }
  std::array<std::vector<double>, 3> times;
  bool stable = true, readable = true;
  const auto drawForeground = [&](size_t phase) {
    const auto began = std::chrono::steady_clock::now();
    const bool drawn =
        foreground.renderer().render({}) && foreground.renderer().readPixels(Buffer::Linear, frame);
    times[phase].push_back(
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began)
            .count());
    readable &= drawn;
    stable &= frame == foregroundReference;
  };
  for (size_t count = 0; count < 120; ++count) { drawForeground(0); }
  std::optional<CrownAtlas> atlas;
  Tasks worker(1);
  const auto captureStarted = std::chrono::steady_clock::now();
  const auto job =
      worker.Post([&] { atlas = CrownAtlas::Bake(*tree, {.Pixels = 128, .Views = 4}, error); });
  bool finished = false;
  while (!(finished = worker.Done(job)) &&
         std::chrono::steady_clock::now() - captureStarted < std::chrono::seconds(30)) {
    drawForeground(1);
  }
  if (!finished) { worker.Wait(job); }
  const double captureMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - captureStarted)
          .count();
  for (size_t count = 0; count < 120; ++count) { drawForeground(2); }
  CHECK(readable && stable, "a parallel crown bake leaves foreground linear pixels unchanged");
  std::printf("worker crown capture %.3f ms; observation limited=%d; no world or "
              "portable-threading claim\n",
              captureMs,
              !finished);
  for (size_t phase = 0; phase < times.size(); ++phase) {
    auto &samples = times[phase];
    std::sort(samples.begin(), samples.end());
    if (samples.empty()) { continue; }
    const auto quantile = [&](double q) {
      return samples[static_cast<size_t>(std::ceil(q * samples.size())) - 1];
    };
    std::printf("foreground %s render+linear-readback n=%zu p50=%.3f p95=%.3f p99=%.3f worst=%.3f "
                "ms over16.67=%zu; trivial scene\n",
                phase == 0   ? "before"
                : phase == 1 ? "during-bake"
                             : "after",
                samples.size(),
                quantile(.5),
                quantile(.95),
                quantile(.99),
                samples.back(),
                static_cast<size_t>(std::count_if(
                    samples.begin(), samples.end(), [](double ms) { return ms > 1000.0 / 60; })));
  }
  std::filesystem::create_directories("build/crown-atlas");
  CHECK(foreground.renderer()
            .saveScreenshot("build/crown-atlas/concurrent-foreground.png")
            .has_value(),
        "concurrent foreground PNG is written");

  CHECK(atlas.has_value(), "the native renderer captures crown surface data");
  if (!atlas) {
    std::printf("%s\n", error.c_str());
    return Report();
  }
  const std::string provenance =
      CrownAtlas::ProvenanceFor(species.Definition(), {.Pixels = 128, .Views = 4});
  CHECK(provenance != CrownAtlas::ProvenanceFor(text + "changed", {.Pixels = 128, .Views = 4}) &&
            provenance != CrownAtlas::ProvenanceFor(text, {.Pixels = 256, .Views = 4}) &&
            provenance != CrownAtlas::ProvenanceFor(text, {.Pixels = 128, .Views = 8}),
        "species and capture shape participate in the compiled producer identity");
  const auto encoded = atlas->Encode(provenance, error);
  CHECK(encoded.has_value(), "real crown data encodes without material loss");
  if (!encoded) {
    std::printf("%s\n", error.c_str());
    return Report();
  }
  auto restored = CrownAtlas::Decode(*encoded, provenance, error);
  CHECK(restored.has_value(), "versioned crown data reads back");
  if (!restored) { return Report(); }
  bool identical = restored->Pixels() == atlas->Pixels() &&
                   restored->CentreM() == atlas->CentreM() &&
                   restored->HalfExtentM() == atlas->HalfExtentM() &&
                   restored->Surfaces() == atlas->Surfaces() &&
                   restored->Views().size() == atlas->Views().size();
  if (identical) {
    for (size_t view = 0; view < atlas->Views().size(); ++view) {
      const auto &before = atlas->Views()[view];
      const auto &after = restored->Views()[view];
      identical &=
          before.TowardEye == after.TowardEye && before.Texels.size() == after.Texels.size();
      if (before.Texels.size() != after.Texels.size()) { continue; }
      for (size_t at = 0; at < before.Texels.size(); ++at) {
        identical &= before.Texels[at].Normal == after.Texels[at].Normal &&
                     before.Texels[at].Depth == after.Texels[at].Depth &&
                     before.Texels[at].Surface == after.Texels[at].Surface;
      }
    }
  }
  CHECK(identical,
        "every material, normal, depth, identity and camera bound survives storage exactly");
  CHECK(restored->Encode(provenance, error) == encoded,
        "the artifact has a canonical byte-for-byte round trip");
  CHECK(!CrownAtlas::Decode(*encoded, provenance + "changed", error),
        "changed generator or species provenance invalidates the cache");
  CHECK(!atlas->Encode("", error), "unidentified cache data is refused");
  for (const size_t length : {size_t(0), size_t(8), size_t(71), encoded->size() - 1}) {
    CHECK(!CrownAtlas::Decode(std::span<const uint8_t>(*encoded).first(length), provenance, error),
          "truncated artifact data is refused");
  }
  auto corrupt = *encoded;
  corrupt[100] ^= 1;
  CHECK(!CrownAtlas::Decode(corrupt, provenance, error), "payload corruption is detected");
  const auto resign = [](std::vector<uint8_t> &bytes) {
    uint64_t hash = kDigestBasis;
    for (size_t at = 0; at < bytes.size() - 8; ++at) { hash = DigestFolded(hash, bytes[at]); }
    for (size_t at = 0; at < 8; ++at) {
      bytes[bytes.size() - 8 + at] = static_cast<uint8_t>(hash >> (at * 8));
    }
  };
  for (const size_t field : {size_t(8), size_t(20), size_t(24), size_t(28)}) {
    corrupt = *encoded;
    for (size_t at = 0; at < 4; ++at) { corrupt[field + at] = 255; }
    resign(corrupt);
    CHECK(!CrownAtlas::Decode(corrupt, provenance, error),
          "unsupported version and oversized dimensions fail even with a valid checksum");
  }
  corrupt = *encoded;
  corrupt[64] = 0;
  corrupt[65] = 0;
  corrupt[66] = 192;
  corrupt[67] = 127;
  resign(corrupt);
  CHECK(!CrownAtlas::Decode(corrupt, provenance, error),
        "nonfinite material data is refused even with a valid checksum");
  std::ofstream artifact("build/crown-atlas/birch.crown", std::ios::binary);
  artifact.write(reinterpret_cast<const char *>(encoded->data()),
                 static_cast<std::streamsize>(encoded->size()));
  CHECK(artifact.good(), "the verified crown artifact is written");
  std::printf("crown artifact %zu bytes; source/raw-data equality checked; compiled producer, "
              "species and capture shape identified\n",
              encoded->size());
  const std::string cacheDirectory = "build/crown-atlas/cache";
  std::filesystem::remove_all(cacheDirectory);
  const Data::ContentStore::Config cacheStore{.Directory = cacheDirectory};
  Data::ContentStore rawStore(cacheStore);
  CrownCache cache(worker, {.Store = cacheStore});
  const std::string key = Sha256Hex(provenance);
  const auto occupied = cacheDirectory + "/." + key + ".0";
  {
    std::ofstream held(occupied);
    held << "occupied";
  }
  CHECK(cache.Publish(*atlas, provenance, error),
        "the crown publishes despite an occupied temporary name");
  std::ifstream held(occupied);
  const std::string retained{std::istreambuf_iterator<char>(held), {}};
  CHECK(retained == "occupied",
        "exclusive publication never truncates another writer's temporary file");
  CHECK(rawStore.Keep(key, encoded->data(), encoded->size()),
        "a second store can publish the same key independently");
  CHECK(!rawStore.Read(key, encoded->size() - 1),
        "the byte budget rejects a large file before returning its payload");
  CHECK(rawStore.Read(key, encoded->size()) == encoded,
        "the complete payload is readable at its exact byte budget");
  std::filesystem::create_directory(cacheDirectory + "/blocked");
  CHECK(!rawStore.Keep("blocked", encoded->data(), encoded->size()) &&
            std::filesystem::is_directory(cacheDirectory + "/blocked"),
        "failed publication preserves the existing destination");
  CHECK(rawStore.Keep(Sha256Hex(provenance + "changed"), encoded->data(), encoded->size()),
        "the stale-cache control contains real bytes under a different provenance key");
  std::latch entered(1), release(1);
  const auto blocker = worker.Post([&] {
    entered.count_down();
    release.wait();
  });
  entered.wait();
  CHECK(cache.Read(provenance) == CrownCache::Request::Queued,
        "a cache read posts while its worker is blocked");
  CHECK(cache.Read(provenance) == CrownCache::Request::Existing,
        "duplicate pending keys share one request");
  CHECK(cache.Read(provenance + "changed") == CrownCache::Request::Queued,
        "the second pending slot is available");
  CHECK(cache.Read(provenance + "third") == CrownCache::Request::Full,
        "the pending budget rejects excess requests");
  CHECK(!cache.Take(), "polling returns without waiting for blocked IO");
  release.count_down();
  worker.Wait(blocker);
  std::vector<CrownCache::Loaded> loaded;
  const auto loadStarted = std::chrono::steady_clock::now();
  while (loaded.size() < 2 &&
         std::chrono::steady_clock::now() - loadStarted < std::chrono::seconds(5)) {
    if (auto ready = cache.Take()) {
      loaded.push_back(std::move(*ready));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  CHECK(loaded.size() == 2, "bounded asynchronous reads complete");
  if (loaded.size() == 2) {
    CHECK(loaded[0].Atlas && loaded[0].Atlas->Encode(provenance, error) == encoded,
          "the worker returns the exact published crown artifact");
    CHECK(!loaded[1].Atlas && !loaded[1].Error.empty(),
          "a stale on-disk artifact is rejected after IO");
    if (loaded[0].Atlas) { restored = std::move(loaded[0].Atlas); }
  }
  {
    CrownCache draining(worker, {.Store = cacheStore});
    CHECK(draining.Read(provenance) == CrownCache::Request::Queued,
          "a pending read can be safely drained during cache destruction");
  }
  atlas = std::move(restored);
  CHECK(atlas->Views().size() == 4 && atlas->Surfaces().size() == 2,
        "four independent views retain bark and leaf materials");
  CHECK(atlas->Surfaces()[0].Roughness == species.ShadingParams().BarkRoughness &&
            atlas->Surfaces()[1].Roughness == species.ShadingParams().LeafRoughness,
        "atlas materials retain declared roughness without baking illumination");
  CHECK(!atlas->GeometryAt(atlas->Views().size()), "an absent crown view is refused");
  const auto fine = tree->GeometryAt(0);
  CHECK(fine.has_value(), "fine geometry remains available for visual comparison");
  std::filesystem::create_directories("build/crown-atlas");
  for (size_t view = 0; view < atlas->Views().size(); ++view) {
    const auto &texels = atlas->Views()[view].Texels;
    CHECK(texels.size() == 128u * 128u, "every view has its full pixel population");
    std::array<size_t, 3> covered{};
    bool valid = true;
    size_t depthMismatch = 0, badNormal = 0;
    float leastNormal = 1, mostNormal = 0;
    std::vector<uint8_t> rgba(texels.size() * 4, 0);
    for (size_t at = 0; at < texels.size(); ++at) {
      const auto &pixel = texels[at];
      valid = valid && std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1;
      depthMismatch += (pixel.Depth > 0) != (pixel.Surface > 0);
      valid = valid && ((pixel.Depth > 0) == (pixel.Surface > 0));
      if (pixel.Surface == 0 || pixel.Surface > 2) { continue; }
      ++covered[pixel.Surface];
      const float lengthSquared = Dot(pixel.Normal, pixel.Normal);
      badNormal += !(std::abs(lengthSquared - 1.0f) < 0.003f);
      leastNormal = std::min(leastNormal, lengthSquared);
      mostNormal = std::max(mostNormal, lengthSquared);
      valid = valid && std::abs(lengthSquared - 1.0f) < 0.003f;
      const auto &colour = atlas->Surfaces()[pixel.Surface - 1].BaseColour;
      for (size_t c = 0; c < 3; ++c) {
        const float linear = colour[c];
        const float srgb = linear <= 0.0031308f ? 12.92f * linear
                                                : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
        rgba[at * 4 + c] = static_cast<uint8_t>(std::lround(srgb * 255.0f));
      }
      rgba[at * 4 + 3] = 255;
    }
    std::printf("view %zu depthMismatch=%zu badNormal=%zu norm2=[%g,%g]\n",
                view,
                depthMismatch,
                badNormal,
                static_cast<double>(leastNormal),
                static_cast<double>(mostNormal));
    CHECK(valid, "depth, coverage, surface identities and unit normals agree");
    CHECK(covered[1] > 0 && covered[2] > 0, "each crown view contains both bark and foliage");
    std::vector<uint8_t> png;
    CHECK(Core::EncodePng(rgba.data(), 128, 128, png), "intrinsic crown colour encodes as a PNG");
    std::ofstream output("build/crown-atlas/birch-" + std::to_string(view) + ".png",
                         std::ios::binary);
    output.write(reinterpret_cast<const char *>(png.data()),
                 static_cast<std::streamsize>(png.size()));
    CHECK(output.good(), "crown reference PNG is written");
    std::printf("view %zu bark=%zu leaf=%zu sampled pixels\n", view, covered[1], covered[2]);
    const auto card = atlas->GeometryAt(view);
    CHECK(card.has_value(), "captured crown exports native geometry");
    if (!card) { continue; }
    const auto material = card->surfaceAt(MaterialInstance(0));
    const auto colourMap = card->imageAt(material.BaseColourMap.Image);
    const auto mrMap = card->imageAt(material.MetalRoughMap.Image);
    bool coverageMatches = colourMap.valid(), materialsMatch = mrMap.valid();
    if (coverageMatches && materialsMatch) {
      for (size_t pixel = 0; pixel < texels.size(); ++pixel) {
        coverageMatches &= (colourMap.Rgba[pixel * 4 + 3] == (texels[pixel].Surface ? 255 : 0));
        if (!texels[pixel].Surface) { continue; }
        const auto &original = atlas->Surfaces()[texels[pixel].Surface - 1];
        materialsMatch &=
            std::abs(mrMap.Rgba[pixel * 4 + 1] / 255.0f - original.Roughness) <= 1.0f / 255;
        materialsMatch &=
            std::abs(mrMap.Rgba[pixel * 4 + 2] / 255.0f - original.Metalness) <= 1.0f / 255;
      }
    }
    CHECK(coverageMatches, "native colour alpha preserves every captured hole");
    CHECK(materialsMatch, "native MR channels retain source material values within one code");
    CHECK(material.NormalMap.bound() && material.BaseColourMap.Sampler.Mip == MipFilter::Linear,
          "the card carries normal detail and retains mip filtering");
    std::array<std::vector<float>, 2> frames;
    for (size_t light = 0; light < frames.size(); ++light) {
      Scenario::Document scene;
      scene.Render.Declared = true;
      scene.Render.Frame = {128, 128};
      scene.Render.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
      scene.Lit.Declared = true;
      scene.Lit.Key.Lux = 20000;
      scene.Lit.Key.BearingDeg = light == 0 ? 135 : 315;
      scene.Lit.Key.ElevationDeg = 40;
      Scenario::View camera;
      camera.Id = "card";
      camera.Person = "first";
      camera.Sees.Placed = true;
      const double extent = atlas->HalfExtentM();
      camera.Sees.Stands.AtM = atlas->CentreM() + atlas->Views()[view].TowardEye * (3 * extent);
      camera.Sees.LooksAt = true;
      camera.Sees.LookAtM = atlas->CentreM();
      camera.Sees.setProjection(Scenario::Camera::Ortho{
          .XMagM = extent, .YMagM = extent, .NearM = extent, .FarM = 5 * extent});
      scene.Views.push_back(camera);
      Engine engine;
      std::vector<float> depth, normals;
      const bool rendered = engine.drawsInto({128, 128}) && engine.declare(scene) &&
                            engine.setGeometry(*card) && engine.assemble() && engine.advance() &&
                            engine.renderer().render({}) &&
                            engine.renderer().readPixels(Buffer::Linear, frames[light]) &&
                            engine.renderer().readPixels(Buffer::Depth, depth) &&
                            engine.renderer().readPixels(Buffer::ShadingNormal, normals);
      CHECK(rendered, "native crown card renders with live lighting");
      CHECK(depth.size() == texels.size() && normals.size() == texels.size() * 4 &&
                frames[light].size() == texels.size() * 4,
            "card attachments cover the complete frame");
      if (!rendered || depth.size() != texels.size() || normals.size() != texels.size() * 4) {
        continue;
      }
      size_t mismatches = 0;
      double normalError = 0;
      for (size_t pixel = 0; pixel < texels.size(); ++pixel) {
        mismatches += ((depth[pixel] > 0) != (texels[pixel].Surface > 0));
        if (!texels[pixel].Surface || depth[pixel] <= 0) { continue; }
        Vec3 actual{{normals[pixel * 4], normals[pixel * 4 + 1], normals[pixel * 4 + 2]}};
        Vec3 expected{{texels[pixel].Normal[0], texels[pixel].Normal[1], texels[pixel].Normal[2]}};
        if (!Normalise(actual) || !Normalise(expected)) {
          normalError = 2;
          continue;
        }
        const auto delta = actual - expected;
        normalError = std::max(normalError, std::sqrt(Dot(delta, delta)));
      }
      std::printf("card %zu light %zu coverage mismatch=%zu normal error=%g\n",
                  view,
                  light,
                  mismatches,
                  normalError);
      CHECK(mismatches == 0, "card raster coverage equals source capture at its camera");
      CHECK(normalError <= 4 * std::sqrt(3.0) / 255,
            "rendered normals retain source directions within RGBA8 quantisation bound");
      CHECK(engine.renderer()
                .saveScreenshot("build/crown-atlas/card-" + std::to_string(view) + "-light-" +
                                std::to_string(light) + ".png")
                .has_value(),
            "lit crown card PNG is written");
      if (view == 0 && fine) {
        Engine reference;
        std::vector<float> original;
        const bool drawn = reference.drawsInto({128, 128}) && reference.declare(scene) &&
                           reference.setGeometry(*fine) && reference.assemble() &&
                           reference.advance() && reference.renderer().render({}) &&
                           reference.renderer().readPixels(Buffer::Linear, original);
        CHECK(drawn, "fine tree renders under the identical camera and light");
        if (drawn && original.size() == frames[light].size()) {
          double difference = 0, energy = 0;
          for (size_t at = 0; at < original.size(); ++at) {
            if (at % 4 == 3) { continue; }
            difference += std::abs(original[at] - frames[light][at]);
            energy += std::abs(original[at]);
          }
          std::printf(
              "card/fine light %zu relative linear RGB L1=%g; diagnostic, no quality acceptance\n",
              light,
              energy > 0 ? difference / energy : 0);
        }
        CHECK(
            reference.renderer()
                .saveScreenshot("build/crown-atlas/fine-0-light-" + std::to_string(light) + ".png")
                .has_value(),
            "fine comparison PNG is written");
      }
    }
    CHECK(!frames[0].empty() && frames[0] != frames[1],
          "changing the light relights captured crown surfaces");
  }
  const auto baseCard = atlas->GeometryAt(0);
  Gltf::Subject baseSubject;
  CHECK(baseCard && baseSubject.Assemble(*baseCard),
        "crown piece fixture starts from the native card reference");
  Render::SceneRenderer renderer;
  Core::Declaration declaration;
  declaration.Built = &baseSubject;
  declaration.SurfaceWidthPx = 384;
  declaration.SurfaceHeightPx = 128;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  declaration.KeyLux = 20000;
  declaration.KeyBearingDeg = 135;
  declaration.KeyElevationDeg = 40;
  std::unique_ptr<Core::Live> live;
  CHECK(Core::Live::Open(renderer, declaration, nullptr, live, error),
        "crown piece Live opens with the reference lighting");
  if (!live) { return Report(); }
  Render::SubjectMesh empty;
  empty.Anchor = {{kWgs84A, 0, 0}};
  CHECK(renderer.SetSubjectMesh(empty, error),
        "reference mesh leaves while the Live coordinate anchor remains");
  CHECK(!CrownPieces::Create(*live, *atlas, 0, error), "crown capacity must be positive");
  auto crowns = CrownPieces::Create(*live, *atlas, 2, error);
  CHECK(crowns && renderer.PieceTriangles() == 2 * atlas->Views().size(),
        "one two-triangle prototype stands per captured view");
  if (!crowns) { return Report(); }
  const std::array<Mat4, 3> excessive{};
  CHECK(!crowns->Update(excessive, {}, error),
        "crown grouping refuses more instances than its capacity");
  error.clear();
  const double extent = atlas->HalfExtentM();
  for (size_t view = 0; view < atlas->Views().size(); ++view) {
    const Vec3 toward = atlas->Views()[view].TowardEye;
    const Vec3 right{{toward[2], 0, -toward[0]}};
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = atlas->CentreM() + toward * (9 * extent), .AimM = atlas->CentreM()}, 0.0);
    CHECK(camera.has_value(), "shared crown camera has a valid basis");
    if (!camera) { continue; }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = 3 * extent;
    camera->YMagM = extent;
    camera->ZNearM = extent;
    camera->ZFarM = 15 * extent;
    live->Eye(*camera);
    std::array<Mat4, 2> models{};
    models[0][0] = models[0][10] = -1;
    for (size_t instance = 0; instance < models.size(); ++instance) {
      const Vec3 shift = atlas->CentreM() + right * ((instance == 0 ? -2 : 2) * extent) -
                         models[instance].TransformPoint(atlas->CentreM());
      for (size_t axis = 0; axis < 3; ++axis) { models[instance][12 + axis] = shift[axis]; }
    }
    CHECK(crowns->Update(models, camera->EyeM, error) && live->Draw(error),
          "crown view selection draws rotated and translated instances");
    renderer.WaitForGpu();
    std::vector<float> depth, normals;
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready &&
              renderer.ReadShadingNormal(normals) == Render::ReadState::Ready &&
              depth.size() == 384u * 128u && normals.size() == 384u * 128u * 4u,
          "shared crown attachments are complete");
    if (depth.size() != 384u * 128u || normals.size() != 384u * 128u * 4u) { continue; }
    size_t mismatches = 0, middle = 0;
    double normalError = 0;
    for (size_t y = 0; y < 128; ++y) {
      for (size_t x = 0; x < 128; ++x) {
        middle += depth[y * 384 + x + 128] > 0;
        for (size_t instance = 0; instance < models.size(); ++instance) {
          const size_t source =
              (view + (instance == 0 ? atlas->Views().size() / 2 : 0)) % atlas->Views().size();
          const auto &texel = atlas->Views()[source].Texels[y * 128 + x];
          const size_t pixel = y * 384 + x + instance * 256;
          mismatches += (depth[pixel] > 0) != (texel.Surface > 0);
          if (depth[pixel] <= 0 || texel.Surface == 0) { continue; }
          Vec3 expected = models[instance].TransformDirection(
              {{texel.Normal[0], texel.Normal[1], texel.Normal[2]}});
          Vec3 actual{{normals[pixel * 4], normals[pixel * 4 + 1], normals[pixel * 4 + 2]}};
          if (!Normalise(expected) || !Normalise(actual)) {
            normalError = 2;
            continue;
          }
          const auto delta = actual - expected;
          normalError = std::max(normalError, std::sqrt(Dot(delta, delta)));
        }
      }
    }
    std::printf("crown pieces view %zu coverage mismatch=%zu normal error=%g middle=%zu\n",
                view,
                mismatches,
                normalError,
                middle);
    CHECK(mismatches == 0 && middle == 0,
          "selected crown views retain source coverage with no unplaced prototype in the gap");
    CHECK(normalError <= 4 * std::sqrt(3.0) / 255,
          "shared tangent frames preserve rotated captured crown normals");
    CHECK(renderer.PieceTriangles() == 2 * atlas->Views().size(),
          "view switching retains the same prototype triangle count");
    CHECK(live->Screenshot("build/crown-atlas/pieces-" + std::to_string(view) + ".png", error),
          "shared crown PNG is written");
    CHECK(crowns->Update({}, camera->EyeM, error) && live->Draw(error),
          "empty instance groups deactivate every view");
    renderer.WaitForGpu();
    CHECK(renderer.ReadDepth(depth) == Render::ReadState::Ready &&
              std::ranges::all_of(depth, [](float value) { return value == 0; }),
          "empty crown groups draw no remaining instances");
  }
  crowns.reset();
  CHECK(renderer.PiecesStanding() == 0, "destroying the crown owner releases every view prototype");
  std::printf(
      "atlas capture, checks and PNG export %.3f ms; payload %zu bytes; no world frame-rate "
      "claim\n",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(),
      atlas->Views().size() * 128u * 128u * sizeof(CrownAtlas::Texel));
  return Report();
}
