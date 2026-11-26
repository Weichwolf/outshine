#include "MapterhornAvailability.h"
#include "RawSourceCache.h"
#include "SourceRange.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators::Terrain {
namespace {

using Data::Address;
using Data::ByteRange;
using Data::Source;
using Data::Wire;
using Availability = Source::Availability;

constexpr int kArchiveZoom = 6;
constexpr int kPlanetMaximumZoom = 12;
constexpr size_t kArchivesMost = 8;
constexpr size_t kLeavesPerArchiveMost = 8;
constexpr ByteRange kRootRange{.First = 0, .Length = PmTilesDirectory::RootWindowBytes};

std::string ArchiveUrl(Data::TileId region) {
  return "https://download.mapterhorn.com/" + std::to_string(region.Zoom) + "-" +
         std::to_string(region.X) + "-" + std::to_string(region.Y) + ".pmtiles";
}

bool Matches(const Data::RangeResponse &origin,
             ByteRange range,
             const Data::RangeResponse *binding) {
  return origin.Bytes == range &&
         (binding == nullptr ||
          (origin.EntityTag == binding->EntityTag && origin.TotalBytes == binding->TotalBytes));
}

}

void MapterhornAvailability::RangeRead::Cancel(Data::SourceTransport &transport) const {
  if (Ticket != Data::Ticket::None && Transport == &transport) { transport.Cancel(Ticket); }
}

MapterhornAvailability::Archive &MapterhornAvailability::Access(Data::TileId region,
                                                                Data::SourceTransport &transport) {
  const auto found = std::ranges::find(Archives_, region, &Archive::Region);
  if (found != Archives_.end()) {
    Archives_.splice(Archives_.begin(), Archives_, found);
    return Archives_.front();
  }
  if (Archives_.size() == kArchivesMost) {
    const auto &victim = Archives_.back();
    victim.Read.Cancel(transport);
    for (const auto &leaf : victim.Leaves) { leaf.Read.Cancel(transport); }
    Archives_.pop_back();
  }
  Archives_.push_front({.Region = region,
                        .Declaration = {.Id = "mapterhorn.coverage",
                                        .Revision = "pmtiles-v3",
                                        .Endpoint = ArchiveUrl(region),
                                        .Wire = Data::WireFormat::TerrariumWebp,
                                        .PayloadSha256 = {},
                                        .Schema = ""},
                        .Leaves = {}});
  return Archives_.front();
}

MapterhornAvailability::Leaf &MapterhornAvailability::AccessLeaf(Archive &archive,
                                                                 ByteRange range,
                                                                 Data::SourceTransport &transport) {
  auto &leaves = archive.Leaves;
  const auto found = std::ranges::find(leaves, range, &Leaf::Range);
  if (found != leaves.end()) {
    leaves.splice(leaves.begin(), leaves, found);
    return leaves.front();
  }
  if (leaves.size() == kLeavesPerArchiveMost) {
    leaves.back().Read.Cancel(transport);
    leaves.pop_back();
  }
  leaves.push_front({.Range = range});
  return leaves.front();
}

MapterhornAvailability::RangeResult
MapterhornAvailability::PollRange(const Archive &archive,
                                  RangeRead &read,
                                  ByteRange range,
                                  const Data::RangeResponse *binding,
                                  Data::RawSourceCache &store,
                                  Data::SourceTransport &transport) {
  if (read.Failed) { return {}; }
  if (read.Ticket == Data::Ticket::None) {
    const std::string_view tag = binding != nullptr ? std::string_view(binding->EntityTag) : "";
    const auto key = Data::ContentKey(archive.Declaration, Address::At(archive.Region), range, tag);
    auto cached =
        store.Lookup(key, static_cast<size_t>(range.Length) + Data::MaximumRangeRecordOverhead);
    if (cached.Where == Data::RawSourceCache::Presence::Bytes) {
      auto origin = Data::UnpackSourceRange(cached.Bytes);
      if (origin && Matches(*origin, range, binding)) {
        return {.Check = {},
                .Record = RangeRecord{.Bytes = std::move(cached.Bytes),
                                      .Origin = std::move(*origin),
                                      .FromStore = true}};
      }
    }
    const auto ticket = transport.Begin(archive.Declaration.Endpoint, range, tag);
    if (!ticket || *ticket == Data::Ticket::None) {
      read.Failed = true;
      return {};
    }
    read.Ticket = *ticket;
    read.Transport = &transport;
    return {.Check = {.State = Availability::Waiting, .StartedRemoteRead = true}};
  }
  if (read.Transport != &transport) { return {}; }
  auto wire = transport.Collect(read.Ticket);
  if (wire.Where() == Wire::State::Working) { return {.Check = {.State = Availability::Waiting}}; }
  read.Ticket = Data::Ticket::None;
  read.Transport = nullptr;
  auto answer = wire.Take();
  if (!answer || answer->Status != Data::kHttpPartialContent || !answer->Range ||
      !answer->Range->Valid(answer->Body.size()) || !Matches(*answer->Range, range, binding)) {
    read.Failed = true;
    return {};
  }
  return {.Check = {},
          .Record =
              RangeRecord{.Bytes = std::move(answer->Body), .Origin = std::move(*answer->Range)}};
}

