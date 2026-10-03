#include <Outshine.h>
#include <world/Provider.h>
#include "Check.h"

#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <cstdlib>
#include <cstdio>

namespace {
using namespace outshine;
using namespace outshine::Data;

constexpr std::array<uint8_t, 852> kPlane42 = {
    137, 80,  78,  71,  13,  10,  26,  10,  0,   0,   0,   13,  73,  72,  68,  82,  0,   0,   1,
    0,   0,   0,   1,   0,   8,   2,   0,   0,   0,   211, 16,  63,  49,  0,   0,   3,   27,  73,
    68,  65,  84,  120, 156, 237, 206, 65,  13,  0,   48,  8,   0,   49,  244, 76,  37,  210, 39,
    227, 30,  52,  169, 128, 206, 190, 129, 179, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,
    132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132,
    250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250,
    1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 250, 1,   132, 62,  228, 23,  9,   247,
    95,  231, 30,  46,  0,   0,   0,   0,   73,  69,  78,  68,  174, 66,  96,  130};

struct Calls {
  std::atomic<int> Configured{0};
  std::atomic<int> Acquired{0};
  std::atomic<int> Decoded{0};
};

enum class Encoding { TerrariumFixture, OpaqueFixture };

class PlaneSource final : public Source {
public:
  PlaneSource(Calls &calls, Encoding encoding) : Calls_(calls), Encoding_(encoding) {
    Decl_.Id = "public.fixture.dem";
    Decl_.Revision = "plane-42m";
    Decl_.MaxZoom = 15;
    Decl_.Latency = LatencyClass::Local;
    Decl_.Keeps = Cacheability::Never;
    Decl_.MaximumPayloadBytes = kPlane42.size();
    if (Encoding_ == Encoding::OpaqueFixture) { Decl_.Wire = WireFormat::ProviderDefined; }
  }

  const SourceDecl &Declaration() const noexcept override { return Decl_; }

  Coverage Covers(const Fetch &request) const noexcept override {
    const auto tile = request.Where().Tile();
    if (request.Kind() != DataKind::Elevation || !tile || tile->Zoom < 0 || tile->Zoom > 15) {
      return Coverage::Outside;
    }
    const auto side = uint32_t{1} << static_cast<uint32_t>(tile->Zoom);
    return tile->X < side && tile->Y < side ? Coverage::Inside : Coverage::Outside;
  }

  Address Serves(const Fetch &request) const noexcept override { return request.Where(); }

  FetchStart Begin(const Address &, Transport &) const override { return Ticket::None; }

  Fetched Collect(const Address &, Ticket, Transport &) const override {
    ++Calls_.Acquired;
    if (Encoding_ == Encoding::OpaqueFixture) { return Fetched::Delivered({42, 17}); }
    return Fetched::Delivered(std::vector<uint8_t>(kPlane42.begin(), kPlane42.end()));
  }

  std::expected<HeightRaster, DecodeFailure>
  DecodeElevation(std::span<const uint8_t> bytes) const override {
    if (Encoding_ == Encoding::TerrariumFixture) {
      return std::unexpected(DecodeFailure::Unsupported);
    }
    ++Calls_.Decoded;
    if (bytes.size() != 2 || bytes[0] != 42 || bytes[1] != 17) {
      return std::unexpected(DecodeFailure::CorruptPayload);
    }
    return HeightRaster{.Rows = 8, .Cols = 8, .Meters = std::vector<float>(64, 42.0f)};
  }

private:
  Calls &Calls_;
  Encoding Encoding_;
  SourceDecl Decl_;
};

class PlaneProvider final : public Provider {
public:
  explicit PlaneProvider(Encoding encoding) : Encoding_(encoding) {}

  mutable Calls Observed;

  std::string_view kind() const override { return "terrain"; }

