#include "GpuSupport.h"
#include "SpirvFile.h"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <limits>
#include <memory>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace Native = outshine::Render::Native;
namespace Cal = outshine::Diagnostics::Calibration;

namespace {
using Json = nlohmann::json;
using Bytes = std::vector<std::byte>;

[[noreturn]] void Fail(const std::string &error) {
  std::println(stderr, "{}", error);
  std::_Exit(1);
}

template <class T> T Require(Native::Result<T> result) {
  if (!result) { Fail(result.error().Message); }
  return std::move(*result);
}

void Check(Native::Result<void> result) {
  if (!result) { Fail(result.error().Message); }
}

Bytes Read(const std::string &path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) { Fail("missing " + path); }
  const auto n = f.tellg();
  if (n <= 0 || n > 32LL * 1024 * 1024) { Fail("invalid bytes " + path); }
  Bytes bytes(static_cast<size_t>(n));
  f.seekg(0);
  f.read(reinterpret_cast<char *>(bytes.data()), n);
  if (!f) { Fail("incomplete " + path); }
  return bytes;
}

void Write(const std::string &path, std::span<const std::byte> bytes) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!f) { Fail("write " + path); }
}

std::vector<uint32_t> Shader(const std::string &path) {
  auto a = outshine::Render::ReadSpirv(path);
  if (!a) { Fail(a.error()); }
  return std::move(*a);
}

std::unique_ptr<Native::Buffer> Upload(Native::Device &device,
                                       Native::Commands &commands,
                                       const Bytes &data,
                                       Native::BufferUse usage) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = data.size(), .Use = usage | Native::BufferUse::CopyDestination}));
  Check(Cal::Upload(device, commands, *buffer, data));
  return buffer;
}

Native::Binding Storage(const Native::Buffer &b) {
  return {.Set = 0,
          .Slot = 0,
          .Value = Native::StorageBuffer{.Data = &b, .Range = {.Bytes = b.Description().Bytes}}};
}

struct alignas(16) Frame {
  std::array<float, 16> Clip;
  std::array<float, 4> Eye;
  std::array<float, 12> Palette;
  std::array<uint32_t, 4> Control;
};

static_assert(sizeof(Frame) == 144);
static_assert(alignof(Frame) == 16);
static_assert(offsetof(Frame, Eye) == 64);
static_assert(offsetof(Frame, Control) == 128);
static_assert(offsetof(Frame, Palette) == 80);

struct Pipelines {
  std::unique_ptr<Native::GraphicsPipeline> Raster;
  std::unique_ptr<Native::ComputePipeline> Compose;
};

Pipelines Create(Native::Device &device, const std::string &path, bool audit) {
  const auto vert = Shader(path + "/parent.vert.spv");
  const auto frag = Shader(path + (audit ? "/parent-audit.frag.spv" : "/parent.frag.spv"));
  const auto compute = Shader(path + "/compose.comp.spv");
  const std::array streams{Native::VertexStream{.Slot = 0, .Stride = 12}};
  const std::array attrs{
      Native::VertexAttribute{.Location = 0, .Stream = 0, .Format = Native::VertexFormat::Float3}};
  const std::array colours{
      Native::ColourTarget{.Texels = Native::Format::Rgba32Float, .Rgb = {}, .Alpha = {}},
      Native::ColourTarget{.Texels = Native::Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  Pipelines p;
  p.Raster =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vert},
                                             .Fragment = Native::ShaderCode{.Spirv = frag},
                                             .Streams = streams,
                                             .Attributes = attrs,
                                             .Colours = std::span(colours).first(audit ? 2 : 1),
                                             .Depth = Native::DepthTarget{},
                                             .Culling = Native::Cull::None,
                                             .Label = "parent-shared-GGX"}));
  p.Compose = Require(device.CreateComputePipeline(compute, {.Label = "parent-first-hit-compose"}));
  return p;
}

struct Case {
  std::string Name;
  uint32_t Width = 0;
  uint32_t Height = 0;
  bool Opaque = false;
  std::vector<std::array<uint32_t, 2>> Layers;
  Frame View{};
  std::unique_ptr<Native::Buffer> Positions, Records;
  uint64_t SourcePositionBytes = 0;
  uint64_t SourceIndexBytes = 0;
  std::unique_ptr<Native::Texture> Colour, Depth, Output, Audit;
  std::unique_ptr<Native::Sampler> Sampler;
  Json Samples = Json::array();
};

