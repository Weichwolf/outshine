#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include "MeshInput.h"
#include "ImageCapture.h"
#include "Check.h"
#include "ImpostorPreparation.h"
#include "ImpostorSurface.h"
#include <Engine.h>
#include <Scenario.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <span>
#include <string>
#include <vector>
#include "Tasks.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <print>
#include <ratio>
#include "PreparedImpostorAssets.h"
#include "RawSourceCache.h"
#include "Digest.h"
#include "Sha256.h"
#include <iterator>
#include <latch>
#include <thread>
#include <utility>
#include "Image.h"
#include "PngEncoding.h"
#include "math/Srgb.h"
#include "HalfTexels.h"
#include "ImpostorInstances.h"
#include "VegetationStreaming.h"
#include "FrameReadback.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Units.h"
#include <limits>
#include <memory>
#include <SDL3/SDL.h>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators;
constexpr int kPixels = 128;
constexpr size_t kViews = 4;
constexpr Extent kCardExtent{.WidthPx = kPixels, .HeightPx = kPixels};
constexpr Extent kForegroundExtent{.WidthPx = 1280, .HeightPx = 720};
constexpr Extent kInstanceExtent{.WidthPx = 3 * kPixels, .HeightPx = kPixels};
constexpr size_t kTexels = size_t{kPixels} * kPixels;
constexpr size_t kChannels = 4;
constexpr uint8_t kOpaque = 255;
constexpr float kByteSteps = 255;
constexpr float kNormalBound = .003f;
constexpr double kLightLux = 20000;
constexpr double kElevationDeg = 40;
constexpr std::array<double, 2> kBearings{135, 315};
constexpr size_t kForegroundFrames = 120;
constexpr auto kCaptureTimeout = std::chrono::seconds(30);
constexpr auto kLoadTimeout = std::chrono::seconds(5);
constexpr auto kPollInterval = std::chrono::milliseconds(1);
constexpr double kFrameBudgetMs = 1000.0 / 60;
constexpr std::array<double, 3> kQuantiles{.5, .95, .99};
constexpr size_t kChecksumBytes = 8;
constexpr size_t kCorruptByte = 100;
constexpr size_t kShortHeaderBytes = 71;
constexpr std::array<size_t, 4> kCorruptFields{8, 20, 24, 28};
constexpr std::array<uint8_t, 4> kNanBytes{0, 0, 192, 127};
constexpr size_t kNanOffset = 64;
constexpr double kBoundsM = 100;
constexpr size_t kMissingPolls = 20;
constexpr float kHalfStep = 0x1p-10f;
constexpr float kHalfSmallest = 0x1p-24f;

std::string OutputDirectory() {
  return (std::filesystem::path(OUTSHINE_TEST_NEST) / "crown-atlas").string();
}

void WriteBytes(std::span<const uint8_t> bytes, const std::string &path) {
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  CHECK(output.good(), "the verified crown diagnostic artifact is written");
}

struct Foreground {
  Engine Renderer;
  std::vector<float> Reference;
  std::vector<float> Frame;
  std::array<std::vector<double>, 3> Times;
  bool Stable = true;
  bool Readable = true;
};

bool OpenForeground(Foreground &foreground) {
  Geometry control;
  Material white;
  white.BaseColour = {{1, 1, 1, 1}};
  white.Unlit = true;
  const auto surface = control.addSurface("white", white);
  CHECK(surface.has_value(), "foreground material stands");
  if (!surface) { return false; }
  const auto part = control.addPart("control", *surface);
  CHECK(part.has_value(), "foreground control part stands");
  if (!part) { return false; }
  CHECK(control.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
            control.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "foreground control geometry stands");
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = kForegroundExtent;
  scene.Render.Outputs = {"sceneLinear"};
  Scenario::View camera;
  camera.Id = "control";
  camera.Person = "first";
  camera.Placement = Scenario::CameraPlacement::Local;
  camera.Sees.PositionM = {{0, 0, 3}};
  scene.Views.push_back(camera);
  const bool ready = foreground.Renderer.setRenderTarget(kForegroundExtent) &&
                     foreground.Renderer.declare(scene) &&
                     foreground.Renderer.setGeometry(control) && foreground.Renderer.assemble() &&
                     foreground.Renderer.advance();
  CHECK(ready, "independent foreground renderer stands before the bake");
  if (!ready) { return false; }
  const bool read = foreground.Renderer.renderer().render({}) &&
                    foreground.Renderer.renderer().readPixels(Buffer::Linear, foreground.Reference);
  CHECK(read, "foreground reference pixels are readable");
  return read && !foreground.Reference.empty();
}

