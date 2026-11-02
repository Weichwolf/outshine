#include "R8PageAtlas.h"
#include "UploadQueue.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Render {
Native::Result<void> R8PageAtlas::Collect() {
  while (!Retired_.empty()) {
    auto ready = Device_->Poll(Retired_.front().Completion);
    if (!ready) { return std::unexpected(ready.error()); }
    if (*ready == Native::CompletionStatus::Pending) { break; }
    Retired_.erase(Retired_.begin());
  }
  return {};
}

Native::Result<std::optional<Native::Completion>>
R8PageAtlas::Update(std::span<const R8PageEdit> edits) {
  if (edits.size() > Options_.EditsPerBatch) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                         .Message = "R8 atlas update exceeds admitted edit count"});
  }
  auto collected = Collect();
  if (!collected) { return std::unexpected(collected.error()); }
  if (edits.empty()) { return std::optional<Native::Completion>{}; }
  if (Retired_.size() == Options_.PendingBatches) {
    return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Busy,
                                         .Message = "R8 atlas update awaits GPU completion"});
  }
  const size_t side = size_t{Options_.Layout.CoreTexels} + 2;
  auto slots = Slots_;
  std::vector<uint8_t> touched(slots.size());
  for (const auto &edit : edits) {
    if (edit.Slot >= slots.size() || touched[edit.Slot] != 0 ||
        (edit.Address ? edit.Pixels.size() != side * side : !edit.Pixels.empty())) {
      return std::unexpected(Native::Error{.Kind = Native::ErrorKind::Invalid,
                                           .Message = "invalid or repeated R8 atlas slot edit"});
    }
    touched[edit.Slot] = 1;
    slots[edit.Slot] = edit.Address;
  }
  auto table = BuildFieldPageTable(slots, Options_.Layout.Entries, Options_.Layout.Probes);
  if (!table) { return std::unexpected(table.error()); }
  auto submitted = Publish(edits, std::move(*table));
  if (!submitted) { return std::unexpected(submitted.error()); }
  Slots_ = std::move(slots);
  return std::optional{*submitted};
}

Native::Result<Native::Completion> R8PageAtlas::Publish(std::span<const R8PageEdit> edits,
                                                        std::vector<FieldPageEntry> table) {
  auto commands = BeginUpload(*Device_, Uploads_);
  if (!commands) { return std::unexpected(commands.error()); }
  const auto shape = Options_.Layout;
  const uint64_t tableBytes = sizeof(shape) + table.size() * sizeof(FieldPageEntry);
  const uint32_t side = shape.CoreTexels + 2;
  const uint64_t rowBytes = ((uint64_t{side} + 255) / 256) * 256;
  const auto count = static_cast<uint64_t>(std::ranges::count_if(
      edits, [](const R8PageEdit &edit) { return edit.Address.has_value(); }));
  const uint64_t bytes = tableBytes + count * rowBytes * side;
  auto lookup = Device_->CreateBuffer({.Bytes = tableBytes,
                                       .Use = Native::BufferUse::Storage |
                                              Native::BufferUse::CopyDestination |
                                              Native::BufferUse::CopySource});
  if (!lookup) { return std::unexpected(lookup.error()); }
  auto transfer = Device_->CreateBuffer(
      {.Bytes = bytes, .Use = Native::BufferUse::CopySource, .Placement = Native::Memory::Upload});
  if (!transfer) { return std::unexpected(transfer.error()); }
  auto mapped = Device_->MapWrite(**transfer, {.Bytes = bytes});
  if (!mapped) { return std::unexpected(mapped.error()); }
  std::memset(mapped->data(), 0, mapped->size());
  std::memcpy(mapped->data(), &shape, sizeof(shape));
  std::memcpy(mapped->data() + sizeof(shape), table.data(), table.size() * sizeof(FieldPageEntry));
  uint64_t at = tableBytes;
  for (const auto &edit : edits) {
    if (!edit.Address) { continue; }
    for (uint32_t row = 0; row < side; ++row) {
      std::memcpy(
          mapped->data() + at + row * rowBytes, edit.Pixels.data() + size_t{row} * side, side);
    }
    at += rowBytes * side;
  }
  auto unmapped = Device_->Unmap(**transfer);
  if (!unmapped) { return std::unexpected(unmapped.error()); }
  auto copied = (*commands)->Copy(**transfer, **lookup, {.Bytes = tableBytes});
  if (!copied) { return std::unexpected(copied.error()); }
  at = tableBytes;
  for (const auto &edit : edits) {
    if (!edit.Address) { continue; }
    copied = (*commands)->Copy(**transfer,
                               *Image_,
                               {.BufferOffset = at,
                                .RowBytes = rowBytes,
                                .Region = {.X = (edit.Slot % shape.Columns) * side,
                                           .Y = (edit.Slot / shape.Columns) * side,
                                           .Width = side,
                                           .Height = side}});
    if (!copied) { return std::unexpected(copied.error()); }
    at += rowBytes * side;
  }
  auto submitted = SubmitUpload(*Device_, Uploads_, std::move(*commands));
  if (!submitted) { return std::unexpected(submitted.error()); }
  Retired_.push_back({.Lookup = std::move(Lookup_), .Completion = *submitted});
  Lookup_ = std::move(*lookup);
  Table_ = std::move(table);
  return *submitted;
}
}
