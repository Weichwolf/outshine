#include "PreparedOsmTiles.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
bool PreparedOsmTiles::AdmitOwner(Work &work, AssetPipeline::RequestTicket &ticket, size_t bytes) {
  const auto admitted = ticket.SetConsumerBytes(bytes);
  if (admitted) { return true; }
  if (admitted.error() != AssetPublicationError::ResourceDeferred) {
    work.Result = std::unexpected("native OSM owner admission failed");
  }
  return false;
}

bool PreparedOsmTiles::Bind(Work &work,
                            const AssetPublication &publication,
                            AssetPipeline::RequestTicket &ticket) {
  if (work.Bound || publication.RequestHit) {
    work.Bound = true;
    return true;
  }
  const auto bound = Cache_->BindRequestIfUnbound(publication.Receipt->Key, work.Request);
  if (!bound) {
    if (bound.error() != AssetCacheError::Busy) {
      work.Result = std::unexpected("native OSM request binding failed");
    }
    return false;
  }
  auto current = Cache_->FindRequest(work.Request);
  if (!current) {
    if (current.error() != AssetCacheError::Busy) {
      work.Result = std::unexpected("native OSM binding lookup failed");
    }
    return false;
  }
  if (!*current) { return RestoreBinding(work, publication, ticket); }
  if ((**current).Key != publication.Receipt->Key) { return LoadWinner(work, **current, ticket); }
  work.RepairPublication.reset();
  work.Bound = true;
  return true;
}

bool PreparedOsmTiles::RestoreBinding(Work &work,
                                      const AssetPublication &publication,
                                      AssetPipeline::RequestTicket &ticket) {
  const auto bytesMost = publication.Native->Bytes().size();
  size_t required = 0;
  if (work.RepairPublication) {
    required = work.RepairPublication->CapacityBytes();
  } else {
    const auto buffers = AssetCache::PublicationBufferBytes(bytesMost);
    if (!buffers || *buffers > std::numeric_limits<size_t>::max() - bytesMost - kReceiptBytesMost) {
      work.Result = std::unexpected("native OSM repair storage plan failed");
      return false;
    }
    required = bytesMost + *buffers;
  }
  if (!AdmitOwner(work, ticket, required + kReceiptBytesMost)) { return false; }
  if (!work.RepairPublication) {
    auto record = *publication.Receipt;
    record.Package.clear();
    record.OffsetBytes = 0;
    record.ByteCount = publication.Native->Bytes().size();
    auto prepared =
        Cache_->PreparePublication({std::move(record)},
                                   std::vector<uint8_t>(publication.Native->Bytes().begin(),
                                                        publication.Native->Bytes().end()));
    if (!prepared) {
      work.Result = std::unexpected("native OSM repair publication failed");
      return false;
    }
    work.RepairPublication.emplace(std::move(*prepared));
  }
  auto restored = Cache_->Publish(*work.RepairPublication);
  if (!restored) {
    if (restored.error() == AssetCacheError::Busy) {
      ++PublicationDeferred_;
    } else {
      work.Result = std::unexpected("native OSM binding repair failed");
    }
    return false;
  }
  ++Writes_;
  work.RepairPublication.reset();
  return false;
}

bool PreparedOsmTiles::LoadWinner(Work &work,
                                  const AssetRecord &record,
                                  AssetPipeline::RequestTicket &ticket) {
  work.RepairPublication.reset();
  const auto bytes = Cache_->PlanRead(record.Key, kPackageBytesMost);
  if (!bytes) {
    if (bytes.error() != AssetCacheError::Busy) {
      work.Result = std::unexpected("winning native OSM metadata failed");
    }
    return false;
  }
  if (!*bytes) { return false; }
  auto plan = **bytes;
  plan.PackageBytesMost = std::max(size_t{1}, plan.PackageBytesMost);
  if (!AdmitOwner(work, ticket, plan.BufferBytesMost + kReceiptBytesMost)) { return false; }
  auto loaded = Load(work.Request, plan);
  if (!loaded) {
    if (loaded.error() != AssetCacheError::Busy &&
        loaded.error() != AssetCacheError::CapacityExceeded) {
      work.Result = std::unexpected("winning native OSM content failed");
    }
    return false;
  }
  if (!*loaded) { return false; }
  PublicationReadBytes_ += (**loaded).Bytes().size();
  work.Override.emplace(std::move(**loaded));
  work.Bound = true;
  return true;
}

