#include "SubjectDraw.h"
#include "SurfaceBindings.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Render {
namespace {
Native::Binding Storage(uint32_t set, uint32_t slot, const Native::Buffer *buffer) {
  return {
      .Set = set,
      .Slot = slot,
      .Value = Native::StorageBuffer{
          .Data = buffer, .Range = {.Bytes = buffer != nullptr ? buffer->Description().Bytes : 0}}};
}

Native::SampledTexture Image(const SubjectResidency::BoundImage &image) {
  return {.Image = {.Image = image.Image.get()}, .Sampling = image.Sample.get()};
}

Native::VertexBuffer Vertex(uint32_t slot, const Native::Buffer *buffer) {
  return {.Stream = slot,
          .Data = buffer,
          .Range = {.Bytes = buffer != nullptr ? buffer->Description().Bytes : 0}};
}
}

void SubjectDraw::BindPlacementStorage(DrawResources &into, VertexLayout layout) const {
  using S = SubjectResidency::Stream;
  into.Bindings[into.BindingCount++] = Storage(0, 0, Bound().Buffer(S::Placements).get());
  uint32_t slot = 1;
  if (CarriesColour(layout)) {
    into.Bindings[into.BindingCount++] = Storage(0, slot++, Bound().Buffer(S::Colour).get());
  }
  if (CarriesTangent(layout)) {
    into.Bindings[into.BindingCount++] = Storage(0, slot, Bound().Buffer(S::Tangent).get());
  }
}

void SubjectDraw::BindVertexStreams(DrawResources &into,
                                    VertexLayout layout,
                                    VertexMotion motion) const {
  using S = SubjectResidency::Stream;
  const auto add = [&](S stream) {
    const auto slot = into.VertexCount++;
    into.Vertices[slot] = Vertex(slot, Bound().Buffer(stream).get());
  };
  add(S::Vertex);
  if (CarriesUv(layout)) { add(S::Uv); }
  if (CarriesUv1(layout)) { add(S::Uv1); }
  add(CarriesNormal(layout) ? S::Normal : S::Emitted);
  if (Binding().WritesVelocity) {
    add(motion == VertexMotion::Rigid || !Bound().Shape().HasPrevious ? S::Vertex : S::Previous);
  }
}

void SubjectDraw::BindSlot(DrawResources &into,
                           size_t slot,
                           VertexLayout layout,
                           const FrameUniforms &uniforms) const {
  const auto &surface = Slots[slot];
  auto images =
      std::array<Native::SampledTexture, kSubjectImages>{Image(surface.Colour),
                                                         Image(surface.Normal),
                                                         Image(surface.MetalRough),
                                                         Image(surface.Emissive),
                                                         Image(surface.SpecularStrength),
                                                         Image(surface.SpecularTint),
                                                         Image(surface.DiffuseReflection),
                                                         Image(surface.DiffuseTransmission),
                                                         Image(surface.Colour),
                                                         Image(surface.Colour),
                                                         Image(surface.Colour)};
  if (Binding().Behind != nullptr) {
    images[kSubjectMaterialImages] = {.Image = {.Image = Binding().Behind},
                                      .Sampling = Binding().BehindSampler};
  }
  if (Atlas_ != nullptr) {
    images[kSubjectMaterialImages + 1] = {.Image = {.Image = Atlas_}, .Sampling = AtlasSampler_};
  }
  if (Reflections_.Atlas != nullptr) {
    images[kSubjectMaterialImages + 2] = {.Image = {.Image = Reflections_.Atlas},
                                          .Sampling = Reflections_.Sampler};
  }
  const SurfaceBindings selected(
      layout,
      surface.Kind,
      surface.Domain,
      ColourAttachment(Binding().Colours, Resource::SceneSurfaceIdentity),
      Binding().CapturesMaterial);
  for (uint32_t at = 0; at < selected.Count; ++at) {
    into.Bindings[into.BindingCount++] = {
        .Set = 2, .Slot = at, .Value = images[selected.Images[at]]};
  }
  if (surface.Domain == SurfaceDomain::Ground) {
    into.Bindings[into.BindingCount++] = Storage(2, selected.Count, GroundClasses_);
    into.Bindings[into.BindingCount++] = Storage(2, selected.Count + 1, GroundPalette_);
  }
  if (selected.Shape.FragmentUniformBuffers > 0) {
    into.Bindings[into.BindingCount++] = {
        .Set = 3,
        .Slot = 0,
        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(surface.Row))}};
  }
  if (selected.Shape.FragmentUniformBuffers > 1) {
    into.Bindings[into.BindingCount++] = {
        .Set = 3,
        .Slot = 1,
        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(uniforms.Light))}};
  }
}

