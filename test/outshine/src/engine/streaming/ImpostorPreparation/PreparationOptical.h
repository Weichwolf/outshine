#ifndef OUTSHINE_TEST_PREPARATIONOPTICAL_H
#define OUTSHINE_TEST_PREPARATIONOPTICAL_H
#include "PreparationCommon.h"
#include "Image.h"
#include "math/Srgb.h"
#include "shade/HalfTexels.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <print>

namespace outshine::Test::CrownFixture {
inline void CheckIntrinsicCapture(const Content::ImpostorAtlas &atlas, size_t view) {
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
  CHECK(Core::EncodePng(rgba.data(), kPixels, kPixels, png),
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

inline bool FullImage(ImageView image, size_t texels) {
  return image.valid() && static_cast<size_t>(image.WidthPx) * image.HeightPx == texels;
}

inline bool SheetMatches(const OpticalMaps &maps,
                         std::span<const float> moments,
                         size_t offset,
                         const Material &original,
                         const Content::ImpostorAtlas::SurfaceSample &sample) {
  bool valid = moments[offset + 3] == 0 && maps.Reflection.Rgba[offset + 3] == kOpaque;
  for (size_t c = 0; c < 3; ++c) {
    valid &= std::abs(static_cast<float>(maps.Reflection.Rgba[offset + c]) / kByteSteps -
                      sample.BaseColour[c]) <= 1.0f / kByteSteps;
    valid &= std::abs(static_cast<float>(maps.Transmission.Rgba[offset + c]) / kByteSteps -
                      original.DiffuseTransmission[c]) <= 1.0f / kByteSteps;
  }
  return valid;
}

inline bool OpaqueMatches(const OpticalMaps &maps,
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

inline void
CheckMaterialMaps(const Content::ImpostorAtlas &atlas, size_t view, const Geometry &card) {
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

inline Scenario::Document
CardScene(size_t view, const Content::ImpostorAtlas &atlas, size_t light) {
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

inline double NormalDifference(Vec3 actual, Vec3 expected) {
  if (!Normalise(actual) || !Normalise(expected)) { return 2; }
  const auto delta = actual - expected;
  return std::sqrt(Dot(delta, delta));
}

struct CardAttachments {
  std::span<const float> Depth;
  std::span<const float> Normals;
};

inline void CheckCardAttachments(const Content::ImpostorAtlas::View &source,
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

inline void CheckFine(const Geometry &fine,
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
  CHECK(reference.renderer()
            .saveScreenshot(OutputDirectory() + "/fine-0-light-" + std::to_string(light) + ".png")
            .has_value(),
        "fine comparison PNG is written");
}

inline void CheckLighting(const Content::ImpostorAtlas &atlas,
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
    CHECK(engine.renderer()
              .saveScreenshot(OutputDirectory() + "/card-" + std::to_string(view) + "-light-" +
                              std::to_string(light) + ".png")
              .has_value(),
          "lit crown card PNG is written");
    if (view == 0 && fine) { CheckFine(*fine, scene, frames[light], light); }
  }
  CHECK(!frames[0].empty() && frames[0] != frames[1],
        "changing the light relights captured crown surfaces");
}

inline void CheckViews(const Content::ImpostorAtlas &atlas,
                       const TreePrototype &tree,
                       const TreeSpecies &species) {
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
}
#endif