bool PreparedOsmTiles::Matches(const OsmField &field,
                               const Work &work,
                               std::string_view key) const {
  const auto &layout = work.Definition;
  if (field.Tiles_.empty() || field.Zoom_ != layout.Zoom || field.Schema_ != layout.Schema ||
      field.Layers_ != layout.Layers) {
    return false;
  }
  const auto &tile = field.Tiles_.front();
  return RequestKey({.X = tile.X, .Y = tile.Y}, layout) == work.Request &&
         ProductKey(work.Request, tile.InputDigest, tile.Source.SourceId, tile.Source.Revision) ==
             key;
}

bool PreparedOsmTiles::RepairContent(Work &work,
                                     const CachedAsset &native,
                                     AssetPipeline::RequestTicket &ticket) {
  const auto bytes = Cache_->PlanRead(native.Record().Key, kPackageBytesMost);
  if (!bytes) {
    if (bytes.error() != AssetCacheError::Busy) {
      work.Result = std::unexpected("invalid native OSM metadata failed");
    }
    return false;
  }
  if (!*bytes) { return true; }
  auto plan = **bytes;
  plan.PackageBytesMost = std::max(size_t{1}, plan.PackageBytesMost);
  const auto backing = work.Override ? work.Override->BackingBytes() : 0;
  if (!AdmitOwner(work, ticket, plan.BufferBytesMost + backing + kReceiptBytesMost)) {
    return false;
  }
  const auto reread =
      Cache_->LoadValidated(native.Record().Key, "osm-tile", plan, [&](const CachedAsset &current) {
        return Sha256Hex(current.Bytes().data(), current.Bytes().size()) != work.BadDigest;
      });
  if (reread) { return true; }
  if (reread.error() != AssetCacheError::Busy &&
      reread.error() != AssetCacheError::CapacityExceeded) {
    work.Result = std::unexpected("invalid native OSM repair failed");
  }
  return false;
}

void PreparedOsmTiles::Repair(Work &work,
                              const CachedAsset &native,
                              AssetPipeline::RequestTicket &ticket) {
  if (work.BadBinding) {
    auto binding = native.Record();
    binding.RequestKey = work.Request;
    const auto unbound = Cache_->UnbindRequestIfMatches(binding);
    if (!unbound) {
      if (unbound.error() != AssetCacheError::Busy) {
        work.Result = std::unexpected("native OSM binding repair failed");
      }
      return;
    }
  } else if (!RepairContent(work, native, ticket)) {
    return;
  }
  if (work.Override) {
    work.Override.reset();
    work.Bound = false;
    work.BadBinding = false;
    work.BadDigest.clear();
    return;
  }
  if (!ticket.Reject()) {
    work.Result = std::unexpected("native OSM receipt could not be rejected");
    return;
  }
  work.Retry = true;
  ticket.Cancel();
}

void PreparedOsmTiles::Decode(Work &work, const AssetPublication &publication) {
  if (!work.Ticket || !publication.Native || !publication.Receipt) {
    work.Result = std::unexpected("native OSM publication has no native backing or ticket");
    return;
  }
  auto &ticket = *work.Ticket;
  if (!Bind(work, publication, ticket)) { return; }
  const auto *native = work.Override ? work.Override->Native() : publication.Native.get();
  if (work.BadBinding || !work.BadDigest.empty()) {
    Repair(work, *native, ticket);
    return;
  }
  const auto bytes = OsmField::NativeTileDecodeBytes(native->Bytes(), kHeapBytesMost);
  if (!bytes) {
    work.BadBinding = publication.RequestHit || work.Override.has_value();
    if (!work.BadBinding) {
      work.BadDigest = Sha256Hex(native->Bytes().data(), native->Bytes().size());
    }
    return;
  }
  const auto backing = work.Override ? work.Override->BackingBytes() : 0;
  if (!AdmitOwner(work, ticket, *bytes + backing + kReceiptBytesMost)) { return; }
  auto field = OsmField::DecodeNativeTile(native->Bytes(), *bytes);
  if (!field || !Matches(*field, work, native->Record().Key)) {
    work.BadBinding = publication.RequestHit || work.Override.has_value();
    field.reset();
    if (!work.BadBinding) {
      work.BadDigest = Sha256Hex(native->Bytes().data(), native->Bytes().size());
    }
    return;
  }
  work.Override.reset();
  const auto retained = ticket.SetConsumerBytes(*bytes);
  if (!retained) {
    work.Result = std::unexpected("native OSM decoder admission cancelled");
    return;
  }
  const auto retainedOwner = AssetPipeline::RetainConsumerBytes(work.Consumer, ticket);
  if (!retainedOwner) {
    work.Result = std::unexpected("native OSM owner retention cancelled");
    return;
  }
  work.Result = std::move(field);
}
}