void Validate(const Bytes &positions,
              const Bytes &indices,
              const Bytes &records,
              std::span<const std::array<uint32_t, 2>> layers) {
  if (positions.size() % 12 != 0 || indices.size() % 12 != 0 ||
      records.size() != indices.size() / 12 * 176) {
    Fail("native vertex, triangle and face record shapes differ");
  }
  for (size_t at = 0; at < indices.size(); at += sizeof(uint32_t)) {
    uint32_t index = 0;
    std::memcpy(&index, indices.data() + at, sizeof(index));
    if (index >= positions.size() / 12) { Fail("native index exceeds uploaded vertices"); }
  }
  for (const auto *values : {&positions, &records}) {
    for (size_t at = 0; at < values->size(); at += sizeof(float)) {
      float value = 0;
      std::memcpy(&value, values->data() + at, sizeof(value));
      if (!std::isfinite(value)) { Fail("native surface input is not finite"); }
    }
  }
  uint64_t next = 0;
  for (const auto &layer : layers) {
    if (layer[0] != next || layer[1] == 0 || layer[1] % 3 != 0) {
      Fail("native first-hit layers do not partition whole ordered triangles");
    }
    next += layer[1];
  }
  if (next != indices.size() / sizeof(uint32_t)) { Fail("native layers omit index ranges"); }
}

Bytes RasterPositions(const Bytes &positions, const Bytes &indices) {
  static_assert(sizeof(float) == 4 && sizeof(uint32_t) == 4);
  constexpr size_t stride = 3 * sizeof(float);
  const auto count = indices.size() / sizeof(uint32_t);
  if (count > std::numeric_limits<size_t>::max() / stride) {
    Fail("expanded diagnostic vertex data exceeds addressable storage");
  }
  Bytes result(count * stride);
  for (size_t at = 0; at < count; ++at) {
    uint32_t index = 0;
    std::memcpy(&index, indices.data() + at * sizeof(index), sizeof(index));
    std::memcpy(result.data() + at * stride, positions.data() + size_t{index} * stride, stride);
  }
  return result;
}

Case Prepare(Native::Device &device,
             const std::string &path,
             std::array<uint32_t, 2> extent,
             const Json &row,
             bool audit) {
  Case c;
  c.Name = row["name"].get<std::string>();
  c.Width = extent[0];
  c.Height = extent[1];
  c.Opaque = row["opaque"];
  c.Layers = row["layers"].get<std::vector<std::array<uint32_t, 2>>>();
  if (c.Layers.empty() || c.Layers.size() > 16 || c.Width == 0 || c.Height == 0) {
    Fail("invalid bounded atlas");
  }
  const auto atlasHeight = uint64_t{c.Height} * c.Layers.size();
  if (c.Width > device.Limits().MaxTextureExtent ||
      atlasHeight > device.Limits().MaxTextureExtent) {
    Fail("native diagnostic atlas exceeds device limits");
  }
  auto view = Read(path + "/frame.bin");
  if (view.size() != 128) { Fail("uniform shape"); }
  std::memcpy(&c.View, view.data(), view.size());
  auto commands = Require(device.Begin());
  const auto base = path + "/" + c.Name;
  const auto positions = Read(base + "/positions.bin");
  const auto indices = Read(base + "/indices.bin");
  const auto records = Read(base + "/records.bin");
  Validate(positions, indices, records, c.Layers);
  c.SourcePositionBytes = positions.size();
  c.SourceIndexBytes = indices.size();
  const auto rasterPositions = RasterPositions(positions, indices);
  c.Positions = Upload(device, *commands, rasterPositions, Native::BufferUse::Vertex);
  c.Records = Upload(device, *commands, records, Native::BufferUse::Storage);
  Check(Cal::Finish(device, std::move(commands)));
  const auto height = c.Height * static_cast<uint32_t>(c.Layers.size());
  c.Colour = Require(device.CreateTexture(
      {.Width = c.Width,
       .Height = height,
       .Texels = Native::Format::Rgba32Float,
       .Use = Native::TextureUse::ColourAttachment | Native::TextureUse::Sampled}));
  c.Depth = Require(device.CreateTexture(
      {.Width = c.Width,
       .Height = height,
       .Texels = Native::Format::Depth32Float,
       .Use = Native::TextureUse::DepthAttachment | Native::TextureUse::Sampled}));
  c.Output = Require(
      device.CreateTexture({.Width = c.Width,
                            .Height = c.Height,
                            .Texels = Native::Format::Rgba32Float,
                            .Use = Native::TextureUse::Storage | Native::TextureUse::CopySource}));
  if (audit) {
    c.Audit = Require(device.CreateTexture(
        {.Width = c.Width,
         .Height = height,
         .Texels = Native::Format::Rgba32Float,
         .Use = Native::TextureUse::ColourAttachment | Native::TextureUse::CopySource}));
  }
  c.Sampler = Require(device.CreateSampler({.Min = Native::Filter::Nearest,
                                            .Mag = Native::Filter::Nearest,
                                            .Mip = Native::Filter::Nearest}));
  return c;
}

