#include "MetalGraphics.h"
#include "MetalInternal.h"
#include <algorithm>
#include <array>
#include <tuple>

namespace outshine::Render::Native::Metal {

std::optional<AttributeFormat> Attribute(VertexFormat format) {
  switch (format) {
    case VertexFormat::Float2: return AttributeFormat{.Native = MTLVertexFormatFloat2, .Bytes = 8};
    case VertexFormat::Float3: return AttributeFormat{.Native = MTLVertexFormatFloat3, .Bytes = 12};
    case VertexFormat::Float4: return AttributeFormat{.Native = MTLVertexFormatFloat4, .Bytes = 16};
    case VertexFormat::Uint32: return AttributeFormat{.Native = MTLVertexFormatUInt, .Bytes = 4};
  }
  return std::nullopt;
}

namespace {

Result<MTLVertexDescriptor *> Vertices(const GraphicsDescription &description) {
  MTLVertexDescriptor *const layout = [MTLVertexDescriptor vertexDescriptor];
  std::array<bool, kVertexBufferSlots> streams{};
  std::array<bool, kVertexAttributeSlots> attributes{};
  for (const auto &stream : description.Streams) {
    if (stream.Slot >= streams.size()) {
      return Failure(ErrorKind::Unsupported, "Metal vertex stream slot exceeds device layout");
    }
    if (streams[stream.Slot] || stream.Stride == 0 ||
        (stream.Rate != VertexRate::Vertex && stream.Rate != VertexRate::Instance)) {
      return Failure(ErrorKind::Invalid, "invalid or repeated Metal vertex stream");
    }
    streams[stream.Slot] = true;
    MTLVertexBufferLayoutDescriptor *const target =
        layout.layouts[kFirstVertexBuffer + stream.Slot];
    target.stride = stream.Stride;
    target.stepFunction = stream.Rate == VertexRate::Vertex ? MTLVertexStepFunctionPerVertex
                                                            : MTLVertexStepFunctionPerInstance;
    target.stepRate = 1;
  }
  for (const auto &attribute : description.Attributes) {
    const auto format = Attribute(attribute.Format);
    const auto stream =
        std::ranges::find(description.Streams, attribute.Stream, &VertexStream::Slot);
    if (!format || attribute.Location >= attributes.size() || attributes[attribute.Location] ||
        stream == description.Streams.end() || attribute.Offset > stream->Stride ||
        format->Bytes > stream->Stride - attribute.Offset) {
      return Failure(ErrorKind::Invalid,
                     "invalid Metal vertex attribute location, stream or extent");
    }
    attributes[attribute.Location] = true;
    MTLVertexAttributeDescriptor *const target = layout.attributes[attribute.Location];
    target.bufferIndex = kFirstVertexBuffer + attribute.Stream;
    target.offset = attribute.Offset;
    target.format = format->Native;
  }
  return layout;
}

std::optional<MTLBlendFactor> Factor(BlendFactor factor) {
  switch (factor) {
    case BlendFactor::Zero: return MTLBlendFactorZero;
    case BlendFactor::One: return MTLBlendFactorOne;
    case BlendFactor::SourceAlpha: return MTLBlendFactorSourceAlpha;
    case BlendFactor::OneMinusSourceAlpha: return MTLBlendFactorOneMinusSourceAlpha;
  }
  return std::nullopt;
}

Result<void> Targets(const MetalDevice &device,
                     const GraphicsDescription &description,
                     MTLRenderPipelineDescriptor *pipeline) {
  if (description.Colours.size() > device.Limits().MaxColourAttachments ||
      (description.Colours.empty() && !description.Depth) ||
      (!description.Colours.empty() && !description.Fragment)) {
    return Failure(ErrorKind::Invalid,
                   "invalid Metal render target count or missing fragment shader");
  }
  for (size_t index = 0; index < description.Colours.size(); ++index) {
    const auto &colour = description.Colours[index];
    const auto sourceRgb = Factor(colour.Rgb.Source);
    const auto destinationRgb = Factor(colour.Rgb.Destination);
    const auto sourceAlpha = Factor(colour.Alpha.Source);
    const auto destinationAlpha = Factor(colour.Alpha.Destination);
    if (!device.Supports(colour.Texels, TextureUse::ColourAttachment, false) ||
        (static_cast<uint32_t>(colour.Write) & ~15u) != 0 || !sourceRgb || !destinationRgb ||
        !sourceAlpha || !destinationAlpha) {
      return Failure(ErrorKind::Invalid,
                     "invalid Metal colour format, write mask or blend function");
    }
    MTLRenderPipelineColorAttachmentDescriptor *const target = pipeline.colorAttachments[index];
    target.pixelFormat = Texels(colour.Texels)->Native;
    target.blendingEnabled = colour.Blend;
    target.sourceRGBBlendFactor = *sourceRgb;
    target.destinationRGBBlendFactor = *destinationRgb;
    target.sourceAlphaBlendFactor = *sourceAlpha;
    target.destinationAlphaBlendFactor = *destinationAlpha;
    target.rgbBlendOperation = MTLBlendOperationAdd;
    target.alphaBlendOperation = MTLBlendOperationAdd;
    MTLColorWriteMask write = MTLColorWriteMaskNone;
    if (Has(colour.Write, ColourChannel::Red)) { write |= MTLColorWriteMaskRed; }
    if (Has(colour.Write, ColourChannel::Green)) { write |= MTLColorWriteMaskGreen; }
    if (Has(colour.Write, ColourChannel::Blue)) { write |= MTLColorWriteMaskBlue; }
    if (Has(colour.Write, ColourChannel::Alpha)) { write |= MTLColorWriteMaskAlpha; }
    target.writeMask = write;
  }
  if (description.Depth) {
    if (description.Depth->Texels != Format::Depth32Float ||
        (description.Depth->Compare != DepthCompare::Greater &&
         description.Depth->Compare != DepthCompare::GreaterEqual)) {
      return Failure(ErrorKind::Invalid, "invalid Metal depth format or comparison");
    }
    pipeline.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
  }
  return {};
}

Result<void> StageBindings(GraphicsLayout &layout, const Shader &shader, ShaderStage stage) {
  for (const auto &binding : shader.Resources) {
    const auto previous = std::ranges::find_if(layout.Bindings, [&](const GraphicsBinding &one) {
      return one.Resource.Set == binding.Source.Set && one.Resource.Slot == binding.Source.Slot;
    });
    if (previous == layout.Bindings.end()) {
      layout.Bindings.push_back({.Resource = binding.Source, .Stages = stage});
    } else {
      if (Has(previous->Stages, stage) || previous->Resource.Kind != binding.Source.Kind) {
        return Failure(ErrorKind::Invalid, "incompatible or repeated graphics shader binding");
      }
      previous->Stages = previous->Stages | stage;
      previous->Resource.MinimumBytes =
          std::max(previous->Resource.MinimumBytes, binding.Source.MinimumBytes);
      if (previous->Resource.Usage != binding.Source.Usage) {
        previous->Resource.Usage = Access::ReadWrite;
      }
    }
  }
  return {};
}

Result<void> CompatibleImages(const Shader &vertex, const Shader &fragment) {
  for (const auto &one : fragment.Resources) {
    const auto other = std::ranges::find_if(vertex.Resources, [&](const ResourceBinding &binding) {
      return binding.Source.Set == one.Source.Set && binding.Source.Slot == one.Source.Slot;
    });
    if (other != vertex.Resources.end() &&
        (other->ArrayTexture != one.ArrayTexture ||
         (other->TexelFormat && one.TexelFormat && other->TexelFormat != one.TexelFormat))) {
      return Failure(ErrorKind::Invalid, "graphics stages disagree on a shared image binding");
    }
  }
  return {};
}

Result<void> RasterState(GraphicsState &state, const GraphicsDescription &description) {
  switch (description.Culling) {
    case Cull::None: state.Culling = MTLCullModeNone; break;
    case Cull::Front: state.Culling = MTLCullModeFront; break;
    case Cull::Back: state.Culling = MTLCullModeBack; break;
    default: return Failure(ErrorKind::Invalid, "invalid Metal cull mode");
  }
  switch (description.Front) {
    case Winding::Clockwise: state.Front = MTLWindingClockwise; break;
    case Winding::CounterClockwise: state.Front = MTLWindingCounterClockwise; break;
    default: return Failure(ErrorKind::Invalid, "invalid Metal front winding");
  }
  state.Streams.assign(description.Streams.begin(), description.Streams.end());
  state.Attributes.assign(description.Attributes.begin(), description.Attributes.end());
  state.Colours.assign(description.Colours.begin(), description.Colours.end());
  state.DepthSettings = description.Depth;
  return {};
}

}

Result<std::unique_ptr<GraphicsPipeline>>
MetalDevice::CreateGraphicsPipeline(const GraphicsDescription &description) {
  if (!OnOwner()) {
    return Failure(ErrorKind::Invalid, "Metal operation belongs to another thread");
  }
  @autoreleasepool {
    auto state = std::make_shared<GraphicsState>();
    state->Device = Identity_;
    if (auto valid = RasterState(*state, description); !valid) {
      return std::unexpected(valid.error());
    }
    MTLRenderPipelineDescriptor *const pipeline = [MTLRenderPipelineDescriptor new];
    pipeline.label = Text(description.Label);
    if (auto valid = Targets(*this, description, pipeline); !valid) {
      return std::unexpected(valid.error());
    }
    const auto vertices = Vertices(description);
    if (!vertices) { return std::unexpected(vertices.error()); }
    pipeline.vertexDescriptor = *vertices;
    auto vertex = Translate(description.Vertex.Spirv, description.Vertex.Entry);
    if (!vertex) { return std::unexpected(vertex.error()); }
    state->Vertex = std::move(*vertex);
    const auto function = Function(Native_, state->Vertex, MTLFunctionTypeVertex);
    if (!function) { return std::unexpected(function.error()); }
    pipeline.vertexFunction = *function;
    GraphicsLayout layout;
    if (auto valid = StageBindings(layout, state->Vertex, ShaderStage::Vertex); !valid) {
      return std::unexpected(valid.error());
    }
    if (description.Fragment) {
      auto fragment = Translate(description.Fragment->Spirv, description.Fragment->Entry);
      if (!fragment) { return std::unexpected(fragment.error()); }
      state->Fragment = std::move(*fragment);
      if (auto valid = CompatibleImages(state->Vertex, *state->Fragment); !valid) {
        return std::unexpected(valid.error());
      }
      const auto fragmentFunction = Function(Native_, *state->Fragment, MTLFunctionTypeFragment);
      if (!fragmentFunction) { return std::unexpected(fragmentFunction.error()); }
      pipeline.fragmentFunction = *fragmentFunction;
      if (auto valid = StageBindings(layout, *state->Fragment, ShaderStage::Fragment); !valid) {
        return std::unexpected(valid.error());
      }
    }
    NSError *error = nil;
    state->Native = [Native_ newRenderPipelineStateWithDescriptor:pipeline error:&error];
    if (state->Native == nil) {
      return Failure(ErrorKind::Invalid, error.localizedDescription.UTF8String);
    }
    MTLDepthStencilDescriptor *const depth = [MTLDepthStencilDescriptor new];
    depth.depthCompareFunction = MTLCompareFunctionAlways;
    if (description.Depth && description.Depth->Test) {
      depth.depthCompareFunction = description.Depth->Compare == DepthCompare::Greater
                                       ? MTLCompareFunctionGreater
                                       : MTLCompareFunctionGreaterEqual;
    }
    depth.depthWriteEnabled = description.Depth && description.Depth->Write;
    state->Depth = [Native_ newDepthStencilStateWithDescriptor:depth];
    if (state->Depth == nil) {
      return Failure(ErrorKind::Driver, "Metal depth state creation failed");
    }
    std::string{}.swap(state->Vertex.Source);
    if (state->Fragment) { std::string{}.swap(state->Fragment->Source); }
    std::ranges::sort(layout.Bindings, [](const GraphicsBinding &a, const GraphicsBinding &b) {
      return std::tie(a.Resource.Set, a.Resource.Slot) < std::tie(b.Resource.Set, b.Resource.Slot);
    });
    return std::make_unique<MetalGraphicsPipeline>(std::move(layout), std::move(state));
  }
}

}
