#include "MetalGraphics.h"
#include "MetalInternal.h"
#include <algorithm>
#include <cmath>

namespace outshine::Render::Native::Metal {
namespace {

struct VertexArgument {
  std::shared_ptr<BufferState> State;
  uint64_t Offset;
  uint32_t Slot;
};

bool TargetsMatch(const RenderState &pass, const GraphicsState &pipeline) {
  if (pass.Colours.size() != pipeline.Colours.size() ||
      pass.Depth.has_value() != pipeline.DepthSettings.has_value()) {
    return false;
  }
  for (size_t slot = 0; slot < pass.Colours.size(); ++slot) {
    if (pass.Colours[slot].Texels != pipeline.Colours[slot].Texels) { return false; }
  }
  return !pass.Depth || pass.Depth->Texels == pipeline.DepthSettings->Texels;
}

bool AreaFits(const DrawState &state, const RenderState &pass) {
  const auto &area = state.Area;
  const auto &clip = state.Clip;
  const auto width = static_cast<float>(pass.Width);
  const auto height = static_cast<float>(pass.Height);
  return std::isfinite(area.X) && std::isfinite(area.Y) && std::isfinite(area.Width) &&
         std::isfinite(area.Height) && std::isfinite(area.MinDepth) &&
         std::isfinite(area.MaxDepth) && area.X >= 0 && area.Y >= 0 && area.Width > 0 &&
         area.Height > 0 && area.MinDepth >= 0 && area.MaxDepth <= 1 &&
         area.MinDepth <= area.MaxDepth && area.X <= width && area.Y <= height &&
         area.Width <= width - area.X && area.Height <= height - area.Y && clip.Width > 0 &&
         clip.Height > 0 && clip.X <= pass.Width && clip.Y <= pass.Height &&
         clip.Width <= pass.Width - clip.X && clip.Height <= pass.Height - clip.Y;
}

bool FetchFits(ByteRange buffer,
               uint32_t extent,
               const VertexStream &stream,
               VertexRange range,
               DrawKind kind) {
  if ((kind != DrawKind::Indirect && (range.Count == 0 || range.Instances == 0)) || extent == 0) {
    return true;
  }
  if (extent > buffer.Bytes) { return false; }
  if (kind == DrawKind::Indirect ||
      (kind == DrawKind::Indexed && stream.Rate == VertexRate::Vertex)) {
    return true;
  }
  const uint64_t count = stream.Rate == VertexRate::Vertex ? range.Count : range.Instances;
  const uint64_t first = stream.Rate == VertexRate::Vertex ? range.First : range.FirstInstance;
  return first + count - 1 <= (buffer.Bytes - extent) / stream.Stride;
}

Result<std::vector<VertexArgument>> VertexArguments(const GraphicsState &pipeline,
                                                    const DrawState &state,
                                                    VertexRange range,
                                                    DrawKind kind,
                                                    uint64_t device) {
  if (state.Vertices.size() != pipeline.Streams.size()) {
    return Failure(ErrorKind::Invalid, "Metal vertex buffers differ from the pipeline layout");
  }
  std::vector<VertexArgument> result;
  result.reserve(pipeline.Streams.size());
  for (const auto &stream : pipeline.Streams) {
    const auto matches = [&](const VertexBuffer &buffer) { return buffer.Stream == stream.Slot; };
    if (std::ranges::count_if(state.Vertices, matches) != 1) {
      return Failure(ErrorKind::Invalid, "missing or repeated Metal vertex stream binding");
    }
    const auto &binding = *std::ranges::find_if(state.Vertices, matches);
    const auto *const buffer = dynamic_cast<const MetalBuffer *>(binding.Data);
    if (buffer == nullptr || buffer->State->Device != device ||
        !Has(buffer->Description().Use, BufferUse::Vertex) ||
        !Fits(binding.Range, buffer->Description().Bytes) || binding.Range.Offset % 4 != 0) {
      return Failure(ErrorKind::Invalid, "invalid Metal vertex buffer, usage or ownership");
    }
    uint32_t extent = 0;
    for (const auto &attribute : pipeline.Attributes) {
      if (attribute.Stream == stream.Slot) {
        extent = std::max(extent, attribute.Offset + Attribute(attribute.Format)->Bytes);
      }
    }
    if (!FetchFits(binding.Range, extent, stream, range, kind)) {
      return Failure(ErrorKind::Invalid, "Metal draw exceeds the bound vertex/instance span");
    }
    result.push_back({.State = buffer->State, .Offset = binding.Range.Offset, .Slot = stream.Slot});
  }
  return result;
}

bool AttachmentAlias(const RenderState &pass, const ShaderArgument &argument) {
  if (argument.Texture == nil) { return false; }
  const auto &view = *argument.Image;
  const auto overlaps = [&](const AttachmentState &target) {
    return target.Native == argument.Resource && target.Mip >= view.FirstMip &&
           target.Mip - view.FirstMip < view.Mips && target.Layer >= view.FirstLayer &&
           target.Layer - view.FirstLayer < view.Layers;
  };
  return std::ranges::any_of(pass.Colours, overlaps) || (pass.Depth && overlaps(*pass.Depth));
}

void ApplyRasterState(id<MTLRenderCommandEncoder> encoder,
                      const GraphicsState &graphics,
                      const DrawState &state,
                      std::span<const VertexArgument> vertices) {
  [encoder setRenderPipelineState:graphics.Native];
  [encoder setDepthStencilState:graphics.Depth];
  [encoder setCullMode:graphics.Culling];
  [encoder setFrontFacingWinding:graphics.Front];
  [encoder setDepthClipMode:graphics.DepthClip];
  const auto area = state.Area;
  [encoder setViewport:MTLViewport{.originX = area.X,
                                   .originY = area.Y,
                                   .width = area.Width,
                                   .height = area.Height,
                                   .znear = area.MinDepth,
                                   .zfar = area.MaxDepth}];
  const auto clip = state.Clip;
  [encoder
      setScissorRect:MTLScissorRect{
                         .x = clip.X, .y = clip.Y, .width = clip.Width, .height = clip.Height}];
  for (const auto &binding : vertices) {
    [encoder setVertexBuffer:binding.State->Native
                      offset:binding.Offset
                     atIndex:kFirstVertexBuffer + binding.Slot];
  }
}

}

Result<void> MetalCommands::PrepareDraw(const DrawState &state,
                                        VertexRange range,
                                        DrawKind kind,
                                        std::span<const std::shared_ptr<BufferState>> reads) {
  if (!OnOwner() || !Render) {
    return Failure(ErrorKind::Invalid, "Metal draw requires its owner and an open render pass");
  }
  @autoreleasepool {
    const auto *const pipeline = dynamic_cast<const MetalGraphicsPipeline *>(state.Pipeline);
    if (pipeline == nullptr || pipeline->State->Device != Device ||
        !TargetsMatch(*Render, *pipeline->State) || !AreaFits(state, *Render) ||
        state.Bindings.size() != pipeline->Layout().Bindings.size()) {
      return Failure(ErrorKind::Invalid, "invalid Metal draw pipeline, targets, area or bindings");
    }
    const auto &graphics = *pipeline->State;
    const auto vertices = VertexArguments(graphics, state, range, kind, Device);
    if (!vertices) { return std::unexpected(vertices.error()); }
    auto vertex = Arguments(graphics.Vertex, state.Bindings, Device);
    if (!vertex) { return std::unexpected(vertex.error()); }
    auto fragment = graphics.Fragment
                        ? Arguments(*graphics.Fragment, state.Bindings, Device)
                        : Result<std::vector<ShaderArgument>>(std::vector<ShaderArgument>{});
    if (!fragment) { return std::unexpected(fragment.error()); }
    const auto aliases = CheckImageAliases(*vertex, *fragment);
    if (!aliases) { return aliases; }
    std::vector<id<MTLResource>> hazards;
    const auto alias = [&](const ShaderArgument &argument) {
      return AttachmentAlias(*Render, argument);
    };
    if (std::ranges::any_of(*vertex, alias) || std::ranges::any_of(*fragment, alias)) {
      return Failure(ErrorKind::Invalid, "Metal draw samples or stores its active attachment");
    }
    for (const auto *const arguments : {&*vertex, &*fragment}) {
      const auto prepared = PrepareResources(*arguments, hazards);
      if (!prepared) { return prepared; }
    }
    const auto readBuffers = PrepareReads(reads, hazards);
    if (!readBuffers) { return readBuffers; }
    for (const auto &binding : *vertices) {
      const auto held = Hold(binding.State);
      if (!held) { return held; }
      TrackHazard(binding.State->Native, Access::Read, hazards);
    }
    const id<MTLRenderCommandEncoder> encoder = Render->Encoder;
    if (!hazards.empty()) {
      const MTLRenderStages stages = MTLRenderStageVertex | MTLRenderStageFragment;
      [encoder memoryBarrierWithResources:hazards.data()
                                    count:hazards.size()
                              afterStages:stages
                             beforeStages:stages];
      std::erase_if(Accesses, [&](const TrackedResource &one) {
        return std::ranges::find(hazards, one.Resource) != hazards.end();
      });
    }
    auto bound = Bind(encoder, ShaderStage::Vertex, graphics.Vertex, *vertex);
    if (!bound) { return bound; }
    if (graphics.Fragment) {
      bound = Bind(encoder, ShaderStage::Fragment, *graphics.Fragment, *fragment);
      if (!bound) { return bound; }
    }
    ApplyRasterState(encoder, graphics, state, *vertices);
    UpdateAccesses(*vertex);
    UpdateAccesses(*fragment);
    for (const auto &binding : *vertices) { UpdateAccess(binding.State->Native, Access::Read); }
    for (const auto &buffer : reads) { UpdateAccess(buffer->Native, Access::Read); }
    return {};
  }
}

Result<void> MetalCommands::Draw(const DrawDescription &description) {
  const auto prepared = PrepareDraw(description.State, description.Range, DrawKind::Direct);
  if (!prepared) { return prepared; }
  const auto range = description.Range;
  if (range.Count == 0 || range.Instances == 0) { return {}; }
  [Render->Encoder drawPrimitives:MTLPrimitiveTypeTriangle
                      vertexStart:range.First
                      vertexCount:range.Count
                    instanceCount:range.Instances
                     baseInstance:range.FirstInstance];
  return {};
}

}
