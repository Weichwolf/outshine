#include "src/render/stages/MediumTransmittanceStage.h"
#include "src/render/stages/MediumMultiScatterStage.h"
#include "src/render/stages/MediumRadianceStage.h"
#include "src/render/stages/IrradianceStage.h"
#include "src/render/stages/EnvironmentSpecularStage.h"
#include "src/render/stages/EnvironmentSpecularLayout.h"
#include "src/render/device/ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Products = std::array<std::vector<std::byte>, 5>;
constexpr std::array kNames{"trans", "multi", "radiance", "environment", "irradiance"};
constexpr float kSunZenith = 0.6f;
constexpr float kEyeHeightM = 100;
constexpr Vec3 kGround{{0.2, 0.3, 0.4}};

std::unique_ptr<Texture> Image(Device &device, uint32_t width, uint32_t height) {
  return Require(device.CreateTexture(
      {.Width = width,
       .Height = height,
       .Texels = Format::Rgba16Float,
       .Use = TextureUse::Sampled | TextureUse::Storage | TextureUse::CopySource}));
}

std::vector<std::byte> Read(Device &device, ReadbackTransfer transfer) {
  const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "the real compute product completes within an explicit offline budget");
  if (!bytes) { Failed(); }
  return {bytes->begin(), bytes->end()};
}

struct Pipeline {
  Device &GpuDevice;
  std::unique_ptr<Texture> Trans, Multi, Radiance, Environment;
  std::unique_ptr<Buffer> Irradiance;
  std::unique_ptr<Sampler> Lut;
  MediumTransmittanceStage TransStage;
  MediumMultiScatterStage MultiStage;
  MediumRadianceStage RadianceStage;
  IrradianceStage IrradianceCompute;
  EnvironmentSpecularStage EnvironmentStage;

  explicit Pipeline(Device &device) : GpuDevice(device) {
    Trans = Image(device, kTransmittanceLutWidth, kTransmittanceLutHeight);
    Multi = Image(device, kMultiScatterLutSize, kMultiScatterLutSize);
    Radiance = Image(device, kSkyViewLutWidth, kSkyViewLutHeight);
    Environment = Image(device, kEnvironmentWidth, kEnvironmentHeight);
    Irradiance = Require(device.CreateBuffer({.Bytes = kIrradianceFloats * sizeof(float),
                                              .Use = BufferUse::Storage | BufferUse::CopySource}));
    Lut = Require(device.CreateSampler({.Mip = Filter::Nearest, .MaxLod = 0}));
    const outshine::Render::Gpu gpu{.Device = &device, .SceneColours = {}};
    Require(TransStage.Configure(gpu, Trans.get()));
    Require(MultiStage.Configure(gpu, Trans.get(), Lut.get(), Multi.get()));
    Require(RadianceStage.Configure(gpu, Trans.get(), Multi.get(), Lut.get(), Radiance.get()));
    Require(
        IrradianceCompute.Configure(gpu, Trans.get(), Multi.get(), Lut.get(), Irradiance.get()));
    Require(EnvironmentStage.Configure(gpu,
                                       {.Sky = Radiance.get(),
                                        .Irradiance = Irradiance.get(),
                                        .Sampler = Lut.get(),
                                        .Target = Environment.get()}));
    StageSubmission submitted;
    const auto absent = TransStage.Encode({.Submission = submitted, .Area = {}, .Clip = {}});
    CHECK(!absent && absent.error().Kind == ErrorKind::Invalid,
          "a configured stage cannot publish work without a declared medium and commands");
  }

  void Declare(const Medium &medium) {
    TransStage.Declare(medium);
    MultiStage.Declare(medium);
    RadianceStage.Declare(medium, kSunZenith, kEyeHeightM);
    IrradianceCompute.Declare(medium, kSunZenith);
    EnvironmentStage.Declare(medium, kSunZenith, kEyeHeightM, kGround);
  }

