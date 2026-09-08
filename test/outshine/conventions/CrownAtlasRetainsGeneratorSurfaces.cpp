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
  if (!SDL_Init(SDL_INIT_VIDEO)) { Unprepared(SDL_GetError()); return Report(); }
  std::string error;
  CHECK(!CrownAtlas::Bake(*tree, {.Pixels=2,.Views=4}, error), "an atlas with no interior pixels is refused");
  const auto started = std::chrono::steady_clock::now();
  const auto atlas = CrownAtlas::Bake(*tree, {.Pixels=128,.Views=4}, error);
  CHECK(atlas.has_value(), "the native renderer captures crown surface data");
  if (!atlas) { std::printf("%s\n", error.c_str()); return Report(); }
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
    CHECK(texels.size() == 128u*128u, "every view has its full pixel population");
    std::array<size_t,3> covered{};
    bool valid = true;
    size_t depthMismatch = 0, badNormal = 0;
    float leastNormal = 1, mostNormal = 0;
    std::vector<uint8_t> rgba(texels.size()*4, 0);
    for (size_t at = 0; at < texels.size(); ++at) {
      const auto &pixel = texels[at];
      valid = valid && std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1;
      depthMismatch += (pixel.Depth > 0) != (pixel.Surface > 0);
      valid = valid && ((pixel.Depth > 0) == (pixel.Surface > 0));
      if (pixel.Surface == 0 || pixel.Surface > 2) { continue; }
      ++covered[pixel.Surface];
      const float lengthSquared = Dot(pixel.Normal, pixel.Normal);
      badNormal += !(std::abs(lengthSquared-1.0f) < 0.003f);
      leastNormal = std::min(leastNormal, lengthSquared);
      mostNormal = std::max(mostNormal, lengthSquared);
      valid = valid && std::abs(lengthSquared-1.0f) < 0.003f;
      const auto &colour = atlas->Surfaces()[pixel.Surface-1].BaseColour;
      for (size_t c = 0; c < 3; ++c) {
        const float linear = colour[c];
        const float srgb = linear <= 0.0031308f ? 12.92f*linear : 1.055f*std::pow(linear,1.0f/2.4f)-0.055f;
        rgba[at*4+c] = static_cast<uint8_t>(std::lround(srgb*255.0f));
      }
      rgba[at*4+3] = 255;
    }
    std::printf("view %zu depthMismatch=%zu badNormal=%zu norm2=[%g,%g]\n", view, depthMismatch, badNormal, static_cast<double>(leastNormal), static_cast<double>(mostNormal));
    CHECK(valid, "depth, coverage, surface identities and unit normals agree");
    CHECK(covered[1] > 0 && covered[2] > 0, "each crown view contains both bark and foliage");
    std::vector<uint8_t> png;
    CHECK(Core::EncodePng(rgba.data(), 128, 128, png), "intrinsic crown colour encodes as a PNG");
    std::ofstream output("build/crown-atlas/birch-"+std::to_string(view)+".png", std::ios::binary);
    output.write(reinterpret_cast<const char *>(png.data()), static_cast<std::streamsize>(png.size()));
    CHECK(output.good(), "crown reference PNG is written");
    std::printf("view %zu bark=%zu leaf=%zu sampled pixels\n",view,covered[1],covered[2]);
    const auto card = atlas->GeometryAt(view);
    CHECK(card.has_value(), "captured crown exports native geometry");
    if (!card) { continue; }
    const auto material = card->surfaceAt(MaterialInstance(0));
    const auto colourMap = card->imageAt(material.BaseColourMap.Image);
    const auto mrMap = card->imageAt(material.MetalRoughMap.Image);
    bool coverageMatches = colourMap.stands(), materialsMatch = mrMap.stands();
    if (coverageMatches && materialsMatch) {
      for (size_t pixel = 0; pixel < texels.size(); ++pixel) {
        coverageMatches &= (colourMap.Rgba[pixel*4+3] == (texels[pixel].Surface ? 255 : 0));
        if (!texels[pixel].Surface) { continue; }
        const auto &original = atlas->Surfaces()[texels[pixel].Surface-1];
        materialsMatch &= std::abs(mrMap.Rgba[pixel*4+1]/255.0f-original.Roughness) <= 1.0f/255;
        materialsMatch &= std::abs(mrMap.Rgba[pixel*4+2]/255.0f-original.Metalness) <= 1.0f/255;
      }
    }
    CHECK(coverageMatches, "native colour alpha preserves every captured hole");
    CHECK(materialsMatch, "native MR channels retain source material values within one code");
    CHECK(material.NormalMap.bound() && material.BaseColourMap.Sampler.Mip == MipFilter::Linear,
          "the card carries normal detail and retains mip filtering");
    std::array<std::vector<float>,2> frames;
    for (size_t light = 0; light < frames.size(); ++light) {
      Scenario::Document scene;
      scene.Render.Declared = true;
      scene.Render.Frame = {128,128};
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
      camera.Sees.Stands.AtM = atlas->CentreM() + atlas->Views()[view].TowardEye*(3*extent);
      camera.Sees.LooksAt = true;
      camera.Sees.LookAtM = atlas->CentreM();
      camera.Sees.setProjection(Scenario::Camera::Ortho{
          .XMagM=extent,.YMagM=extent,.NearM=extent,.FarM=5*extent});
      scene.Views.push_back(camera);
      Engine engine;
      std::vector<float> depth, normals;
      const bool rendered = engine.drawsInto({128,128}) && engine.declare(scene) &&
          engine.setGeometry(*card) && engine.assemble() && engine.advance() &&
          engine.renderer().render({}) && engine.renderer().readPixels(Buffer::Linear,frames[light]) &&
          engine.renderer().readPixels(Buffer::Depth,depth) &&
          engine.renderer().readPixels(Buffer::ShadingNormal,normals);
      CHECK(rendered, "native crown card renders with live lighting");
      CHECK(depth.size()==texels.size() && normals.size()==texels.size()*4 && frames[light].size()==texels.size()*4, "card attachments cover the complete frame");
      if (!rendered || depth.size()!=texels.size() || normals.size()!=texels.size()*4) { continue; }
      size_t mismatches = 0;
      double normalError = 0;
      for (size_t pixel=0; pixel<texels.size(); ++pixel) {
        mismatches += ((depth[pixel]>0) != (texels[pixel].Surface>0));
        if (!texels[pixel].Surface || depth[pixel]<=0) { continue; }
        Vec3 actual{{normals[pixel*4],normals[pixel*4+1],normals[pixel*4+2]}};
        Vec3 expected{{texels[pixel].Normal[0],texels[pixel].Normal[1],texels[pixel].Normal[2]}};
        if (!Normalise(actual) || !Normalise(expected)) { normalError=2; continue; }
        const auto delta = actual-expected;
        normalError = std::max(normalError,std::sqrt(Dot(delta,delta)));
      }
      std::printf("card %zu light %zu coverage mismatch=%zu normal error=%g\n",view,light,mismatches,normalError);
      CHECK(mismatches == 0, "card raster coverage equals source capture at its camera");
      CHECK(normalError <= 4*std::sqrt(3.0)/255, "rendered normals retain source directions within RGBA8 quantisation bound");
      CHECK(engine.renderer().saveScreenshot("build/crown-atlas/card-"+std::to_string(view)+"-light-"+std::to_string(light)+".png").has_value(), "lit crown card PNG is written");
      if (view == 0 && fine) {
        Engine reference;
        std::vector<float> original;
        const bool drawn = reference.drawsInto({128,128}) && reference.declare(scene) &&
            reference.setGeometry(*fine) && reference.assemble() && reference.advance() &&
            reference.renderer().render({}) && reference.renderer().readPixels(Buffer::Linear,original);
        CHECK(drawn, "fine tree renders under the identical camera and light");
        if (drawn && original.size() == frames[light].size()) {
          double difference=0, energy=0;
          for (size_t at=0; at<original.size(); ++at) {
            if (at%4==3) { continue; }
            difference += std::abs(original[at]-frames[light][at]);
            energy += std::abs(original[at]);
          }
          std::printf("card/fine light %zu relative linear RGB L1=%g; diagnostic, no quality acceptance\n",light,energy>0 ? difference/energy : 0);
        }
        CHECK(reference.renderer().saveScreenshot("build/crown-atlas/fine-0-light-"+std::to_string(light)+".png").has_value(), "fine comparison PNG is written");
      }

    }
    CHECK(!frames[0].empty() && frames[0]!=frames[1], "changing the light relights captured crown surfaces");

  }
  std::printf("atlas capture, checks and PNG export %.3f ms; payload %zu bytes; no world frame-rate claim\n",
              std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),
              atlas->Views().size()*128u*128u*sizeof(CrownAtlas::Texel));
  return Report();
}
