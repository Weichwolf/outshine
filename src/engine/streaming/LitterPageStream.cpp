#include "LitterPageStream.h"
#include "LitterPagePreparation.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <utility>

namespace outshine {
namespace {
using Address = Render::FieldPageAddress;
using Render::Native::Error;
using Render::Native::ErrorKind;

bool Before(Address a, Address b) noexcept {
  return std::tie(a.Mip, a.X, a.Y) < std::tie(b.Mip, b.X, b.Y);
}

Generators::LitterPageAddress NativeAddress(Address address, uint32_t core) noexcept {
  return {.Tile = {address.X, address.Y}, .Level = address.Mip, .CoreTexels = core};
}
}

LitterPageStream::LitterPageStream(AssetPipeline &pipeline,
                                   std::shared_ptr<AssetCache> cache,
                                   Render::R8PageAtlas &atlas,
                                   AssetPipeline::Consumer consumer,
                                   LitterPageStreamLimits limits)
    : Pipeline_(&pipeline),
      Cache_(std::move(cache)),
      Atlas_(&atlas),
      Consumer_(std::move(consumer)),
      Limits_(limits),
      Seen_(atlas.Options().Slots, 0),
      Victims_(atlas.Options().Slots) {
  Wanted_.reserve(limits.DemandMost);
  Pending_.reserve(limits.PendingMost);
  Edits_.reserve(limits.PendingMost);
  Completed_.reserve(limits.PendingMost);
  std::ranges::iota(Victims_, 0u);
}

Render::Native::Result<std::unique_ptr<LitterPageStream>>
LitterPageStream::Create(AssetPipeline &pipeline,
                         std::shared_ptr<AssetCache> cache,
                         Render::R8PageAtlas &atlas,
                         LitterPageStreamLimits limits) {
  if (!cache || limits.DemandMost == 0 || limits.PendingMost == 0 ||
      limits.PendingMost > atlas.Options().EditsPerBatch ||
      limits.PendingMost > atlas.Options().Slots ||
      (atlas.Options().Layout.CoreTexels != 64 && atlas.Options().Layout.CoreTexels != 128)) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid litter page stream limits or atlas"});
  }
  auto consumer = pipeline.OpenConsumer({.Identity = cache});
  if (!consumer) {
    return std::unexpected(Error{.Kind = ErrorKind::Busy,
                                 .Message = "litter page stream consumer admission deferred"});
  }
  return std::unique_ptr<LitterPageStream>(
      new LitterPageStream(pipeline, std::move(cache), atlas, std::move(*consumer), limits));
}

Render::Native::Result<void> LitterPageStream::Observe(std::span<const Address> pages) {
  if (pages.size() > Limits_.DemandMost || std::ranges::any_of(pages, [&](Address page) {
        return !Generators::PlanLitterPage(
            NativeAddress(page, Atlas_->Options().Layout.CoreTexels));
      })) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "litter feedback exceeds its selected bounds"});
  }
  if (Epoch_ == std::numeric_limits<uint64_t>::max()) {
    std::ranges::fill(Seen_, 0);
    for (auto &pending : Pending_) { pending.Seen = 0; }
    Epoch_ = 0;
  }
  ++Epoch_;
  Wanted_.assign(pages.begin(), pages.end());
  std::ranges::sort(Wanted_, Before);
  const auto duplicate = std::ranges::unique(Wanted_);
  Wanted_.erase(duplicate.begin(), duplicate.end());
  for (const auto page : Wanted_) {
    if (const auto slot = Atlas_->Find(page)) { Seen_[*slot] = Epoch_; }
  }
  for (auto &pending : Pending_) {
    if (std::ranges::binary_search(Wanted_, pending.Address, Before)) { pending.Seen = Epoch_; }
  }
  return {};
}