void DrawForeground(Foreground &foreground, size_t phase) {
  const auto began = std::chrono::steady_clock::now();
  const bool drawn = foreground.Renderer.renderer().render({}) &&
                     foreground.Renderer.renderer().readPixels(Buffer::Linear, foreground.Frame);
  foreground.Times[phase].push_back(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  foreground.Readable &= drawn;
  foreground.Stable &= foreground.Frame == foreground.Reference;
}

void PrintForegroundTimes(Foreground &foreground) {
  constexpr std::array<const char *, 3> labels{"before", "during-bake", "after"};
  for (size_t phase = 0; phase < foreground.Times.size(); ++phase) {
    auto &samples = foreground.Times[phase];
    std::ranges::sort(samples);
    if (samples.empty()) { continue; }
    const auto quantile = [&](double q) {
      return samples[static_cast<size_t>(std::ceil(q * static_cast<double>(samples.size()))) - 1];
    };
    std::println("foreground {} render+linear-readback n={} p50={:.3f} p95={:.3f} p99={:.3f} "
                 "worst={:.3f} ms over16.67={}; trivial scene",
                 labels[phase],
                 samples.size(),
                 quantile(kQuantiles[0]),
                 quantile(kQuantiles[1]),
                 quantile(kQuantiles[2]),
                 samples.back(),
                 std::count_if(samples.begin(), samples.end(), [](double ms) {
                   return ms > kFrameBudgetMs;
                 }));
  }
}

std::optional<ImpostorAtlas>
CaptureAtlas(const TreePrototype &tree, Tasks &worker, Foreground &foreground) {
  std::string error;
  for (size_t n = 0; n < kForegroundFrames; ++n) { DrawForeground(foreground, 0); }
  std::optional<ImpostorAtlas> atlas;
  const auto began = std::chrono::steady_clock::now();
  const auto job = worker.Post(
      [&] { atlas = BakeImpostorAtlas(tree, {.Pixels = kPixels, .Views = kViews}, error); });
  bool finished = false;
  while (!(finished = worker.TakeCompletion(job)) &&
         std::chrono::steady_clock::now() - began < kCaptureTimeout) {
    DrawForeground(foreground, 1);
  }
  if (!finished) { worker.Wait(job); }
  const double elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  for (size_t n = 0; n < kForegroundFrames; ++n) { DrawForeground(foreground, 2); }
  CHECK(foreground.Readable && foreground.Stable,
        "a parallel crown bake leaves foreground linear pixels unchanged");
  std::println("worker crown capture {:.3f} ms; observation limited={}; no world or "
               "portable-threading claim",
               elapsed,
               !finished);
  PrintForegroundTimes(foreground);
  std::filesystem::create_directories(OutputDirectory());
  CHECK(outshine::App::SaveImage(foreground.Renderer,
                                 OutputDirectory() + "/concurrent-foreground.png")
            .has_value(),
        "concurrent foreground PNG is written");
  CHECK(atlas.has_value(), "the native renderer captures crown surface data");
  if (!atlas) { std::println("{}", error); }
  return atlas;
}

bool SameView(const ImpostorAtlas::View &before, const ImpostorAtlas::View &after) {
  if (before.TowardEye != after.TowardEye || before.Texels.size() != after.Texels.size()) {
    return false;
  }
  for (size_t at = 0; at < before.Texels.size(); ++at) {
    const auto &a = before.Texels[at];
    const auto &b = after.Texels[at];
    if (a.Normal != b.Normal || a.Depth != b.Depth || a.Surface != b.Surface) { return false; }
  }
  return true;
}

void CheckSameAtlas(const ImpostorAtlas &atlas, const ImpostorAtlas &restored) {
  bool identical = restored.Pixels() == atlas.Pixels() && restored.CentreM() == atlas.CentreM() &&
                   restored.HalfExtentM() == atlas.HalfExtentM() &&
                   restored.Surfaces() == atlas.Surfaces() &&
                   restored.Views().size() == atlas.Views().size();
  if (identical) {
    for (size_t view = 0; view < atlas.Views().size(); ++view) {
      identical &= SameView(atlas.Views()[view], restored.Views()[view]);
    }
  }
  CHECK(identical,
        "every material, normal, depth, identity and camera bound survives storage exactly");
}

void Resign(std::vector<uint8_t> &bytes) {
  uint64_t hash = kDigestBasis;
  for (size_t at = 0; at < bytes.size() - kChecksumBytes; ++at) {
    hash = DigestFolded(hash, bytes[at]);
  }
  for (size_t at = 0; at < kChecksumBytes; ++at) {
    bytes[bytes.size() - kChecksumBytes + at] = static_cast<uint8_t>(hash >> (at * kChecksumBytes));
  }
}

void CheckCorruption(const ImpostorAtlas &atlas,
                     const std::vector<uint8_t> &encoded,
                     const std::string &provenance) {
  std::string error;
  CHECK(!ImpostorAtlas::Decode(encoded, provenance + "changed", error),
        "changed generator or species provenance invalidates the cache");
  CHECK(!atlas.Encode("", error), "unidentified cache data is refused");
  for (const size_t length : {size_t{0}, kChecksumBytes, kShortHeaderBytes, encoded.size() - 1}) {
    CHECK(!ImpostorAtlas::Decode(std::span(encoded).first(length), provenance, error),
          "truncated artifact data is refused");
  }
  auto corrupt = encoded;
  corrupt[kCorruptByte] ^= 1;
  CHECK(!ImpostorAtlas::Decode(corrupt, provenance, error), "payload corruption is detected");
  for (const size_t field : kCorruptFields) {
    corrupt = encoded;
    for (size_t at = 0; at < sizeof(uint32_t); ++at) { corrupt[field + at] = kOpaque; }
    Resign(corrupt);
    CHECK(!ImpostorAtlas::Decode(corrupt, provenance, error),
          "unsupported version and oversized dimensions fail even with a valid checksum");
  }
  corrupt = encoded;
  std::ranges::copy(kNanBytes, corrupt.begin() + kNanOffset);
  Resign(corrupt);
  CHECK(!ImpostorAtlas::Decode(corrupt, provenance, error),
        "nonfinite material data is refused even with a valid checksum");
}

std::optional<std::vector<uint8_t>>
CheckStorage(const ImpostorAtlas &atlas, const TreeSpecies &species, const std::string &text) {
  std::string error;
  const std::string provenance =
      ImpostorAtlasProvenance(species.Definition(), {.Pixels = kPixels, .Views = kViews});
  CHECK(provenance !=
                ImpostorAtlasProvenance(text + "changed", {.Pixels = kPixels, .Views = kViews}) &&
            provenance != ImpostorAtlasProvenance(text, {.Pixels = 2 * kPixels, .Views = kViews}) &&
            provenance != ImpostorAtlasProvenance(text, {.Pixels = kPixels, .Views = 2 * kViews}),
        "species and capture shape participate in the compiled producer identity");
  auto encoded = atlas.Encode(provenance, error);
  CHECK(encoded.has_value(), "real crown data encodes without material loss");
  if (!encoded) {
    std::println("{}", error);
    return std::nullopt;
  }
  const auto restored = ImpostorAtlas::Decode(*encoded, provenance, error);
  CHECK(restored.has_value(), "versioned crown data reads back");
  if (!restored) { return std::nullopt; }
  CheckSameAtlas(atlas, *restored);
  CHECK(restored->Encode(provenance, error) == encoded,
        "the artifact has a canonical byte-for-byte round trip");
  CheckCorruption(atlas, *encoded, provenance);
  WriteBytes(*encoded, OutputDirectory() + "/birch.crown");
  std::println("crown artifact {} bytes; source/raw-data equality checked; compiled producer, "
               "species and capture shape identified",
               encoded->size());
  return encoded;
}

void CheckPublicationCollision(Data::RawSourceCache &raw,
                               const Data::RawSourceCache::Config &config,
                               const std::vector<uint8_t> &bytes,
                               const std::string &provenance) {
  const auto key = Sha256Hex(provenance);
  const auto occupied = config.Directory + "/." + key + ".0";
  {
    std::ofstream held(occupied);
    held << "occupied";
  }
  CHECK(raw.Keep(key, bytes.data(), bytes.size()),
        "source bytes publish despite an occupied temporary name");
  std::ifstream held(occupied);
  const std::string retained{std::istreambuf_iterator<char>(held), {}};
  CHECK(retained == "occupied",
        "exclusive publication never truncates another writer's temporary file");
}

void CheckRawStore(Data::RawSourceCache &raw,
                   const Data::RawSourceCache::Config &config,
                   const std::vector<uint8_t> &bytes,
                   const std::string &provenance) {
  const auto key = Sha256Hex(provenance);
  CHECK(raw.Keep(key, bytes.data(), bytes.size()),
        "a second store can publish the same key independently");
  CHECK(raw.Lookup(key, bytes.size() - 1).Where == Data::RawSourceCache::Presence::Unknown,
        "the byte budget rejects a large file before returning its payload");
  const auto read = raw.Lookup(key, bytes.size());
  CHECK(read.Bytes == bytes, "the complete payload is readable at its exact byte budget");
  std::filesystem::create_directory(config.Directory + "/blocked");
  CHECK(!raw.Keep("blocked", bytes.data(), bytes.size()) &&
            std::filesystem::is_directory(config.Directory + "/blocked"),
        "failed publication preserves the existing destination");
  CHECK(raw.Keep(Sha256Hex(provenance + "changed"), bytes.data(), bytes.size()),
        "the stale-cache control contains real bytes under a different provenance key");
}

bool PublishStale(const std::string &directory,
                  const std::vector<uint8_t> &bytes,
                  const std::string &provenance) {
  auto native = AssetCache::Open(directory + "/prototypes.sqlite");
  CHECK(native.has_value(), "native prototype index can be opened independently");
  if (!native) { return false; }
  const AssetRecord stale{.Key = Sha256Hex(provenance + "changed"),
                          .Kind = "impostor-prototype",
                          .Bounds = {.Min = {{-kBoundsM, -kBoundsM, -kBoundsM}},
                                     .Max = {{kBoundsM, kBoundsM, kBoundsM}}},
                          .Package = {},
                          .ByteCount = bytes.size(),
                          .Parent = {}};
  CHECK((*native)->Publish(std::span(&stale, size_t{1}), bytes).has_value(),
        "native stale control has valid indexed bytes but the wrong generator provenance");
  return true;
}

void CheckPendingReads(Content::PreparedImpostorAssets &cache,
                       Tasks &worker,
                       const std::string &provenance) {
  std::latch entered(1);
  std::latch release(1);
  const auto blocker = worker.Post([&] {
    entered.count_down();
    release.wait();
  });
  entered.wait();
  CHECK(cache.Read(provenance) == Content::PreparedImpostorAssets::Request::Queued,
        "a cache read posts while its worker is blocked");
  CHECK(cache.Read(provenance) == Content::PreparedImpostorAssets::Request::Existing,
        "duplicate pending keys share one request");
  CHECK(cache.Read(provenance + "changed") == Content::PreparedImpostorAssets::Request::Queued,
        "the second pending slot is available");
  CHECK(cache.Read(provenance + "third") == Content::PreparedImpostorAssets::Request::Full,
        "the pending budget rejects excess requests");
  CHECK(!cache.Take(), "polling returns without waiting for blocked IO");
  release.count_down();
  worker.Wait(blocker);
}

void CheckLoaded(Content::PreparedImpostorAssets &cache,
                 std::optional<ImpostorAtlas> &atlas,
                 const std::vector<uint8_t> &bytes,
                 const std::string &provenance) {
  std::vector<Content::PreparedImpostorAssets::Loaded> loaded;
  const auto began = std::chrono::steady_clock::now();
  while (loaded.size() < 2 && std::chrono::steady_clock::now() - began < kLoadTimeout) {
    if (auto ready = cache.Take()) {
      loaded.push_back(std::move(*ready));
    } else {
      std::this_thread::sleep_for(kPollInterval);
    }
  }
  CHECK(loaded.size() == 2, "bounded asynchronous reads complete");
  CHECK(cache.Costs().Hits == 1 && cache.Costs().Misses == 1 &&
            cache.Costs().ReadBytes == 2 * bytes.size(),
        "native IO reads both complete packages and rejects the stale decoded product");
  if (loaded.size() != 2) { return; }
  std::string error;
  const auto &ready = loaded[0].Atlas;
  bool exact = false;
  if (ready) {
    const auto encoded = ready->Encode(provenance, error);
    exact = encoded.has_value() && *encoded == bytes;
  }
  CHECK(exact, "the worker returns the exact published crown artifact");
  CHECK(!loaded[1].Atlas && !loaded[1].Error.empty(),
        "a stale on-disk artifact is rejected after IO");
  if (loaded[0].Atlas) { atlas = std::move(loaded[0].Atlas); }
}

bool CheckCache(std::optional<ImpostorAtlas> &atlas,
                const std::vector<uint8_t> &bytes,
                const std::string &provenance,
                Tasks &worker) {
  if (!atlas) { return false; }
  const std::string directory = OutputDirectory() + "/cache";
  std::filesystem::remove_all(directory);
  const Data::RawSourceCache::Config config{.Directory = directory + "/sources"};
  Data::RawSourceCache raw(config);
  Content::PreparedImpostorAssets cache(worker, {.Directory = directory});
  CheckPublicationCollision(raw, config, bytes, provenance);
  std::string error;
  CHECK(cache.Publish(*atlas, provenance, error),
        "native prototype publishes through the common package and spatial-index service");
  CheckRawStore(raw, config, bytes, provenance);
  if (!PublishStale(directory, bytes, provenance)) { return false; }
  CheckPendingReads(cache, worker, provenance);
  CheckLoaded(cache, atlas, bytes, provenance);
  {
    Content::PreparedImpostorAssets draining(worker, {.Directory = directory});
    CHECK(draining.Read(provenance) == Content::PreparedImpostorAssets::Request::Queued,
          "a pending read can be safely drained during cache destruction");
  }
  return true;
}

void CheckIntrinsicCapture(const ImpostorAtlas &atlas, size_t view) {
  const auto &texels = atlas.Views()[view].Texels;
  CHECK(texels.size() == kTexels, "every view has its full pixel population");
  std::array<size_t, 3> covered{};
  bool valid = true;
  size_t depthMismatch = 0;
  size_t badNormal = 0;
  float leastNormal = 1;
  float mostNormal = 0;
  std::vector<uint8_t> rgba(texels.size() * kChannels, 0);
  for (size_t at = 0; at < texels.size(); ++at) {
    const auto &pixel = texels[at];
    valid = valid && std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1;
    depthMismatch += static_cast<size_t>((pixel.Depth > 0) != (pixel.Surface > 0));
    valid = valid && ((pixel.Depth > 0) == (pixel.Surface > 0));
    if (pixel.Surface == 0 || pixel.Surface > 2) { continue; }
    ++covered[pixel.Surface];
    const float squared = Dot(pixel.Normal, pixel.Normal);
    badNormal += static_cast<size_t>(!(std::abs(squared - 1.0f) < kNormalBound));
    leastNormal = std::min(leastNormal, squared);
    mostNormal = std::max(mostNormal, squared);
    valid = valid && std::abs(squared - 1.0f) < kNormalBound;
    const auto &colour = atlas.Surfaces()[pixel.Surface - 1].BaseColour;
    for (size_t c = 0; c < 3; ++c) {
      rgba[at * kChannels + c] =
          static_cast<uint8_t>(std::lround(ColourSpace::SrgbFromLinear(colour[c]) * kByteSteps));
    }
    rgba[at * kChannels + 3] = kOpaque;
  }
  std::println("view {} depthMismatch={} badNormal={} norm2=[{},{}]",
               view,
               depthMismatch,
               badNormal,
               leastNormal,
               mostNormal);
  CHECK(valid, "depth, coverage, surface identities and unit normals agree");
  CHECK(covered[1] > 0 && covered[2] > 0, "each crown view contains both bark and foliage");
  std::vector<uint8_t> png;
  CHECK(App::EncodePng(rgba.data(), kPixels, kPixels, png),
        "intrinsic crown colour encodes as a PNG");
  WriteBytes(png, OutputDirectory() + "/birch-" + std::to_string(view) + ".png");
  std::println("view {} bark={} leaf={} sampled pixels", view, covered[1], covered[2]);
}

struct OpticalMaps {
  ImageView Colour;
  ImageView Moments;
  ImageView Reflection;
  ImageView Transmission;
};

bool FullImage(ImageView image, size_t texels) {
  return image.valid() && static_cast<size_t>(image.WidthPx) * image.HeightPx == texels;
}

bool SheetMatches(const OpticalMaps &maps,
                  std::span<const float> moments,
                  size_t offset,
                  const Material &original,
                  const ImpostorAtlas::SurfaceSample &sample) {
  bool valid = moments[offset + 3] == 0 && maps.Reflection.Rgba[offset + 3] == kOpaque;
  for (size_t c = 0; c < 3; ++c) {
    valid &= std::abs(static_cast<float>(maps.Reflection.Rgba[offset + c]) / kByteSteps -
                      sample.BaseColour[c]) <= 1.0f / kByteSteps;
    valid &= std::abs(static_cast<float>(maps.Transmission.Rgba[offset + c]) / kByteSteps -
                      original.DiffuseTransmission[c]) <= 1.0f / kByteSteps;
  }
  return valid;
}

bool OpaqueMatches(const OpticalMaps &maps,
                   std::span<const float> moments,
                   size_t offset,
                   const Material &original) {
  const float alpha = original.Roughness * original.Roughness;
  return std::abs(moments[offset + 1] - alpha * alpha) <=
             std::max(kHalfSmallest, alpha * alpha * kHalfStep) &&
         std::abs(moments[offset + 2] - original.Metalness) <=
             std::max(kHalfSmallest, original.Metalness * kHalfStep) &&
         moments[offset + 3] == 1 && maps.Reflection.Rgba[offset + 3] == 0;
}

void CheckMaterialMaps(const ImpostorAtlas &atlas, size_t view, const Geometry &card) {
  const auto material = card.surfaceAt(MaterialInstance(0));
  const OpticalMaps maps{.Colour = card.imageAt(material.BaseColourMap.Image),
                         .Moments = card.imageAt(material.MetalRoughMap.Image),
                         .Reflection = card.imageAt(material.DiffuseReflectionMap.Image),
                         .Transmission = card.imageAt(material.DiffuseTransmissionMap.Image)};
  CHECK(material.Scattering == ScatteringModel::AreaMixture && maps.Reflection.valid() &&
            maps.Transmission.valid(),
        "native crown card retains separate opaque and sheet optical maps");
  const auto moments =
      Core::UnpackHalfTexels({.WidthPx = static_cast<uint32_t>(maps.Moments.WidthPx),
                              .HeightPx = static_cast<uint32_t>(maps.Moments.HeightPx)},
                             maps.Moments.Rgba);
  const auto &source = atlas.Views()[view];
  bool coverage = FullImage(maps.Colour, source.Texels.size());
  bool materials = FullImage(maps.Moments, source.Texels.size()) &&
                   FullImage(maps.Reflection, source.Texels.size()) &&
                   FullImage(maps.Transmission, source.Texels.size()) &&
                   maps.Moments.Format == ImagePixelFormat::Rgba16Float && moments.has_value() &&
                   source.Materials.size() == source.Texels.size();
  if (coverage && materials && moments) {
    for (size_t pixel = 0; pixel < source.Texels.size(); ++pixel) {
      coverage &= maps.Colour.Rgba[pixel * kChannels + 3] ==
                  (source.Texels[pixel].Surface != 0 ? kOpaque : 0);
      if (source.Texels[pixel].Surface == 0) { continue; }
      const auto &original = atlas.Surfaces()[source.Texels[pixel].Surface - 1];
      if (original.Scattering == ScatteringModel::DiffuseThinSheet) {
        materials &=
            SheetMatches(maps, *moments, pixel * kChannels, original, source.Materials[pixel]);
      } else {
        materials &= OpaqueMatches(maps, *moments, pixel * kChannels, original);
      }
    }
  }
  CHECK(coverage, "native colour alpha preserves every captured hole");
  CHECK(
      materials,
      "native covered-area channels retain opaque roughness moments and sheet R/T within one code");
  CHECK(material.NormalMap.bound() && material.BaseColourMap.Sampler.Mip == MipFilter::Linear,
        "the card carries normal detail and retains mip filtering");
}

Scenario::Document CardScene(size_t view, const ImpostorAtlas &atlas, size_t light) {
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = kCardExtent;
  scene.Render.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  scene.Lit.Declared = true;
  scene.Lit.Key.Lux = kLightLux;
  scene.Lit.Key.BearingDeg = kBearings[light];
  scene.Lit.Key.ElevationDeg = kElevationDeg;
  Scenario::View camera;
  camera.Id = "card";
  camera.Person = "first";
  camera.Placement = Scenario::CameraPlacement::Local;
  const double extent = atlas.HalfExtentM();
  camera.Sees.PositionM = atlas.CentreM() + atlas.Views()[view].TowardEye * (3 * extent);
  camera.Sees.LooksAt = true;
  camera.Sees.LookAtM = atlas.CentreM();
  camera.Sees.setProjection(
      Camera::Ortho{.XMagM = extent, .YMagM = extent, .NearM = extent, .FarM = 5 * extent});
  scene.Views.push_back(camera);
  return scene;
}

double NormalDifference(Vec3 actual, Vec3 expected) {
  if (!Normalise(actual) || !Normalise(expected)) { return 2; }
  const auto delta = actual - expected;
  return std::sqrt(Dot(delta, delta));
}

struct CardAttachments {
  std::span<const float> Depth;
  std::span<const float> Normals;
};

void CheckCardAttachments(const ImpostorAtlas::View &source,
                          const CardAttachments &data,
                          size_t view,
                          size_t light) {
  size_t mismatches = 0;
  double normalError = 0;
  for (size_t pixel = 0; pixel < source.Texels.size(); ++pixel) {
    mismatches +=
        static_cast<size_t>((data.Depth[pixel] > 0) != (source.Texels[pixel].Surface > 0));
    if (source.Texels[pixel].Surface == 0 || data.Depth[pixel] <= 0) { continue; }
    const auto &n = source.Texels[pixel].Normal;
    const Vec3 actual{{data.Normals[pixel * kChannels],
                       data.Normals[pixel * kChannels + 1],
                       data.Normals[pixel * kChannels + 2]}};
    normalError = std::max(normalError, NormalDifference(actual, {{n[0], n[1], n[2]}}));
  }
  std::println("card {} light {} coverage mismatch={} normal error={}",
               view,
               light,
               mismatches,
               normalError);
  CHECK(mismatches == 0, "card raster coverage equals source capture at its camera");
  CHECK(normalError <= 4 * std::sqrt(3.0) / kByteSteps,
        "rendered normals retain source directions within RGBA8 quantisation bound");
}

void CheckFine(const Geometry &fine,
               const Scenario::Document &scene,
               std::span<const float> frame,
               size_t light) {
  Engine reference;
  std::vector<float> original;
  const bool drawn = reference.setRenderTarget(kCardExtent) && reference.declare(scene) &&
                     reference.setGeometry(fine) && reference.assemble() && reference.advance() &&
                     reference.renderer().render({}) &&
                     reference.renderer().readPixels(Buffer::Linear, original);
  CHECK(drawn, "fine tree renders under the identical camera and light");
  if (drawn && original.size() == frame.size()) {
    double difference = 0;
    double energy = 0;
    for (size_t at = 0; at < original.size(); ++at) {
      if (at % kChannels == 3) { continue; }
      difference += std::abs(original[at] - frame[at]);
      energy += std::abs(original[at]);
    }
    std::println("card/fine light {} relative linear RGB L1={}; diagnostic, no quality acceptance",
                 light,
                 energy > 0 ? difference / energy : 0);
  }
  CHECK(outshine::App::SaveImage(
            reference, OutputDirectory() + "/fine-0-light-" + std::to_string(light) + ".png")
            .has_value(),
        "fine comparison PNG is written");
}

void CheckLighting(const ImpostorAtlas &atlas,
                   size_t view,
                   const Geometry &card,
                   const std::optional<Geometry> &fine) {
  std::array<std::vector<float>, 2> frames;
  for (size_t light = 0; light < frames.size(); ++light) {
    const auto scene = CardScene(view, atlas, light);
    Engine engine;
    std::vector<float> depth;
    std::vector<float> normals;
    const bool rendered = engine.setRenderTarget(kCardExtent) && engine.declare(scene) &&
                          engine.setGeometry(card) && engine.assemble() && engine.advance() &&
                          engine.renderer().render({}) &&
                          engine.renderer().readPixels(Buffer::Linear, frames[light]) &&
                          engine.renderer().readPixels(Buffer::Depth, depth) &&
                          engine.renderer().readPixels(Buffer::ShadingNormal, normals);
    CHECK(rendered, "native crown card renders with live lighting");
    CHECK(depth.size() == kTexels && normals.size() == kTexels * kChannels &&
              frames[light].size() == kTexels * kChannels,
          "card attachments cover the complete frame");
    if (!rendered || depth.size() != kTexels || normals.size() != kTexels * kChannels ||
        frames[light].size() != kTexels * kChannels) {
      continue;
    }
    CheckCardAttachments(atlas.Views()[view], {.Depth = depth, .Normals = normals}, view, light);
    CHECK(outshine::App::SaveImage(engine,
                                   OutputDirectory() + "/card-" + std::to_string(view) + "-light-" +
                                       std::to_string(light) + ".png")
              .has_value(),
          "lit crown card PNG is written");
    if (view == 0 && fine) { CheckFine(*fine, scene, frames[light], light); }
  }
  CHECK(!frames[0].empty() && frames[0] != frames[1],
        "changing the light relights captured crown surfaces");
}

void CheckViews(const ImpostorAtlas &atlas, const TreePrototype &tree, const TreeSpecies &species) {
  CHECK(atlas.Views().size() == kViews && atlas.Surfaces().size() == 2,
        "four independent views retain bark and leaf materials");
  if (atlas.Surfaces().size() != 2) { return; }
  CHECK(atlas.Surfaces()[0].Roughness == species.ShadingParams().BarkRoughness &&
            atlas.Surfaces()[1].Roughness == species.ShadingParams().LeafRoughness,
        "atlas materials retain declared roughness without baking illumination");
  CHECK(!Render::BuildImpostorSurface(atlas, atlas.Views().size()),
        "an absent crown view is refused");
  const auto fine = tree.GeometryAt(0);
  CHECK(fine.has_value(), "fine geometry remains available for visual comparison");
  for (size_t view = 0; view < atlas.Views().size(); ++view) {
    CheckIntrinsicCapture(atlas, view);
    const auto card = Render::BuildImpostorSurface(atlas, view);
    CHECK(card.has_value(), "captured crown exports native geometry");
    if (!card) { continue; }
    CheckMaterialMaps(atlas, view, *card);
    CheckLighting(atlas, view, *card, fine);
  }
}

constexpr double kInstanceFarExtents = 15;
constexpr uint64_t kTreeBody = 17;
constexpr uint64_t kAbsentBody = 18;

struct InstanceScene {
  Render::SceneRenderer Renderer;
  std::unique_ptr<Core::RuntimeScene> Live;
  std::string Error;
};

bool OpenInstances(InstanceScene &scene, const ImpostorAtlas &atlas) {
  const auto card = Render::BuildImpostorSurface(atlas, 0);
  CHECK(card.has_value(), "crown piece fixture starts from the native card reference");
  if (!card) { return false; }
  Core::Declaration declaration;
  declaration.InitialGeometry = &*card;
  declaration.SurfaceWidthPx = kInstanceExtent.WidthPx;
  declaration.SurfaceHeightPx = kInstanceExtent.HeightPx;
  declaration.Outputs = {"sceneLinear", "sceneDepth", "sceneShadingNormal"};
  declaration.KeyLux = kLightLux;
  declaration.KeyBearingDeg = kBearings[0];
  declaration.KeyElevationDeg = kElevationDeg;
  CHECK(Core::RuntimeScene::Open(scene.Renderer, declaration, nullptr, scene.Live, scene.Error),
        "crown piece Live opens with the reference lighting");
  if (!scene.Live) { return false; }
  Render::MeshInput empty;
  empty.Anchor = {{kWgs84A, 0, 0}};
  CHECK(scene.Renderer.SetGeometryMesh(empty, scene.Error),
        "reference mesh leaves while the Live coordinate anchor remains");
  return true;
}

bool Screenshot(InstanceScene &scene, const std::string &path) {
  const auto saved = App::SaveImage(scene.Renderer, kInstanceExtent, path);
  if (!saved) { scene.Error = saved.error(); }
  return saved.has_value();
}

std::array<Mat4, 2> Models(const ImpostorAtlas &atlas, Vec3 toward) {
  const Vec3 right{{toward[2], 0, -toward[0]}};
  std::array<Mat4, 2> models{};
  models[0][0] = models[0][10] = -1;
  for (size_t instance = 0; instance < models.size(); ++instance) {
    const Vec3 shift = atlas.CentreM() + right * ((instance == 0 ? -2 : 2) * atlas.HalfExtentM()) -
                       models[instance].TransformPoint(atlas.CentreM());
    for (size_t axis = 0; axis < 3; ++axis) { models[instance][12 + axis] = shift[axis]; }
  }
  return models;
}

struct Attachments {
  std::vector<float> Depth;
  std::vector<float> Normals;
};

bool ReadAttachments(InstanceScene &scene, Attachments &data) {
  const bool ready = scene.Renderer.ReadDepth(data.Depth) == Render::ReadState::Ready &&
                     scene.Renderer.ReadShadingNormal(data.Normals) == Render::ReadState::Ready &&
                     data.Depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels &&
                     data.Normals.size() == size_t{kInstanceExtent.WidthPx} * kPixels * kChannels;
  CHECK(ready, "shared crown attachments are complete");
  return ready;
}

struct PixelLocation {
  size_t X;
  size_t Y;
  size_t Instance;
};

struct PixelResult {
  bool Mismatch = false;
  double NormalError = 0;
};

PixelResult InspectPixel(const ImpostorAtlas &atlas,
                         size_t view,
                         const Attachments &data,
                         const Mat4 &model,
                         PixelLocation at) {
  const size_t source =
      (view + (at.Instance == 0 ? atlas.Views().size() / 2 : 0)) % atlas.Views().size();
  const auto &texel = atlas.Views()[source].Texels[at.Y * kPixels + at.X];
  const size_t pixel = at.Y * kInstanceExtent.WidthPx + at.X + at.Instance * 2 * kPixels;
  PixelResult result{.Mismatch = (data.Depth[pixel] > 0) != (texel.Surface > 0)};
  if (data.Depth[pixel] <= 0 || texel.Surface == 0) { return result; }
  const Vec3 expected =
      model.TransformDirection({{texel.Normal[0], texel.Normal[1], texel.Normal[2]}});
  const Vec3 actual{{data.Normals[pixel * kChannels],
                     data.Normals[pixel * kChannels + 1],
                     data.Normals[pixel * kChannels + 2]}};
  result.NormalError = NormalDifference(actual, expected);
  return result;
}

void CheckInstancePixels(const ImpostorAtlas &atlas,
                         size_t view,
                         const Attachments &data,
                         const std::array<Mat4, 2> &models) {
  size_t mismatches = 0;
  size_t middle = 0;
  double normalError = 0;
  for (size_t y = 0; y < kPixels; ++y) {
    for (size_t x = 0; x < kPixels; ++x) {
      middle += static_cast<size_t>(data.Depth[y * kInstanceExtent.WidthPx + x + kPixels] > 0);
      for (size_t instance = 0; instance < models.size(); ++instance) {
        const auto pixel = InspectPixel(
            atlas, view, data, models[instance], {.X = x, .Y = y, .Instance = instance});
        mismatches += static_cast<size_t>(pixel.Mismatch);
        normalError = std::max(normalError, pixel.NormalError);
      }
    }
  }
  std::println("crown pieces view {} coverage mismatch={} normal error={} middle={}",
               view,
               mismatches,
               normalError,
               middle);
  CHECK(mismatches == 0 && middle == 0,
        "selected crown views retain source coverage with no unplaced prototype in the gap");
  CHECK(normalError <= 4 * std::sqrt(3.0) / kByteSteps,
        "shared tangent frames preserve rotated captured crown normals");
}

void CheckInstanceViews(InstanceScene &scene,
                        const ImpostorAtlas &atlas,
                        Render::ImpostorInstances &crowns) {
  const double extent = atlas.HalfExtentM();
  for (size_t view = 0; view < atlas.Views().size(); ++view) {
    const Vec3 toward = atlas.Views()[view].TowardEye;
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = atlas.CentreM() + toward * (9 * extent), .AimM = atlas.CentreM()}, 0.0);
    CHECK(camera.has_value(), "shared crown camera has a valid basis");
    if (!camera) { continue; }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = 3 * extent;
    camera->YMagM = extent;
    camera->ZNearM = extent;
    camera->ZFarM = kInstanceFarExtents * extent;
    scene.Live->Eye(*camera);
    const auto models = Models(atlas, toward);
    CHECK(crowns.Update(models, camera->EyeM, scene.Error) && scene.Live->Draw(scene.Error),
          "crown view selection draws rotated and translated instances");
    scene.Renderer.WaitForGpu();
    Attachments data;
    if (!ReadAttachments(scene, data)) { continue; }
    CheckInstancePixels(atlas, view, data, models);
    CHECK(scene.Renderer.PieceTriangles() == 2 * atlas.Views().size(),
          "view switching retains the same prototype triangle count");
    CHECK(Screenshot(scene, OutputDirectory() + "/pieces-" + std::to_string(view) + ".png"),
          "shared crown PNG is written");
    CHECK(crowns.Update({}, camera->EyeM, scene.Error) && scene.Live->Draw(scene.Error),
          "empty instance groups deactivate every view");
    scene.Renderer.WaitForGpu();
    CHECK(scene.Renderer.ReadDepth(data.Depth) == Render::ReadState::Ready &&
              std::ranges::all_of(data.Depth, [](const float value) { return value == 0; }),
          "empty crown groups draw no remaining instances");
  }
}

