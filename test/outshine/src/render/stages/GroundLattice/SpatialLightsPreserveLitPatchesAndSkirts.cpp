#include "test/harness/render/NativeGroundLatticeProbe.h"
#include "LightCells.h"
#include "LightCellFrame.h"

namespace {
using namespace outshine::Test::GroundProbe;
constexpr size_t kSources = 17;
constexpr float kIntensity = 20;
constexpr float kRange = 8;

LightCells Sources() {
  std::array<SubjectLight, kSources> lights;
  for (size_t i = 0; i < lights.size(); ++i) {
    lights[i].Light.Kind = LightKind::Point;
    lights[i].Light.RangeM = kRange;
    lights[i].Light.Intensity = i + 1 == lights.size() ? kIntensity : 0;
    lights[i].PositionEcefM = {{i + 1 == lights.size() ? 0 : 128.0 * (i + 1), 0, 2}};
  }
  auto cells = BuildLightCells(lights,
                               {.EmitterBytes = 1024 * 1024,
                                .IndexBytes = 1024 * 1024,
                                .ScratchBytes = 1024 * 1024,
                                .SphereBoxTests = 1000000});
  CHECK(cells && !cells->FullEnumeration,
        "the actual producer keeps all seventeen finite sources in the spatial index");
  if (!cells) { Failed(); }
  return std::move(*cells);
}

void Populate(GroundLattice &ground) {
  std::array<float, GroundLattice::kPageNodes> heights;
  heights.fill(kHeightBase);
  const auto page =
      Require(RetryOffline([&] { return ground.PlacePage(heights); }, std::chrono::seconds(5)));
  const std::array tiles{Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  Require(RetryOffline([&] { return ground.SetInstances(tiles, {}); }, std::chrono::seconds(5)));
}

std::unique_ptr<Texture> BlackTexture(Device &device, Commands &commands) {
  auto texture =
      Require(device.CreateTexture({.Width = 1,
                                    .Height = 1,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::Sampled}));
  const std::array attachment{
      ColourAttachment{.Image = {.Image = texture.get()}, .Begin = Load::Clear}};
  Require(commands.BeginRenderPass({.Colours = attachment, .Depth = {}}));
  Require(commands.EndRenderPass());
  return texture;
}

struct Inputs {
  SubjectView View;
  SubjectLightingUniform Lighting;
  LightCellFrame Frame;
  PackedSubjectMaterial Material;
  std::unique_ptr<Buffer> Classes, Palette, Emitters, Index, OpaqueDepth;
  std::unique_ptr<Texture> Black;
  std::unique_ptr<Native::Sampler> Sampling;
};

Inputs Prepare(Device &device, Commands &commands, const LightCells &cells) {
  Inputs input;
  input.View.Current[5] = -1;
  input.Lighting.Up = {{0, 0, 1, 0}};
  input.Lighting.ViewPosition = {{0, 0, 2, 1}};
  input.Lighting.Count[0] = 1;
  input.Lighting.Items[0] = cells.Emitters.back();
  auto frame = MakeLightCellFrame({}, LightCellLookup::Hash);
  CHECK(frame, "the origin camera has a valid spatial light frame");
  if (!frame) { Failed(); }
  input.Frame = *frame;
  input.Material = PackSubjectMaterial(SubjectMaterial{}, 1);
  const std::array<uint32_t, 3> classes{0, 0, ~0u};
  input.Classes = Upload(device, commands, classes);
  std::array<float, kPaletteFloats> palette{};
  palette[0] = std::bit_cast<float>(1u);
  palette[2] = 1;
  for (size_t row = 0; row < 2; ++row) {
    for (size_t channel = 0; channel < 4; ++channel) {
      palette[4 + row * 4 + channel] = kSurface[channel];
    }
  }
  palette[12] = palette[13] = kNoRockSlope;
  std::array<uint32_t, kPaletteFloats> paletteWords;
  std::memcpy(paletteWords.data(), palette.data(), sizeof(palette));
  input.Palette = Upload(device, commands, paletteWords);
  std::vector<uint32_t> emitterWords(cells.Emitters.size() * sizeof(SubjectLightUniform) /
                                     sizeof(uint32_t));
  std::memcpy(emitterWords.data(), cells.Emitters.data(), emitterWords.size() * sizeof(uint32_t));
  input.Emitters = Upload(device, commands, emitterWords);
  input.Index = Upload(device, commands, cells.Words);
  input.Black = BlackTexture(device, commands);
  input.Sampling = Require(device.CreateSampler({}));
  return input;
}

std::vector<Binding> Bindings(const Inputs &input, bool spatial) {
  std::vector<Binding> bindings{
      {.Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&input.View, 1))}},
      {.Set = 2,
       .Slot = 0,
       .Value =
           SampledTexture{.Image = {.Image = input.Black.get()}, .Sampling = input.Sampling.get()}},
      {.Set = 2,
       .Slot = 1,
       .Value =
           SampledTexture{.Image = {.Image = input.Black.get()}, .Sampling = input.Sampling.get()}},
      Storage(2, 2, *input.Classes),
      Storage(2, 3, *input.Palette),
      {.Set = 3, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(input.Material))}},
      {.Set = 3,
       .Slot = 1,
       .Value = Uniform{.Bytes = std::as_bytes(std::span(&input.Lighting, 1))}}};
  if (spatial) {
    bindings.push_back(Storage(2, 24, *input.Emitters));
    bindings.push_back(Storage(2, 25, *input.Index));
    bindings.push_back({.Set = 3,
                        .Slot = 4,
                        .Value = Uniform{.Bytes = std::as_bytes(std::span(&input.Frame, 1))}});
  }
  return bindings;
}

