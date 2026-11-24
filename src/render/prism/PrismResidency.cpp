#include "PrismResidency.h"
#include "PrismPlacements.h"
#include "UploadQueue.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Render {
Native::Result<void> PrismResidency::Uses(Native::Device &device, UploadQueue *queue) {
  if ((Device_ != nullptr && Device_ != &device && !Products_.empty()) ||
      (queue != nullptr && &queue->Device() != &device)) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism products belong to another device"});
  }
  Device_ = &device;
  Queue_ = queue;
  return {};
}

bool PrismResidency::ReuseFrom(const PrismResidency &source) {
  if (!Products_.empty() || (Device_ != nullptr && Device_ != source.Device_)) { return false; }
  if (source.Products_.empty()) { return true; }
  Device_ = source.Device_;
  Products_ = source.Products_;
  Next_ = source.Next_;
  Generation_ = source.Generation_;
  ShadowChanges_.Unknown();
  return true;
}

uint64_t PrismResidency::HeldBytes() const noexcept {
  uint64_t bytes = 0;
  for (const auto &product : Products_) {
    for (const auto &buffer : product.Buffers) {
      if (buffer) { bytes += buffer->Description().Bytes; }
    }
  }
  return bytes;
}

Native::Result<uint64_t> PrismResidency::Place(std::shared_ptr<const PrismGrid> source,
                                               std::span<const PrismInstance> instances,
                                               uint32_t material) {
  if (Device_ == nullptr || !source || Next_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "prism registration requires a source and device"});
  }
  auto info = InspectPrismGrid(*source);
  if (!info) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid prism sample ranges or bounds"});
  }
  PrismProduct product{.Id = Next_,
                       .Material = material,
                       .Source = std::move(source),
                       .Info = std::move(*info),
                       .Records = {},
                       .Buffers = {}};
  if (auto replaced = ReplaceInstances(product, instances); !replaced) {
    return std::unexpected(replaced.error());
  }
  Products_.push_back(std::move(product));
  return Next_++;
}

Native::Result<uint64_t> PrismResidency::PlaceConvex(std::span<const ConvexPrism> shapes,
                                                     uint32_t material) {
  if (Device_ == nullptr || shapes.empty() || Next_ == std::numeric_limits<uint64_t>::max()) {
    return std::unexpected(
        Native::Error{.Kind = Native::ErrorKind::Invalid,
                      .Message = "convex prism registration requires shapes and device"});
  }
  auto packed = PackConvexPrisms(shapes);
  if (!packed) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid convex prism or placement"});
  }
  const auto samples = std::as_bytes(std::span(packed->Samples));
  const auto patches = std::as_bytes(std::span(packed->Patches));
  auto uploaded = UploadPrismBuffers(*Device_, Queue_, samples, patches);
  if (!uploaded) { return std::unexpected(uploaded.error()); }
  PrismProduct product{.Id = Next_,
                       .Material = material,
                       .Source = {},
                       .Info = {},
                       .Records = {},
                       .Buffers = {std::move((*uploaded)[0]), std::move((*uploaded)[1])}};
  RecordShadowChanges(product, packed->Patches);
  product.Records = std::move(packed->Patches);
  Products_.push_back(std::move(product));
  Costs_.SampleBytes += samples.size();
  Costs_.PatchBytes += patches.size();
  ++Costs_.Submissions;
  ++Generation_;
  return Next_++;
}

Native::Result<void> PrismResidency::ReplaceInstances(PrismProduct &product,
                                                      std::span<const PrismInstance> instances) {
  auto records = PackPrismPlacements(*product.Source, product.Info, instances);
  if (!records) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "invalid prism instance or placement"});
  }
  if (*records == product.Records) { return {}; }
  if (records->empty()) {
    RecordShadowChanges(product, *records);
    product.Records.clear();
    product.Buffers[1].reset();
    ++Generation_;
    return {};
  }
  const auto samples = product.Buffers[0] ? std::span<const std::byte>{}
                                          : std::as_bytes(std::span(*product.Source->Samples));
  const auto patches = std::as_bytes(std::span(*records));
  auto uploaded = UploadPrismBuffers(*Device_, Queue_, samples, patches);
  if (!uploaded) { return std::unexpected(uploaded.error()); }
  if ((*uploaded)[0]) { product.Buffers[0] = std::move((*uploaded)[0]); }
  product.Buffers[1] = std::move((*uploaded)[1]);
  RecordShadowChanges(product, *records);
  product.Records = std::move(*records);
  Costs_.SampleBytes += samples.size();
  Costs_.PatchBytes += patches.size();
  ++Costs_.Submissions;
  ++Generation_;
  return {};
}

Native::Result<void> PrismResidency::SetInstances(uint64_t id,
                                                  std::span<const PrismInstance> instances) {
  const auto found = std::ranges::find(Products_, id, &PrismProduct::Id);
  if (found == Products_.end() || !found->Source) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "prism grid is no longer resident"});
  }
  return ReplaceInstances(*found, instances);
}

bool PrismResidency::Release(uint64_t id) {
  const auto found = std::ranges::find(Products_, id, &PrismProduct::Id);
  if (found == Products_.end()) { return false; }
  RecordShadowChanges(*found, {});
  if (!found->Records.empty()) { ++Generation_; }
  Products_.erase(found);
  return true;
}

Boxf PrismResidency::Bounds() const noexcept {
  Boxf bounds;
  for (const auto &product : Products_) {
    for (const auto &record : product.Records) {
      bounds.Cover(record.LocalBounds().Through(record.Current));
    }
  }
  return bounds;
}

}
