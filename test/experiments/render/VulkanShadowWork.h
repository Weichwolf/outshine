#ifndef OUTSHINE_EXPERIMENT_VULKANSHADOWWORK_H
#define OUTSHINE_EXPERIMENT_VULKANSHADOWWORK_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>

namespace outshine::Experiment {
inline constexpr uint32_t kNoShadowPatch = std::numeric_limits<uint32_t>::max();
inline constexpr size_t kShadowPatchesMost = 16384;

struct ShadowPatchAddress {
  uint32_t Region = 0;
  int32_t X = 0, Y = 0;
  uint32_t Width = 0, Height = 0;
  [[nodiscard]] bool operator==(const ShadowPatchAddress &) const = default;
};

struct ShadowIndexedPacket {
  uint32_t FirstIndex = 0, IndexCount = 0, Instances = 0, FirstInstance = 0;
  int32_t VertexOffset = 0;
  uint32_t VertexBindings = 0;
  bool PipelineKnown = false, Fragment = false;

  [[nodiscard]] uint64_t Requests() const noexcept { return uint64_t{IndexCount} * Instances; }
};

struct ShadowPatchWork {
  ShadowPatchAddress Address;
  uint32_t Previous = kNoShadowPatch, Queue = 0;
  uint64_t Serial = 0;
  uint64_t IndexedPackets = 0, IndexRequests = 0, IndexedInstances = 0;
  uint64_t InstanceIndexRequests = 0, OneStreamRequests = 0, TwoStreamRequests = 0;
  uint64_t OtherStreamRequests = 0, UnknownPipelinePackets = 0, FragmentRequests = 0;
  uint64_t NonIndexedPackets = 0, InstanceVertexRequests = 0;
  uint64_t IndirectPackets = 0, IndirectRecords = 0;
  std::array<ShadowIndexedPacket, 4> Largest{};
};

struct ShadowSubmission {
  uint64_t Serial = 0;
  uint32_t Queue = 0;
};

class ShadowWorkTable {
public:
  void Indexed(uint32_t &head, ShadowPatchAddress address, ShadowIndexedPacket packet) noexcept {
    auto *row = Patch(head, address);
    if (row == nullptr) { return; }
    Add(row->IndexedPackets, 1);
    Add(row->IndexRequests, packet.IndexCount);
    Add(row->IndexedInstances, packet.Instances);
    Add(row->InstanceIndexRequests, packet.Requests());
    if (!packet.PipelineKnown) {
      Add(row->UnknownPipelinePackets, 1);
      Add(row->OtherStreamRequests, packet.Requests());
    } else if (packet.VertexBindings == 1) {
      Add(row->OneStreamRequests, packet.Requests());
    } else if (packet.VertexBindings == 2) {
      Add(row->TwoStreamRequests, packet.Requests());
    } else {
      Add(row->OtherStreamRequests, packet.Requests());
    }
    if (packet.Fragment) { Add(row->FragmentRequests, packet.Requests()); }
    for (auto &largest : row->Largest) {
      if (packet.Requests() > largest.Requests()) { std::swap(largest, packet); }
    }
  }

  void NonIndexed(uint32_t &head,
                  ShadowPatchAddress address,
                  uint32_t vertices,
                  uint32_t instances) noexcept {
    auto *row = Patch(head, address);
    if (row == nullptr) { return; }
    Add(row->NonIndexedPackets, 1);
    Add(row->InstanceVertexRequests, uint64_t{vertices} * instances);
  }

  void Indirect(uint32_t &head, ShadowPatchAddress address, uint32_t records) noexcept {
    auto *row = Patch(head, address);
    if (row == nullptr) { return; }
    Add(row->IndirectPackets, 1);
    Add(row->IndirectRecords, records);
  }

  void Submitted(uint32_t head, ShadowSubmission into) noexcept {
    while (head != kNoShadowPatch) {
      if (head >= Count_ || Rows_[head].Serial != 0) {
        ++Dropped_;
        return;
      }
      auto &row = Rows_[head];
      row.Serial = into.Serial;
      row.Queue = into.Queue;
      head = row.Previous;
    }
  }

  [[nodiscard]] std::span<const ShadowPatchWork> Rows() const noexcept {
    return std::span(Rows_).first(Count_);
  }

  [[nodiscard]] uint64_t Dropped() const noexcept { return Dropped_; }

private:
  void Add(uint64_t &value, uint64_t more) noexcept {
    if (more > std::numeric_limits<uint64_t>::max() - value) {
      ++Dropped_;
      return;
    }
    value += more;
  }

  ShadowPatchWork *Patch(uint32_t &head, ShadowPatchAddress address) noexcept {
    for (uint32_t at = head; at != kNoShadowPatch; at = Rows_[at].Previous) {
      if (at >= Count_ || Rows_[at].Serial != 0) {
        ++Dropped_;
        return nullptr;
      }
      if (Rows_[at].Address == address) { return &Rows_[at]; }
    }
    if (Count_ == Rows_.size()) {
      ++Dropped_;
      return nullptr;
    }
    auto &row = Rows_[Count_];
    row.Address = address;
    row.Previous = head;
    head = static_cast<uint32_t>(Count_++);
    return &row;
  }

  std::array<ShadowPatchWork, kShadowPatchesMost> Rows_{};
  size_t Count_ = 0;
  uint64_t Dropped_ = 0;
};
}

#endif
