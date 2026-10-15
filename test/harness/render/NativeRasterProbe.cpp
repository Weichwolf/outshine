#include "NativeGpuCheck.h"
#include <array>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

void Rejected(Device &device, GraphicsDescription description, const char *reason) {
  const auto pipeline = device.CreateGraphicsPipeline(description);
  CHECK(!pipeline && pipeline.error().Kind == ErrorKind::Invalid, reason);
}

void Pipelines(Device &device, const char *vertexPath, const char *fragmentPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  std::array<ColourTarget, 5> colours{};
  for (auto &colour : colours) { colour.Texels = Format::Rgba32Float; }
  const GraphicsDescription description{.Vertex = {.Spirv = vertex},
                                        .Fragment = ShaderCode{.Spirv = fragment},
                                        .Colours = colours,
                                        .Depth = DepthTarget{},
                                        .Culling = Cull::None};
  const auto pipeline = Require(device.CreateGraphicsPipeline(description));
  CHECK(pipeline->Layout().Bindings.empty(), "the MRT shader has no invented resource bindings");
  auto depthOnly = description;
  depthOnly.Fragment.reset();
  depthOnly.Colours = {};
  CHECK(device.CreateGraphicsPipeline(depthOnly),
        "depth-only pipelines need no fragment or colour targets");
  auto bad = description;
  bad.Vertex = {};
  Rejected(device, bad, "a graphics pipeline requires readable vertex SPIR-V");
  bad = description;
  bad.Vertex.Entry = "missing";
  Rejected(device, bad, "a graphics pipeline requires the declared entry point");
  bad = description;
  bad.Vertex.Spirv = fragment;
  Rejected(device, bad, "fragment SPIR-V cannot serve as a vertex shader");
  std::array<ColourTarget, 9> excess{};
  bad = description;
  bad.Colours = excess;
  Rejected(device, bad, "attachment count is checked before indexing native descriptors");
  const std::array depthColour{ColourTarget{.Texels = Format::Depth32Float}};
  bad = description;
  bad.Colours = depthColour;
  Rejected(device, bad, "depth texels cannot become a colour target");
  bad = description;
  bad.Depth->Texels = Format::Rgba8Unorm;
  Rejected(device, bad, "colour texels cannot become a depth target");
  const std::array repeated{VertexStream{.Slot = 0, .Stride = 16},
                            VertexStream{.Slot = 0, .Stride = 16}};
  bad = description;
  bad.Streams = repeated;
  Rejected(device, bad, "repeated vertex streams cannot alias native buffer descriptors");
  const std::array streams{VertexStream{.Slot = 0, .Stride = 8}};
  const std::array outside{
      VertexAttribute{.Location = 0, .Stream = 0, .Offset = 4, .Format = VertexFormat::Float2}};
  bad = description;
  bad.Streams = streams;
  bad.Attributes = outside;
  Rejected(device, bad, "vertex attributes must fit wholly within their stream stride");
}

void PresentPipeline(Device &device, const char *vertexPath, const char *fragmentPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array colour{ColourTarget{.Texels = Format::Rgba8Unorm}};
  const auto pipeline =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Colours = colour,
                                             .Culling = Cull::None}));
  const auto &bindings = pipeline->Layout().Bindings;
  CHECK(bindings.size() == 1 && bindings[0].Resource.Set == 2 && bindings[0].Resource.Slot == 0 &&
            bindings[0].Resource.Kind == BindingKind::SampledTexture &&
            static_cast<uint32_t>(bindings[0].Stages) ==
                static_cast<uint32_t>(ShaderStage::Fragment),
        "the production present shader preserves its fragment-only set/binding texture layout");
}

}

int main(int argc, char *const *argv) {
  if (argc != 4) { return 1; }
  auto device = Require(CreateDevice());
  Pipelines(*device, argv[1], argv[2]);
  PresentPipeline(*device, argv[1], argv[3]);
  return Report();
}
