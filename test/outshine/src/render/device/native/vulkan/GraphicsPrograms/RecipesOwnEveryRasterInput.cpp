#include "src/render/device/native/vulkan/GraphicsRecipe.h"
#include "Check.h"
#include <array>
#include <cstdint>
#include <map>

namespace {
using namespace outshine::Render::Native;
using namespace outshine::Render::Native::Vulkan;

struct Input {
  std::array<uint32_t, 5> Vertex{1, 2, 3, 4, 5};
  std::array<uint32_t, 5> Fragment{6, 7, 8, 9, 10};
  std::array<VertexStream, 1> Streams{{{.Stride = 8}}};
  std::array<VertexAttribute, 1> Attributes{{{.Format = VertexFormat::Float2}}};
  std::array<ColourTarget, 1> Colours{};

  [[nodiscard]] GraphicsDescription Describe() const {
    return {.Vertex = {.Spirv = Vertex},
            .Fragment = ShaderCode{.Spirv = Fragment},
            .Streams = Streams,
            .Attributes = Attributes,
            .Colours = Colours,
            .Depth = DepthTarget{},
            .Label = {}};
  }
};

void IndependentFields() {
  const Input input;
  const auto initial = GraphicsRecipe::From(input.Describe());
  const auto check = [&](auto change) {
    Input candidate;
    auto description = candidate.Describe();
    change(candidate, description);
    CHECK(GraphicsRecipe::From(description) != initial,
          "every shader, stream, target and raster input participates in exact identity");
  };
  check([](auto &v, auto &) { ++v.Vertex[0]; });
  check([](auto &, auto &d) { d.Vertex.Entry = "other"; });
  check([](auto &v, auto &) { ++v.Fragment[0]; });
  check([](auto &, auto &d) { d.Fragment->Entry = "other"; });
  check([](auto &, auto &d) { d.Fragment.reset(); });
  check([](auto &, auto &d) { d.Streams = {}; });
  check([](auto &v, auto &) { ++v.Streams[0].Slot; });
  check([](auto &v, auto &) { ++v.Streams[0].Stride; });
  check([](auto &v, auto &) { v.Streams[0].Rate = VertexRate::Instance; });
  check([](auto &, auto &d) { d.Attributes = {}; });
  check([](auto &v, auto &) { ++v.Attributes[0].Location; });
  check([](auto &v, auto &) { ++v.Attributes[0].Stream; });
  check([](auto &v, auto &) { ++v.Attributes[0].Offset; });
  check([](auto &v, auto &) { v.Attributes[0].Format = VertexFormat::Float3; });
  check([](auto &, auto &d) { d.Colours = {}; });
  check([](auto &v, auto &) { v.Colours[0].Texels = Format::Rgba16Float; });
  check([](auto &v, auto &) { v.Colours[0].Write = ColourChannel::Red; });
  check([](auto &v, auto &) { v.Colours[0].Blend = true; });
  check([](auto &v, auto &) { v.Colours[0].Rgb.Source = BlendFactor::SourceAlpha; });
  check([](auto &v, auto &) { v.Colours[0].Rgb.Destination = BlendFactor::One; });
  check([](auto &v, auto &) { v.Colours[0].Alpha.Source = BlendFactor::SourceAlpha; });
  check([](auto &v, auto &) { v.Colours[0].Alpha.Destination = BlendFactor::One; });
  check([](auto &, auto &d) { d.Depth.reset(); });
  check([](auto &, auto &d) { d.Depth->Texels = Format::Rgba8Unorm; });
  check([](auto &, auto &d) { d.Depth->Compare = DepthCompare::GreaterEqual; });
  check([](auto &, auto &d) { d.Depth->Test = false; });
  check([](auto &, auto &d) { d.Depth->Write = false; });
  check([](auto &, auto &d) { d.Culling = Cull::None; });
  check([](auto &, auto &d) { d.Front = Winding::Clockwise; });
  check([](auto &, auto &d) { d.ClampDepth = true; });
}

void BorrowingAndCollisions() {
  Input input;
  auto description = input.Describe();
  const auto initial = GraphicsRecipe::From(description);
  description.Label = "diagnostic label only";
  CHECK(GraphicsRecipe::From(description) == initial,
        "the unused Vulkan debug label does not change program semantics");
  ++input.Vertex[0];
  auto changed = GraphicsRecipe::From(description);
  CHECK(changed != initial && initial == GraphicsRecipe::From(Input{}.Describe()),
        "recipe bytes own their snapshot independently of caller storage");
  changed.Digest = initial.Digest;
  std::map<GraphicsRecipe, unsigned> recipes;
  recipes.emplace(initial, 1);
  recipes.emplace(changed, 2);
  CHECK(recipes.size() == 2 && recipes.at(initial) == 1 && recipes.at(changed) == 2,
        "an explicit hash collision cannot alias different full recipe bytes");
}
}

int main() {
  IndependentFields();
  BorrowingAndCollisions();
  outshine::Test::Covers("full owned graphics recipe identity, every descriptor field, nonsemantic "
                         "labels and forced digest collisions; no GPU execution claimed");
  return outshine::Test::Report();
}