Native::Result<void> Encode(Native::Commands &cmd, const Pipelines &p, const Case &c) {
  const std::array colours{Native::ColourAttachment{.Image = {.Image = c.Colour.get()},
                                                    .Begin = Native::Load::Clear,
                                                    .End = Native::Store::Store},
                           Native::ColourAttachment{.Image = {.Image = c.Audit.get()},
                                                    .Begin = Native::Load::Clear,
                                                    .End = Native::Store::Store}};
  auto result =
      cmd.BeginRenderPass({.Colours = std::span(colours).first(c.Audit ? 2 : 1),
                           .Depth = Native::DepthAttachment{.Image = {.Image = c.Depth.get()},
                                                            .Begin = Native::Load::Clear,
                                                            .End = Native::Store::Store,
                                                            .Clear = 0}});
  if (!result) { return result; }
  const std::array vertices{
      Native::VertexBuffer{.Stream = 0,
                           .Data = c.Positions.get(),
                           .Range = {.Bytes = c.Positions->Description().Bytes}}};
  for (size_t layer = 0; layer < c.Layers.size(); ++layer) {
    Frame view = c.View;
    view.Control = {0, c.Opaque ? 1u : 0u, 0, 0};
    const std::array bindings{
        Storage(*c.Records),
        Native::Binding{.Set = 1,
                        .Slot = 0,
                        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
    const auto y = static_cast<uint32_t>(layer) * c.Height;
    result = cmd.Draw({.State = {.Pipeline = p.Raster.get(),
                                 .Bindings = bindings,
                                 .Vertices = vertices,
                                 .Area = {.Y = static_cast<float>(y),
                                          .Width = static_cast<float>(c.Width),
                                          .Height = static_cast<float>(c.Height)},
                                 .Clip = {.Y = y, .Width = c.Width, .Height = c.Height}},
                       .Range = {.First = c.Layers[layer][0], .Count = c.Layers[layer][1]}});
    if (!result) { return result; }
  }
  result = cmd.EndRenderPass();
  if (!result) { return result; }
  const std::array<uint32_t, 4> size{c.Width, c.Height, static_cast<uint32_t>(c.Layers.size()), 0};
  const std::array bindings{
      Native::Binding{.Set = 0,
                      .Slot = 0,
                      .Value = Native::SampledTexture{.Image = {.Image = c.Colour.get()},
                                                      .Sampling = c.Sampler.get()}},
      Native::Binding{.Set = 0,
                      .Slot = 1,
                      .Value = Native::SampledTexture{.Image = {.Image = c.Depth.get()},
                                                      .Sampling = c.Sampler.get()}},
      Native::Binding{
          .Set = 1, .Slot = 0, .Value = Native::StorageTexture{.Image = {.Image = c.Output.get()}}},
      Native::Binding{.Set = 2,
                      .Slot = 0,
                      .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(&size, 1))}}};
  return cmd.Dispatch(
      *p.Compose, bindings, {.X = (c.Width + 7u) / 8u, .Y = (c.Height + 7u) / 8u, .Z = 1});
}