Render::Native::Result<void> LitterPageStream::PrepareEdits() {
  Edits_.clear();
  Completed_.clear();
  bool sorted = false;
  size_t next = 0;
  for (size_t index = 0; index < Pending_.size(); ++index) {
    const auto &pending = Pending_[index];
    const auto result = pending.Ticket.Poll();
    if (result.Status == AssetPublicationStatus::Pending) { continue; }
    if (result.Status != AssetPublicationStatus::Ready || !result.Native) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "litter asset delivery failed: " + result.Message});
    }
    if (Atlas_->Find(pending.Address)) {
      Completed_.push_back(index);
      continue;
    }
    if (!sorted) {
      std::ranges::sort(Victims_, [&](uint32_t a, uint32_t b) {
        return std::tie(Seen_[a], a) < std::tie(Seen_[b], b);
      });
      sorted = true;
    }
    if (next == Victims_.size() || Seen_[Victims_[next]] == Epoch_) {
      ++Counts_.Deferred;
      continue;
    }
    const auto lease = result.Lease();
    if (!lease) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "litter asset has no admitted backing"});
    }
    const auto &record = result.Native->Record();
    const auto bytes = lease->Bytes().subspan(static_cast<size_t>(record.OffsetBytes),
                                              static_cast<size_t>(record.ByteCount));
    const auto decoded = Generators::DecodeLitterPage(
        NativeAddress(pending.Address, Atlas_->Options().Layout.CoreTexels), bytes);
    if (!decoded) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid, .Message = decoded.error()});
    }
    Edits_.push_back(
        {.Slot = Victims_[next++], .Address = pending.Address, .Pixels = decoded->Pixels});
    Completed_.push_back(index);
  }
  return {};
}

Render::Native::Result<void> LitterPageStream::Publish() {
  const auto collected = Atlas_->Collect();
  if (!collected) { return collected; }
  const auto prepared = PrepareEdits();
  if (!prepared) { return prepared; }
  if (!Edits_.empty()) {
    const auto updated = Atlas_->Update(Edits_);
    if (!updated) {
      if (updated.error().Kind == ErrorKind::Busy) {
        ++Counts_.Deferred;
        return {};
      }
      return std::unexpected(updated.error());
    }
    for (const auto &edit : Edits_) { Seen_[edit.Slot] = 0; }
    for (const auto index : Completed_) {
      const auto &pending = Pending_[index];
      const auto slot = Atlas_->Find(pending.Address);
      if (slot) { Seen_[*slot] = std::max(Seen_[*slot], pending.Seen); }
    }
    Counts_.Published += Edits_.size();
  }
  for (const auto index : std::views::reverse(Completed_)) {
    Counts_.CacheHits += Pending_[index].Ticket.Poll().RequestHit ? 1u : 0u;
    Pending_.erase(Pending_.begin() + static_cast<std::ptrdiff_t>(index));
  }
  return {};
}

Render::Native::Result<void> LitterPageStream::Request() {
  for (const auto page : Wanted_) {
    if (Pending_.size() == Limits_.PendingMost) { break; }
    if (Atlas_->Find(page) ||
        std::ranges::find(Pending_, page, &Pending::Address) != Pending_.end()) {
      continue;
    }
    auto request = RequestLitterPage(
        *Pipeline_, Consumer_, Cache_, NativeAddress(page, Atlas_->Options().Layout.CoreTexels));
    if (!request) {
      if (request.error() == AssetPublicationError::ResourceDeferred) {
        ++Counts_.Deferred;
        return {};
      }
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "litter asset request was rejected"});
    }
    Pending_.push_back({.Address = page, .Seen = Epoch_, .Ticket = std::move(*request)});
    ++Counts_.Started;
  }
  return {};
}

Render::Native::Result<void> LitterPageStream::Advance() {
  const auto published = Publish();
  if (!published) { return published; }
  return Request();
}

LitterPageStreamCounts LitterPageStream::Counts() const noexcept {
  auto counts = Counts_;
  counts.Wanted = Wanted_.size();
  counts.Pending = Pending_.size();
  counts.BookkeepingBytes =
      sizeof(*this) + Wanted_.capacity() * sizeof(Address) + Pending_.capacity() * sizeof(Pending) +
      Seen_.capacity() * sizeof(uint64_t) + Victims_.capacity() * sizeof(uint32_t) +
      Edits_.capacity() * sizeof(Render::R8PageEdit) + Completed_.capacity() * sizeof(size_t);
  return counts;
}
}