SubjectDraw::FrameUniforms SubjectDraw::FrameUniformsOf(const FrameContext &ctx) {
  FrameUniforms uniforms;
  constexpr size_t anchorSlot = 48;
  constexpr size_t previousAnchorSlot = 52;
  for (int axis = 0; axis < 3; ++axis) {
    uniforms.View[anchorSlot + axis] =
        static_cast<float>(Anchor[axis] + ctx.PreViewTranslation[axis]);
    uniforms.View[previousAnchorSlot + axis] =
        static_cast<float>(Anchor[axis] + ctx.PrevPreViewTranslation[axis]);
  }
  for (int i = 0; i < 16; ++i) {
    uniforms.View[i] = ctx.Mvp[i];
    uniforms.View[16 + i] = ctx.PrevMvp[i];
    uniforms.View[32 + i] = static_cast<float>(LightFromWorld_[i]);
  }
  ++UniformPushes_;
  uniforms.Light = PackedLights(ctx);
  ShadowedFrames_ += uniforms.Light[2] > 0.5f ? 1u : 0u;
  return uniforms;
}

size_t SubjectDraw::IndirectRunEnd(size_t first) const noexcept {
  const auto &batch = Batches[first];
  size_t end = first + 1;
  while (end < Batches.size() && Batches[end].JobCount > 0 &&
         Batches[end].MaterialSlot == batch.MaterialSlot &&
         BatchLayout[end] == BatchLayout[first] && Batches[end].Motion == batch.Motion) {
    ++end;
  }
  return end;
}

Native::Result<void> SubjectDraw::DrawBatchRange(const PassRecording &into,
                                                 size_t first,
                                                 bool culled,
                                                 size_t end,
                                                 const DrawResources &resources) {
  const auto &batch = Batches[first];
  const auto &surface = Slots[batch.MaterialSlot];
  const auto pipeline =
      PipelineAt(surface.Domain, BatchLayout[first], surface.Kind, surface.CullsBack);
  const auto &index = Bound().Buffer(culled ? SubjectResidency::Stream::DrawIndex
                                            : SubjectResidency::Stream::Index);
  if (into.Commands == nullptr || !index || !Binding().Pipelines[pipeline]) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "subject draw is missing native resources"});
  }
  const Native::DrawState state{
      .Pipeline = Binding().Pipelines[pipeline].get(),
      .Bindings = std::span(resources.Bindings).first(resources.BindingCount),
      .Vertices = std::span(resources.Vertices).first(resources.VertexCount),
      .Area = into.Area,
      .Clip = into.Clip};
  const Native::IndexBuffer indices{.Data = index.get(),
                                    .Range = {.Bytes = index->Description().Bytes}};
  Native::Result<void> recorded;
  if (culled) {
    recorded = into.Commands->DrawIndexedIndirect(
        {.State = state,
         .Indices = indices,
         .Range = {.Data = Bound().Buffer(SubjectResidency::Stream::DrawArguments).get(),
                   .Offset = first * Native::kIndexedIndirectBytes,
                   .Count = static_cast<uint32_t>(end - first)}});
  } else {
    recorded = into.Commands->DrawIndexed({.State = state,
                                           .Indices = indices,
                                           .Range = {.First = batch.FirstIndex,
                                                     .Count = batch.IndexCount,
                                                     .Instances = batch.Instances,
                                                     .FirstInstance = batch.ModelSlot}});
  }
  if (recorded) { ++EncodedDrawCalls_; }
  return recorded;
}

Native::Result<void> SubjectDraw::Encode(const FrameContext &ctx, const PassRecording &into) {
  EncodedDrawCalls_ = 0;
  const bool drawsBatches = !Batches.empty() && Bound().Buffer(SubjectResidency::Stream::Vertex) &&
                            Bound().Buffer(SubjectResidency::Stream::Index);
  if (!drawsBatches && (Binding().Behind != nullptr || Ground_.Drawn() == 0)) { return {}; }
  const auto uniforms = FrameUniformsOf(ctx);
  const bool cut = Bound().Buffer(SubjectResidency::Stream::DrawIndex) &&
                   Bound().Buffer(SubjectResidency::Stream::DrawArguments) && !Args_.empty();
  std::pair bound{kPipelines, VertexMotion::Deforming};
  uint32_t boundSlot = kNoSlot;
  DrawResources resources;
  for (size_t at = 0; drawsBatches && at < Batches.size(); ++at) {
    const auto &batch = Batches[at];
    const auto &surface = Slots[batch.MaterialSlot];
    const bool glass =
        surface.Kind == SurfaceKind::ThinTransmissive || surface.Kind == SurfaceKind::Refractive;
    if (glass != (Binding().Behind != nullptr)) { continue; }
    const auto wanted = BatchLayout[at];
    const std::pair key{PipelineAt(surface.Domain, wanted, surface.Kind, surface.CullsBack),
                        batch.Motion};
    if (key != bound) {
      resources = {};
      BindVertexStreams(resources, wanted, batch.Motion);
      BindPlacementStorage(resources, wanted);
      resources.Bindings[resources.BindingCount++] = {
          .Set = 1,
          .Slot = 0,
          .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(uniforms.View))}};
      bound = key;
      boundSlot = kNoSlot;
    }
    if (boundSlot != batch.MaterialSlot) {
      resources.BindingCount =
          2u + (CarriesColour(wanted) ? 1u : 0u) + (CarriesTangent(wanted) ? 1u : 0u);
      BindSlot(resources, batch.MaterialSlot, wanted, uniforms);
      boundSlot = batch.MaterialSlot;
    }
    const bool culled = cut && batch.JobCount > 0;
    const size_t end = culled ? IndirectRunEnd(at) : at + 1;
    const auto recorded = DrawBatchRange(into, at, culled, end, resources);
    if (!recorded) { return recorded; }
    at = end - 1;
  }
  return EncodeGround(into, uniforms);
}