  void Encode(const PassRecording &recording) {
    Require(TransStage.Encode(recording));
    Require(MultiStage.Encode(recording));
    Require(RadianceStage.Encode(recording));
    Require(IrradianceCompute.Encode(recording));
    Require(EnvironmentStage.Encode(recording));
  }

  [[nodiscard]] bool Settled() const {
    return TransStage.Settled() && MultiStage.Settled() && RadianceStage.Settled() &&
           IrradianceCompute.Settled() && EnvironmentStage.Settled();
  }

  void Run(bool accept) {
    auto commands = Require(GpuDevice.Begin());
    StageSubmission submitted;
    Encode({.Commands = commands.get(), .Submission = submitted, .Area = {}, .Clip = {}});
    CHECK(!Settled(), "recorded work does not publish its cached products before submission");
    if (accept) {
      const auto completion = Require(GpuDevice.Submit(std::move(commands)));
      submitted.Commit();
      CHECK(Settled(), "accepted GPU work publishes every cache transaction");
      CHECK(Require(GpuDevice.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
            "the full dependent atmosphere chain completes");
    }
  }

  Products Capture() {
    Products products;
    const std::array textures{Trans.get(), Multi.get(), Radiance.get(), Environment.get()};
    for (size_t i = 0; i < textures.size(); ++i) {
      const auto &shape = textures[i]->Description();
      products[i] =
          Read(GpuDevice,
               Require(ReadbackTransfer::Start(
                   GpuDevice, *textures[i], {.Width = shape.Width, .Height = shape.Height})));
    }
    products.back() =
        Read(GpuDevice,
             Require(ReadbackTransfer::Start(
                 GpuDevice, *Irradiance, {.Bytes = Irradiance->Description().Bytes})));
    return products;
  }

  void Reuse() {
    StageSubmission submitted;
    Encode({.Submission = submitted, .Area = {}, .Clip = {}});
    CHECK(Settled(), "unchanged products need no commands or GPU dispatches");
    const auto rejected = TransStage.Configure({.Device = &GpuDevice, .SceneColours = {}}, nullptr);
    CHECK(!rejected && rejected.error().Kind == ErrorKind::Invalid && Settled(),
          "an invalid replacement preserves the previously accepted cache");
  }
};

void Export(const Products &products, std::string_view prefix, char state) {
  if (prefix.empty()) { return; }
  for (size_t i = 0; i < products.size(); ++i) {
    std::ofstream file(std::string(prefix) + state + '-' + kNames[i] + ".bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(products[i].data()),
               static_cast<std::streamsize>(products[i].size()));
    CHECK(file, "optional comparison exports contain actual GPU product bytes");
  }
}
}

int main(int argc, char **argv) {
  CHECK(argc <= 2, "the probe accepts only an optional diagnostic output prefix");
  if (argc > 2) { return Report(); }
  const std::string_view prefix = argc == 2 ? argv[1] : "";
  auto device = Require(CreateDevice({.Validation = true}));
  Pipeline pipeline(*device);
  pipeline.Declare(kEarthAir);
  pipeline.Run(false);
  CHECK(!pipeline.Settled(), "abandoning recorded work leaves every product dirty");
  pipeline.Run(true);
  const auto first = pipeline.Capture();
  Export(first, prefix, '0');
  pipeline.Declare(kEarthAir);
  pipeline.Reuse();
  auto changed = kEarthAir;
  changed.RayleighScaleHeightKm *= 2;
  pipeline.Declare(changed);
  pipeline.Run(false);
  CHECK(!pipeline.Settled() && pipeline.Capture() == first,
        "an abandoned update neither publishes cache state nor changes GPU products");
  pipeline.Run(true);
  const auto second = pipeline.Capture();
  Export(second, prefix, '1');
  for (size_t i = 0; i < first.size(); ++i) {
    CHECK(!first[i].empty() && first[i].size() == second[i].size() && first[i] != second[i],
          "the changed physical atmosphere reaches each real dependent product");
  }
  return Report();
}