uint32_t FindBirch(const Generators::BuiltinGenerators &catalogue, const TreeSpecies &species) {
  uint32_t birch = 0;
  while (const auto *tree = catalogue.TreeFor(Generators::ClusterId{birch})) {
    if (tree->Definition() == species.Definition()) { break; }
    ++birch;
  }
  CHECK(catalogue.TreeFor(Generators::ClusterId{birch}) != nullptr,
        "the loaded atlas resolves to its actual catalogue cluster");
  return birch;
}

void CheckWorldCoverage(InstanceScene &scene, const ImpostorAtlas &atlas) {
  std::vector<float> depth;
  CHECK(scene.Renderer.ReadDepth(depth) == Render::ReadState::Ready &&
            depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels,
        "world crown depth is complete");
  size_t mismatch = 0;
  if (depth.size() == size_t{kInstanceExtent.WidthPx} * kPixels) {
    for (size_t y = 0; y < kPixels; ++y) {
      for (size_t x = 0; x < static_cast<size_t>(kInstanceExtent.WidthPx); ++x) {
        const bool expected =
            x >= kPixels && x < 2 * size_t{kPixels} &&
            atlas.Views()[0].Texels[y * size_t{kPixels} + x - kPixels].Surface > 0;
        mismatch += static_cast<size_t>((depth[y * kInstanceExtent.WidthPx + x] > 0) != expected);
      }
    }
  }
  CHECK(mismatch == 0,
        "world consumer preserves the captured crown coverage at its geographic root");
  CHECK(Screenshot(scene, OutputDirectory() + "/world.png"), "world crown consumer PNG is written");
  CHECK(scene.Renderer.PieceTriangles() == 2 * kViews,
        "one geographic tree retains only the four shared view prototypes");
}

