#include "GroundLattice.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace outshine::Render {
Native::Result<void> GroundLattice::ConfigurePatchSelection() {
  if (Device_ == nullptr) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground selection has no device"});
  }
  return Selection_.Configure(*Device_);
}

Native::Result<void> GroundLattice::SelectPatches(const FrameContext &ctx,
                                                  const Vec3 &anchor,
                                                  const PassRecording &into,
                                                  Native::Buffer &pyramid,
                                                  const PyramidShape &shape) {
  if (!DrawEnabled_ || Drawn() == 0) { return {}; }
  if (into.Commands == nullptr || !PatchList_) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "ground patch input is missing"});
  }
  return Selection_.Cull(*into.Commands, ctx, anchor, into.Area, pyramid, shape, *PatchList_, 0.5f);
}

Native::Result<void> GroundLattice::DrawPatches(const PassRecording &into,
                                                std::span<const Native::Binding> bindings) const {
  const uint32_t count = VisibleReal_ + VisibleVirtual_;
  if (count == 0) { return {}; }
  if (Pipelines_->Patches() == nullptr || into.Commands == nullptr || !PatchGrid_ || !PatchIndex_ ||
      !PatchFractions_ || !PatchList_ || !Visible_ || !Pages_ || !Nearest_) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "ground patch draw is missing native resources"});
  }
  constexpr size_t maximumBindings = PatchShape.VertexSamplers + PatchShape.VertexUniformBuffers +
                                     PatchShape.VertexStorageBuffers + PatchShape.FragmentSamplers +
                                     PatchShape.FragmentUniformBuffers +
                                     PatchShape.FragmentStorageBuffers;
  std::array<Native::Binding, maximumBindings> resources;
  if (bindings.size() + 3u > resources.size()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "ground patch draw has too many shader bindings"});
  }
  std::ranges::copy(bindings, resources.begin());
  resources[bindings.size()] = {
      .Set = 0,
      .Slot = 0,
      .Value = Native::SampledTexture{.Image = {.Image = Pages_.get(), .Layers = PageLayers_},
                                      .Sampling = Nearest_.get()}};
  resources[bindings.size() + 1u] = {
      .Set = 0,
      .Slot = 1,
      .Value = Native::StorageBuffer{.Data = Visible_.get(),
                                     .Range = {.Bytes = Visible_->Description().Bytes}}};
  resources[bindings.size() + 2u] = {
      .Set = 0,
      .Slot = 2,
      .Value = Native::StorageBuffer{.Data = PatchFractions_.get(),
                                     .Range = {.Bytes = PatchFractions_->Description().Bytes}}};
  const std::array vertices{
      Native::VertexBuffer{.Stream = 0,
                           .Data = PatchGrid_.get(),
                           .Range = {.Bytes = PatchGrid_->Description().Bytes}},
      Native::VertexBuffer{.Stream = 1,
                           .Data = Selection_.Ready() ? Selection_.Codes() : PatchList_.get(),
                           .Range = {.Bytes = Selection_.Ready()
                                                  ? Selection_.Codes()->Description().Bytes
                                                  : PatchList_->Description().Bytes}}};
  const Native::DrawState state{.Pipeline = Pipelines_->Patches(),
                                .Bindings = std::span(resources).first(bindings.size() + 3u),
                                .Vertices = vertices,
                                .Area = into.Area,
                                .Clip = into.Clip};
  const Native::IndexBuffer indices{.Data = PatchIndex_.get(),
                                    .Range = {.Bytes = PatchIndex_->Description().Bytes}};
  if (Selection_.Ready()) {
    return into.Commands->DrawIndexedIndirect({.State = state,
                                               .Indices = indices,
                                               .Range = {.Data = Selection_.Arguments(),
                                                         .Offset = Selection_.ArgumentOffset(),
                                                         .Count = 4}});
  }
  return into.Commands->DrawIndexed(
      {.State = state,
       .Indices = indices,
       .Range = {.Count = kPatchIndices, .Instances = count * kPatchCount}});
}
}