Bytes Download(Native::Device &device, const Case &c, bool audit = false) {
  const auto height = c.Height * (audit ? static_cast<uint32_t>(c.Layers.size()) : 1u);
  const uint64_t row = (uint64_t{c.Width} * 16u + 255u) / 256u * 256u;
  auto buffer = Require(device.CreateBuffer({.Bytes = row * height,
                                             .Use = Native::BufferUse::CopyDestination,
                                             .Placement = Native::Memory::Readback}));
  auto commands = Require(device.Begin());
  Check(commands->Copy(audit ? *c.Audit : *c.Output,
                       *buffer,
                       {.RowBytes = row, .Region = {.Width = c.Width, .Height = height}}));
  Check(Cal::Finish(device, std::move(commands)));
  const auto mapped = Require(device.MapRead(*buffer, {.Bytes = row * height}));
  Bytes bytes(uint64_t{c.Width} * height * 16u);
  for (uint32_t y = 0; y < height; ++y) {
    std::memcpy(bytes.data() + uint64_t{y} * c.Width * 16u,
                mapped.data() + uint64_t{y} * row,
                uint64_t{c.Width} * 16u);
  }
  Check(device.Unmap(*buffer));
  return bytes;
}

}

int main(int argc, char *const *argv) {
  if (argc != 4 && argc != 5) { Fail("parent-probe <input> <shaders> <output> [--audit]"); }
  const bool audit = argc == 5;
  if (audit && std::string_view(argv[4]) != "--audit") { Fail("unknown native probe option"); }
  std::ifstream in(std::string(argv[1]) + "/input.json");
  Json meta = Json::parse(in, nullptr, false);
  if (meta.is_discarded()) { Fail("metadata"); }
  auto device = Require(
      Native::CreateDevice({.Validation = audit, .GpuTiming = Native::GpuTimingMode::Submission}));
  auto pipelines = Create(*device, argv[2], audit);
  std::vector<Case> cases;
  for (const auto &row : meta["cases"]) {
    cases.push_back(Prepare(*device, argv[1], {meta["width"], meta["height"]}, row, audit));
  }
  for (uint32_t round = 0; round < 24; ++round) {
    for (size_t at = 0; at < cases.size(); ++at) {
      const auto slot = round % 2 == 0 ? at : cases.size() - 1 - at;
      auto &c = cases[slot];
      const auto sample = Require(
          Cal::Measure(*device, [&](Native::Commands &cmd) { return Encode(cmd, pipelines, c); }));
      if (!sample.GpuNs || !sample.EncodeNs) { Fail("missing GPU or encode timing"); }
      c.Samples.push_back(
          {{"gpu_ns", *sample.GpuNs}, {"encode_ns", *sample.EncodeNs}, {"wall_ns", sample.WallNs}});
    }
  }
  Json report = {
      {"device", device->Limits().Name},
      {"validation", audit},
      {"width", meta["width"]},
      {"height", meta["height"]},
      {"raster_representation", "expanded-triangle-corners"},
      {"scope",
       "Bounded component crop at unchanged presentation pixel spacing, paid group first-hit "
       "atlas, full/2-lobe GGX, depth sort and C/T; no full scene or world admission."},
      {"cases", Json::array()}};
  for (const auto &c : cases) {
    auto bytes = Download(*device, c);
    Write(std::string(argv[3]) + "/" + c.Name + ".rgba32f", bytes);
    if (c.Audit) {
      Write(std::string(argv[3]) + "/" + c.Name + ".audit32f", Download(*device, c, true));
    }
    report["cases"].push_back(
        {{"name", c.Name},
         {"layers", c.Layers.size()},
         {"samples", c.Samples},
         {"source_position_bytes", c.SourcePositionBytes},
         {"source_index_bytes", c.SourceIndexBytes},
         {"raster_position_bytes", c.Positions->Description().Bytes},
         {"buffer_bytes", c.Positions->Description().Bytes + c.Records->Description().Bytes}});
  }
  std::ofstream out(std::string(argv[3]) + "/result.json");
  out << report.dump(2);
  if (!out) { Fail("report write"); }
  return 0;
}