void CheckMissing(InstanceScene &scene,
                  Tasks &worker,
                  const Generators::BuiltinGenerators &catalogue,
                  std::span<const WorldInstance> placements,
                  const TangentFrame &frame,
                  VegetationStreaming::Config config) {
  config.Shape.Views = kViews + 1;
  const auto began = std::chrono::steady_clock::now();
  {
    auto absent = VegetationStreaming::Create(
        scene.Renderer, worker, catalogue, placements, frame, config, scene.Error);
    CHECK(absent != nullptr, "an absent capture shape can request its artifact");
    if (absent) {
      for (size_t attempt = 0; attempt < kMissingPolls; ++attempt) {
        CHECK(
            absent->Step({}, false, VegetationStreaming::ResourcePublication::Allowed, scene.Error),
            "realtime polling defers a missing crown instead of baking it");
        std::this_thread::sleep_for(kPollInterval);
      }
      CHECK(!absent->Ready() && absent->Resident() == 0 && scene.Renderer.PiecesStanding() == 0,
            "missing crown remains explicitly nonresident");
    }
  }
  CHECK(std::chrono::steady_clock::now() - began < std::chrono::seconds(1),
        "missing crown destruction drains IO without waiting for an unrequested expensive bake");
}

void CheckWorld(InstanceScene &scene,
                const ImpostorAtlas &atlas,
                const TreeSpecies &species,
                Tasks &worker) {
  outshine::Generators::GroundMaterialCatalog materials;
  outshine::Generators::RegionSurfaceRecipes vegetation;
  Generators::BuiltinGenerators catalogue;
  CHECK(materials.Load("data/world/ground-materials.json") &&
            vegetation.Load("data/world/vegetation.json", materials) &&
            catalogue.EnsureCatalogue(vegetation, "data/world/species", scene.Error),
        "world crown catalogue loads through the production species path");
  const auto birch = FindBirch(catalogue, species);
  const std::array<WorldInstance, 2> placements{
      {{.Body = kTreeBody, .Cluster = birch, .Where = {}},
       {.Body = kAbsentBody, .Cluster = ~0u, .Where = {}}}};
  const VegetationStreaming::Config config{.Cache = {.Directory = OutputDirectory() + "/cache"},
                                           .Shape = {.Pixels = kPixels, .Views = kViews}};
  const auto frame = TangentFrame::At({});
  auto world = VegetationStreaming::Create(
      scene.Renderer, worker, catalogue, placements, frame, config, scene.Error);
  CHECK(world && world->Wanted() == 1 && !world->Ready(),
        "only tree clusters request resident crown prototypes");
  if (world) {
    const double extent = atlas.HalfExtentM();
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = atlas.CentreM() + Vec3{{0, 0, 9 * extent}}, .AimM = atlas.CentreM()}, 0.0);
    CHECK(camera.has_value(), "world crown camera has a valid basis");
    if (camera) {
      camera->Kind = Render::CameraKind::Orthographic;
      camera->XMagM = 3 * extent;
      camera->YMagM = extent;
      camera->ZNearM = extent;
      camera->ZFarM = kInstanceFarExtents * extent;
      scene.Live->Eye(*camera);
      bool stepped = true;
      const auto began = std::chrono::steady_clock::now();
      while (!world->Ready() && std::chrono::steady_clock::now() - began < kLoadTimeout) {
        if (!world->Step(camera->EyeM,
                         false,
                         VegetationStreaming::ResourcePublication::Allowed,
                         scene.Error)) {
          stepped = false;
          break;
        }
        std::this_thread::sleep_for(kPollInterval);
      }
      CHECK(stepped && world->Ready() && world->Resident() == 1,
            "world crown consumer loads the matching artifact without preparation");
      CHECK(scene.Live->Draw(scene.Error), "geographically placed resident crown draws");
      scene.Renderer.WaitForGpu();
      CheckWorldCoverage(scene, atlas);
    }
  }
  world.reset();
  CHECK(scene.Renderer.PiecesStanding() == 0,
        "world crown destruction releases all resident prototypes");
  CheckMissing(scene, worker, catalogue, placements, frame, config);
}