void MapterhornAvailability::KeepRange(const Archive &archive,
                                       const RangeRecord &record,
                                       std::string_view tag,
                                       Data::RawSourceCache &store) {
  if (record.FromStore || !store.Enabled()) { return; }
  const auto package = Data::PackSourceRange(record.Bytes, record.Origin);
  if (!package) { return; }
  const auto key =
      Data::ContentKey(archive.Declaration, Address::At(archive.Region), record.Origin.Bytes, tag);
  (void)store.Keep(key, package->data(), package->size());
}

Source::AvailabilityCheck MapterhornAvailability::LoadRoot(Archive &archive,
                                                           Data::RawSourceCache &store,
                                                           Data::SourceTransport &transport) {
  auto found = PollRange(archive, archive.Read, kRootRange, nullptr, store, transport);
  if (!found.Record) { return found.Check; }
  auto &record = *found.Record;
  const auto layout = PmTilesDirectory::ReadLayout(record.Bytes, record.Origin.TotalBytes);
  if (!layout) {
    archive.Read.Failed = true;
    return {};
  }
  const auto bytes = std::span(record.Bytes)
                         .subspan(static_cast<size_t>(layout->Root.First),
                                  static_cast<size_t>(layout->Root.Length));
  auto root = PmTilesDirectory::Read(bytes, *layout);
  if (!root) {
    archive.Read.Failed = true;
    return {};
  }
  KeepRange(archive, record, {}, store);
  archive.Directory =
      Index{.Layout = *layout, .Root = std::move(*root), .Origin = std::move(record.Origin)};
  return {};
}

Source::AvailabilityCheck MapterhornAvailability::CheckLeaf(Archive &archive,
                                                            const Index &index,
                                                            const PmTilesDirectory::Entry &entry,
                                                            uint64_t tileId,
                                                            Data::RawSourceCache &store,
                                                            Data::SourceTransport &transport) {
  if (entry.Bytes.Length > PmTilesDirectory::DirectoryBytesMost) { return {}; }
  const ByteRange range{.First = index.Layout.Leaves.First + entry.Bytes.First,
                        .Length = entry.Bytes.Length};
  auto &leaf = AccessLeaf(archive, range, transport);
  if (!leaf.Directory) {
    const auto found = PollRange(archive, leaf.Read, range, &index.Origin, store, transport);
    if (!found.Record) { return found.Check; }
    leaf.Directory = PmTilesDirectory::Read(found.Record->Bytes, index.Layout);
    if (!leaf.Directory) {
      leaf.Read.Failed = true;
      return {};
    }
    KeepRange(archive, *found.Record, index.Origin.EntityTag, store);
  }
  return {.State = leaf.Directory->Find(tileId) != nullptr ? Availability::Direct
                                                           : Availability::Absent};
}

Source::AvailabilityCheck MapterhornAvailability::Check(Data::TileId tile,
                                                        Data::RawSourceCache &store,
                                                        Data::SourceTransport &transport) {
  if (tile.Zoom <= kPlanetMaximumZoom || tile.Zoom > Data::TileId::MaximumZoom) { return {}; }
  const auto drop = static_cast<unsigned>(tile.Zoom - kArchiveZoom);
  const std::scoped_lock lock(Mutex_);
  Archive &archive =
      Access({.Zoom = kArchiveZoom, .X = tile.X >> drop, .Y = tile.Y >> drop}, transport);
  Source::AvailabilityCheck check;
  if (!archive.Directory) { check = LoadRoot(archive, store, transport); }
  if (!archive.Directory) { return check; }
  const auto &index = *archive.Directory;
  if (tile.Zoom < index.Layout.MinimumZoom || tile.Zoom > index.Layout.MaximumZoom) {
    return {.State = Availability::Absent};
  }
  const auto tileId = PmTilesDirectory::TileId(tile);
  const auto *entry = index.Root.Find(tileId);
  if (entry == nullptr) { return {.State = Availability::Absent}; }
  return entry->Run == 0 ? CheckLeaf(archive, index, *entry, tileId, store, transport)
                         : Source::AvailabilityCheck{};
}

}