std::vector<float> Capture(Device &device, const LightCells &cells, bool spatial, bool adaptive) {
  GroundPipelineBinding pipelines;
  GroundLattice ground;
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  Require(RetryOffline([&] { return ground.Configure(pipelines, device, {}, targets); },
                       std::chrono::seconds(5)));
  if (spatial) { Require(pipelines.ConfigureLit(device, {}, targets, true)); }
  ground.UseSpatialLights(spatial);
  Populate(ground);
  auto colour = Target(device, Format::Rgba32Float, false);
  auto depth = Target(device, Format::Depth32Float, true);
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  auto input = Prepare(device, *commands, cells);
  FrameContext frame;
  frame.Mvp = input.View.Current;
  Require(ground.Cull(frame, {}, *commands));
  CHECK(ground.Drawn() == 2, "both terrain tiles reach the actual patch and skirt consumers");
  StageSubmission submission;
  const PassRecording pass{.Commands = commands.get(),
                           .Submission = submission,
                           .Area = {.Width = kWidth, .Height = kHeight},
                           .Clip = {.Width = kWidth, .Height = kHeight}};
  if (adaptive) {
    Require(ground.ConfigurePatchSelection());
    const auto shape = PyramidOver({.WidthPx = kWidth, .HeightPx = kHeight});
    input.OpaqueDepth = Upload(device, *commands, std::vector<uint32_t>(shape.Texels));
    Require(ground.SelectPatches(frame, {}, pass, *input.OpaqueDepth, shape));
  }
  const std::array attachments{
      ColourAttachment{.Image = {.Image = colour.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass(
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Require(ground.Encode(pass, Bindings(input, spatial)));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "spatial terrain patches and skirts finish within the explicit offline budget");
  return Read(device, *colour, 4);
}

void Compare(std::span<const float> actual, std::span<const float> reference) {
  double peak = 0, signal = 0, error = 0, maximum = 0;
  size_t nonfinite = 0;
  for (size_t pixel = 0; pixel < kPixels; ++pixel) {
    for (size_t channel = 0; channel < 3; ++channel) {
      const size_t at = pixel * 4 + channel;
      peak = std::max(peak, static_cast<double>(reference[at]));
      const double difference = static_cast<double>(actual[at]) - reference[at];
      maximum = std::max(maximum, std::abs(difference));
      signal += static_cast<double>(reference[at]) * reference[at];
      error += difference * difference;
      nonfinite += !std::isfinite(actual[at]) || !std::isfinite(reference[at]);
    }
  }
  CHECK(nonfinite == 0, "every RGB value remains finite");
  CHECK(signal > 1 && peak > 0, "the seventeenth source visibly illuminates actual terrain");
  CHECK(maximum <= 0.02 * peak && error <= 0.005 * 0.005 * signal,
        "all spatial terrain RGB values preserve the full active-light BRDF reference");
}
}

int main() {
  using namespace outshine::Test::GroundProbe;
  auto device = Require(CreateDevice({.Validation = true}));
  const auto cells = Sources();
  for (const bool adaptive : {false, true}) {
    const auto reference = Capture(*device, cells, false, adaptive);
    const auto actual = Capture(*device, cells, true, adaptive);
    Compare(actual, reference);
  }
  return Report();
}