void CheckInstances(const ImpostorAtlas &atlas, const TreeSpecies &species, Tasks &worker) {
  InstanceScene scene;
  if (!OpenInstances(scene, atlas)) { return; }
  CHECK(!Render::ImpostorInstances::Create(scene.Renderer, atlas, 0, scene.Error),
        "crown capacity must be positive");
  auto crowns = Render::ImpostorInstances::Create(scene.Renderer, atlas, 2, scene.Error);
  CHECK(crowns && scene.Renderer.PieceTriangles() == 2 * atlas.Views().size(),
        "one two-triangle prototype stands per captured view");
  if (!crowns) { return; }
  const std::array<Mat4, 3> excessive{};
  CHECK(!crowns->Update(excessive, {}, scene.Error),
        "crown grouping refuses more instances than its capacity");
  scene.Error.clear();
  CheckInstanceViews(scene, atlas, *crowns);
  crowns.reset();
  CHECK(scene.Renderer.PiecesStanding() == 0,
        "destroying the crown owner releases every view prototype");
  CheckWorld(scene, atlas, species, worker);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  std::ifstream input("data/world/species/birch.json");
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
  CHECK(!BakeImpostorAtlas(*tree, {.Pixels = 2, .Views = kViews}, error),
        "an atlas with no interior pixels is refused");
  const auto began = std::chrono::steady_clock::now();
  Foreground foreground;
  if (!OpenForeground(foreground)) { return Report(); }
  Tasks worker(1);
  auto atlas = CaptureAtlas(*tree, worker, foreground);
  if (!atlas) { return Report(); }
  const auto bytes = CheckStorage(*atlas, species, text);
  if (!bytes) { return Report(); }
  const auto provenance =
      ImpostorAtlasProvenance(species.Definition(), {.Pixels = kPixels, .Views = kViews});
  if (!CheckCache(atlas, *bytes, provenance, worker) || !atlas) { return Report(); }
  CheckViews(*atlas, *tree, species);
  CheckInstances(*atlas, species, worker);
  std::println(
      "atlas capture, checks and PNG export {:.3f} ms; payload {} bytes; no world frame-rate claim",
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count(),
      atlas->Views().size() * kTexels * sizeof(ImpostorAtlas::Texel));
  return Report();
}