Native::Result<void> SubjectDraw::EncodeGround(const PassRecording &into,
                                               const FrameUniforms &uniforms) const {
  if (Binding().Behind != nullptr || Ground_.Drawn() == 0) { return {}; }
  for (const auto &surface : Slots) {
    if (surface.Domain != SurfaceDomain::Ground) { continue; }
    std::array<Native::Binding, 7> bindings;
    size_t count = 0;
    if (!Binding().CapturesMaterial) {
      auto shadow = Image(surface.Colour);
      auto reflection = shadow;
      if (Atlas_ != nullptr) { shadow = {.Image = {.Image = Atlas_}, .Sampling = AtlasSampler_}; }
      if (Reflections_.Atlas != nullptr) {
        reflection = {.Image = {.Image = Reflections_.Atlas}, .Sampling = Reflections_.Sampler};
      }
      bindings[count++] = {.Set = 2, .Slot = 0, .Value = shadow};
      bindings[count++] = {.Set = 2, .Slot = 1, .Value = reflection};
    }
    const uint32_t firstStorage = Binding().CapturesMaterial ? 0u : 2u;
    bindings[count++] = Storage(2, firstStorage, GroundClasses_);
    bindings[count++] = Storage(2, firstStorage + 1, GroundPalette_);
    bindings[count++] = {.Set = 1,
                         .Slot = 0,
                         .Value =
                             Native::Uniform{.Bytes = std::as_bytes(std::span(uniforms.View))}};
    bindings[count++] = {.Set = 3,
                         .Slot = 0,
                         .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(surface.Row))}};
    bindings[count++] = {.Set = 3,
                         .Slot = 1,
                         .Value =
                             Native::Uniform{.Bytes = std::as_bytes(std::span(uniforms.Light))}};
    return Ground_.Encode(into, std::span(bindings).first(count));
  }
  return {};
}

Native::Result<void> SubjectDraw::EncodeShadow(const PassRecording &into,
                                               const Native::GraphicsPipeline &pipeline,
                                               const DrawBatch &batch,
                                               uint32_t instances,
                                               std::span<const std::byte> view) const {
  if (instances == 0) { return {}; }
  if (into.Commands == nullptr || !Bound().Buffer(SubjectResidency::Stream::Index)) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "subject shadow draw is missing native resources"});
  }
  using S = SubjectResidency::Stream;
  DrawResources resources;
  resources.Vertices[resources.VertexCount++] = Vertex(0, Bound().Buffer(S::Vertex).get());
  resources.Bindings[resources.BindingCount++] = Storage(0, 0, Bound().Buffer(S::Placements).get());
  resources.Bindings[resources.BindingCount++] = {
      .Set = 1, .Slot = 0, .Value = Native::Uniform{.Bytes = view}};
  if (batch.Kind == SurfaceKind::Masked) {
    const auto &surface = Slots[batch.MaterialSlot];
    if (CarriesUv(batch.Layout)) {
      resources.Vertices[resources.VertexCount++] = Vertex(1, Bound().Buffer(S::Uv).get());
      resources.Bindings[resources.BindingCount++] = {
          .Set = 2, .Slot = 0, .Value = Image(surface.Colour)};
    }
    if (CarriesUv1(batch.Layout)) {
      resources.Vertices[resources.VertexCount++] = Vertex(2, Bound().Buffer(S::Uv1).get());
    }
    if (CarriesColour(batch.Layout)) {
      resources.Bindings[resources.BindingCount++] = Storage(0, 1, Bound().Buffer(S::Colour).get());
    }
    resources.Bindings[resources.BindingCount++] = {
        .Set = 3,
        .Slot = 0,
        .Value = Native::Uniform{.Bytes = std::as_bytes(std::span(surface.Row))}};
  }
  const auto &index = Bound().Buffer(S::Index);
  return into.Commands->DrawIndexed(
      {.State = {.Pipeline = &pipeline,
                 .Bindings = std::span(resources.Bindings).first(resources.BindingCount),
                 .Vertices = std::span(resources.Vertices).first(resources.VertexCount),
                 .Area = into.Area,
                 .Clip = into.Clip},
       .Indices = {.Data = index.get(), .Range = {.Bytes = index->Description().Bytes}},
       .Range = {.First = batch.FirstIndex,
                 .Count = batch.IndexCount,
                 .Instances = instances,
                 .FirstInstance = batch.ModelSlot}});
}
}