  std::expected<std::unique_ptr<Source>, std::string> make(const SourceProvider &,
                                                           std::string_view) const override {
    ++Observed.Configured;
    return std::make_unique<PlaneSource>(Observed, Encoding_);
  }

private:
  Encoding Encoding_;
};
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  for (const auto encoding : {Encoding::TerrariumFixture, Encoding::OpaqueFixture}) {
    PlaneProvider provider(encoding);
    Engine engine;
    CHECK(engine.registerProvider(provider), "external provider registers through the public API");
    CHECK(!engine.registerProvider(provider), "duplicate registration preserves the first factory");
    CHECK(engine.setRoots({.Shipped = "src/assets", .Offline = true}), "offline roots configured");
    CHECK(engine.setRenderTarget(Extent{64, 64}), "offscreen target configured");
    Scenario::Document scene;
    scene.Ground.Declared = true;
    scene.Ground.VegetationEnabled = false;
    scene.Ground.Origin.LatitudeDeg = 47;
    scene.Ground.Origin.LongitudeDeg = 8;
    scene.Ground.Origin.RadiusM = 1000;
    scene.Render.Declared = true;
    scene.Render.Frame = {64, 64};
    scene.Render.Outputs = {"sceneLinear", "sceneDepth"};
    scene.Lit.Declared = true;
    scene.Lit.Key.Lux = 40000;
    scene.Lit.Key.ElevationDeg = 60;
    Scenario::View view;
    view.Id = "plane";
    view.Person = "first";
    view.Placement = Scenario::CameraPlacement::Geodetic;
    view.Geographic.Geodetic = {.LongitudeDeg = 8, .LatitudeDeg = 47, .HeightM = 80};
    view.Geographic.PitchDeg = -30;
    view.Sees.FovDeg = 55;
    scene.Views.push_back(view);
    const auto declared = engine.declare(scene);
    CHECK(declared, "library terrain needs no unrelated OSM provider");
    const auto assembled = declared ? engine.assemble() : declared;
    CHECK(assembled, assembled ? "world assembled" : assembled.error().c_str());
    if (assembled) {
      const auto loaded = engine.preload(10.0);
      CHECK(loaded, loaded ? "world prepared" : loaded.error().c_str());
      const auto height = engine.sampleHeight({.LongitudeDeg = 8, .LatitudeDeg = 47});
      CHECK(height && std::abs(*height - 42.0) < 0.001,
            "public source bytes become the exact independent 42m terrain plane");
      CHECK(provider.Observed.Configured > 0 && provider.Observed.Acquired > 0,
            "runtime invokes both the registered factory and its configured source");
      CHECK((provider.Observed.Decoded > 0) == (encoding == Encoding::OpaqueFixture),
            "provider-owned native decoding handles opaque bytes without a private engine route");
      std::vector<float> pixels;
      CHECK(engine.advance() && engine.renderer().render({}) &&
                engine.renderer().readPixels(Buffer::Linear, pixels),
            "the provider-driven world renders through the public API");
      CHECK(pixels.size() == 64 * 64 * 4 &&
                std::ranges::all_of(pixels, [](float value) { return std::isfinite(value); }),
            "a complete finite rendered frame reaches the client");
      std::vector<float> depth;
      const auto readDepth = engine.renderer().readPixels(Buffer::Depth, depth);
      CHECK(readDepth, readDepth ? "depth captured" : readDepth.error().c_str());
      CHECK(readDepth &&
                std::ranges::any_of(depth, [](float value) { return value > 0 && value < 1; }),
            "the frame contains rasterized terrain, not only a clear or sky attachment");
      if (const char *path = std::getenv("OUTSHINE_PROVIDER_CAPTURE")) {
        std::vector<uint8_t> colour;
        CHECK(engine.renderer().readPixels(colour), "display pixels captured");
        if (colour.size() == 64 * 64 * 4) {
          const auto file = std::unique_ptr<std::FILE, decltype(&std::fclose)>(
              std::fopen(path, "wb"), &std::fclose);
          CHECK(file != nullptr, "explicit diagnostic output opened");
          if (file) {
            CHECK(std::fprintf(file.get(), "P6\n64 64\n255\n") > 0, "diagnostic header written");
            std::array<uint8_t, 64 * 64 * 3> rgb{};
            for (size_t pixel = 0; pixel < 64 * 64; ++pixel) {
              std::copy_n(colour.data() + pixel * 4, 3, rgb.data() + pixel * 3);
            }
            CHECK(std::fwrite(rgb.data(), 1, rgb.size(), file.get()) == rgb.size(),
                  "diagnostic pixels written");
          }
        }
      }
    }
  }
  SDL_Quit();
  return Report();
}
