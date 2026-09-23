#include "Wayfinding.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Path {

NetworkWeaveJob::NetworkWeaveJob(Network &&network) : Network_(std::move(network)) {}

std::expected<NetworkWeaveJob, std::string> NetworkWeaveJob::Begin(Network &&network) {
  NetworkWeaveJob job(std::move(network));
  std::string error;
  if (!job.Network_.PrepareWeave(error)) { return std::unexpected(std::move(error)); }
  job.Order_.resize(job.Network_.Ways_.size());
  for (size_t at = 0; at < job.Order_.size(); ++at) { job.Order_[at] = at; }
  return job;
}

void NetworkWeaveJob::SortWays() {
  const size_t end = std::min(NextSort_ + kWayRun, Order_.size());
  std::sort(Order_.begin() + static_cast<ptrdiff_t>(NextSort_),
            Order_.begin() + static_cast<ptrdiff_t>(end),
            [this](size_t a, size_t b) { return Network_.WayLess(a, b); });
  NextSort_ = end;
  if (NextSort_ != Order_.size()) { return; }
  SortedOrder_.reserve(Order_.size());
  RunHeap_.reserve((Order_.size() + kWayRun - 1u) / kWayRun);
  for (size_t at = 0; at < Order_.size(); at += kWayRun) {
    RunHeap_.push_back({.At = at, .End = std::min(at + kWayRun, Order_.size())});
  }
  const auto later = [this](RunCursor a, RunCursor b) {
    return Network_.WayLess(Order_[b.At], Order_[a.At]);
  };
  std::make_heap(RunHeap_.begin(), RunHeap_.end(), later);
  Stage_ = Stage::MergeWays;
}

void NetworkWeaveJob::MergeWays(size_t itemsMost) {
  const auto later = [this](RunCursor a, RunCursor b) {
    return Network_.WayLess(Order_[b.At], Order_[a.At]);
  };
  const size_t count = std::min(itemsMost, size_t{4096});
  for (size_t made = 0; made < count && !RunHeap_.empty(); ++made) {
    std::pop_heap(RunHeap_.begin(), RunHeap_.end(), later);
    RunCursor next = RunHeap_.back();
    RunHeap_.pop_back();
    SortedOrder_.push_back(Order_[next.At++]);
    if (next.At < next.End) {
      RunHeap_.push_back(next);
      std::push_heap(RunHeap_.begin(), RunHeap_.end(), later);
    }
  }
  if (RunHeap_.empty()) { Stage_ = Stage::ReservePointCopy; }
}

void NetworkWeaveJob::CopyWays(size_t itemsMost) {
  const size_t count = std::min(itemsMost, size_t{8192});
  size_t made = 0;
  while (NextCopyWay_ < SortedOrder_.size() && made < count) {
    const Network::Way &source = Network_.Ways_[SortedOrder_[NextCopyWay_]];
    if (NextCopyPoint_ == 0) {
      Network::Way moved = source;
      moved.First = SortedPoints_.size() / 2u;
      SortedWays_.push_back(moved);
    }
    const size_t from = source.First + NextCopyPoint_;
    SortedPoints_.push_back(Network_.Points_[2u * from]);
    SortedPoints_.push_back(Network_.Points_[2u * from + 1u]);
    SortedWayOf_.push_back(static_cast<uint32_t>(SortedWays_.size() - 1u));
    ++made;
    if (++NextCopyPoint_ == source.Count) {
      ++NextCopyWay_;
      NextCopyPoint_ = 0;
    }
  }
  if (NextCopyWay_ != SortedOrder_.size()) { return; }
  Network_.Points_ = std::move(SortedPoints_);
  Network_.WayOf_ = std::move(SortedWayOf_);
  Network_.Ways_ = std::move(SortedWays_);
  Stage_ = Stage::BeginSnap;
}

void NetworkWeaveJob::BeginSnap() {
  NodeOf_.resize(Network_.Points_.size() / 2u);
  Stage_ = Stage::SnapPoints;
}

void NetworkWeaveJob::SnapPoints(size_t itemsMost) {
  const size_t end = NextPoint_ + std::min(itemsMost, NodeOf_.size() - NextPoint_);
  for (; NextPoint_ < end; ++NextPoint_) { Network_.SnapOnePoint(NextPoint_, NodeOf_, ByCell_); }
  if (NextPoint_ == NodeOf_.size()) {
    Outgoing_.resize(Network_.Nodes_.size());
    Stage_ = Stage::BuildEdges;
  }
}

void NetworkWeaveJob::BuildEdges(size_t itemsMost) {
  size_t visited = 0;
  while (NextWay_ < Network_.Ways_.size() && visited < itemsMost) {
    const Network::Way &way = Network_.Ways_[NextWay_];
    if (NextStep_ >= way.Count) {
      ++NextWay_;
      NextStep_ = 1;
      continue;
    }
    Network_.AppendWayEdge(way, NextStep_++, NodeOf_, Outgoing_);
    ++visited;
  }
  if (NextWay_ == Network_.Ways_.size()) {
    TieReachM_ = Network_.TieReachM();
    Network_.Tied_ = 0;
    Stage_ = Stage::IndexEdges;
  }
}

std::expected<void, std::string> NetworkWeaveJob::IndexEdges(size_t itemsMost) {
  size_t visited = 0;
  std::string error;
  while (visited < itemsMost) {
    if (IndexCursor_) {
      EdgeIndexCursor &cursor = *IndexCursor_;
      const Network::Node &a = Network_.Nodes_[cursor.Ends.From];
      const Network::Node &b = Network_.Nodes_[cursor.Ends.To];
      if (!cursor.Shape) {
        cursor.Shape = Network_.ShapeRowOver(cursor.Row, Snap{.CellM = TieReachM_});
        const int64_t one = Network::ColumnIn(*cursor.Shape, a.LongitudeDeg);
        const int64_t two = Network::ColumnIn(*cursor.Shape, b.LongitudeDeg);
        cursor.Column = std::min(one, two);
        cursor.LastColumn = std::max(one, two);
      }
      if (!Network_.IndexEdgeCell(
              cursor.Ends,
              {.Row = cursor.Row,
               .Column = ((cursor.Column % cursor.Shape->Columns) + cursor.Shape->Columns) %
                         cursor.Shape->Columns},
              ByEdgeCell_,
              error)) {
        return std::unexpected(std::move(error));
      }
      ++visited;
      if (cursor.Column == cursor.LastColumn) {
        if (cursor.Row == cursor.LastRow) {
          IndexCursor_.reset();
        } else {
          ++cursor.Row;
          cursor.Shape.reset();
        }
      } else {
        ++cursor.Column;
      }
      continue;
    }
    if (NextNode_ == Outgoing_.size()) {
      NextNode_ = 0;
      NextEdge_ = 0;
      Stage_ = Stage::BeginAdjacency;
      break;
    }
    if (NextEdge_ == Outgoing_[NextNode_].size()) {
      ++NextNode_;
      NextEdge_ = 0;
      ++visited;
      continue;
    }
    const Network::Edge &edge = Outgoing_[NextNode_][NextEdge_++];
    ++visited;
    if (!Indexed_.insert(Network::PhysicalEdgeKey(NextNode_, edge.To)).second) { continue; }
    const Network::EdgeEnds ends{.From = std::min(NextNode_, edge.To),
                                 .To = std::max(NextNode_, edge.To)};
    const Network::Node &a = Network_.Nodes_[ends.From];
    const Network::Node &b = Network_.Nodes_[ends.To];
    IndexCursor_ = EdgeIndexCursor{
        .Ends = ends,
        .Row = Network_.RowOver(std::min(a.LatitudeDeg, b.LatitudeDeg), Snap{.CellM = TieReachM_}),
        .LastRow =
            Network_.RowOver(std::max(a.LatitudeDeg, b.LatitudeDeg), Snap{.CellM = TieReachM_}),
        .Shape = std::nullopt};
  }
  return {};
}

void NetworkWeaveJob::BeginAdjacency() {
  Adjacency_.Adopt(Network_.Nodes_.size(), std::move(Indexed_));
  Stage_ = Stage::BuildAdjacency;
}

void NetworkWeaveJob::BuildAdjacency(size_t itemsMost) {
  if (Adjacency_.AccumulateDegrees(itemsMost)) { Stage_ = Stage::TieEnds; }
}

void NetworkWeaveJob::TieEnds(size_t itemsMost) {
  size_t visited = 0;
  while (NextNode_ < Network_.Nodes_.size() && visited < itemsMost) {
    const size_t loose = NextNode_++;
    ++visited;
    if (Adjacency_.Degree(loose) != 1) { continue; }
    const Network::NearestEdge best = Network_.NearestEdgeTo(loose, ByEdgeCell_, TieReachM_);
    if (best.From == Network_.Nodes_.size()) { continue; }
    if (Network_.SpliceInto(loose, best, TieReachM_, Outgoing_, ByEdgeCell_)) {
      Adjacency_.Disconnect(best.From, best.To);
      Adjacency_.Connect(best.From, loose);
      Adjacency_.Connect(loose, best.To);
      ++Network_.Tied_;
    }
  }
  if (NextNode_ == Network_.Nodes_.size()) { Stage_ = Stage::Publish; }
}

void NetworkWeaveJob::Publish() {
  for (size_t node = 0; node < Network_.Nodes_.size(); ++node) {
    Network_.Nodes_[node].FirstEdge = Network_.Edges_.size();
    Network_.Nodes_[node].EdgeCount = Outgoing_[node].size();
    for (const Network::Edge &edge : Outgoing_[node]) { Network_.Edges_.push_back(edge); }
  }
  Network_.NodeOfPoint_.resize(NodeOf_.size());
  for (size_t at = 0; at < NodeOf_.size(); ++at) {
    Network_.NodeOfPoint_[at] = static_cast<uint32_t>(NodeOf_[at]);
  }
  Network_.Cells_ = std::move(ByCell_);
  Network_.Woven_ = true;
  Stage_ = Stage::Done;
}

std::expected<bool, std::string> NetworkWeaveJob::Advance(size_t itemsMost) {
  if (itemsMost == 0) { return std::unexpected("network weave work budget is zero"); }
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  switch (Stage_) {
    case Stage::SortWays: SortWays(); break;
    case Stage::MergeWays: MergeWays(itemsMost); break;
    case Stage::ReservePointCopy:
      SortedPoints_.reserve(Network_.Points_.size());
      Stage_ = Stage::ReserveOwnerCopy;
      break;
    case Stage::ReserveOwnerCopy:
      SortedWayOf_.reserve(Network_.WayOf_.size());
      Stage_ = Stage::ReserveWayCopy;
      break;
    case Stage::ReserveWayCopy:
      SortedWays_.reserve(Network_.Ways_.size());
      Stage_ = Stage::CopyWays;
      break;
    case Stage::CopyWays: CopyWays(itemsMost); break;
    case Stage::BeginSnap: BeginSnap(); break;
    case Stage::SnapPoints: SnapPoints(itemsMost); break;
    case Stage::BuildEdges: BuildEdges(itemsMost); break;
    case Stage::IndexEdges:
      if (auto indexed = IndexEdges(itemsMost); !indexed) {
        return std::unexpected(indexed.error());
      }
      break;
    case Stage::BeginAdjacency: BeginAdjacency(); break;
    case Stage::BuildAdjacency: BuildAdjacency(itemsMost); break;
    case Stage::TieEnds: TieEnds(itemsMost); break;
    case Stage::Publish: Publish(); break;
    case Stage::Done: return true;
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  switch (before) {
    case Stage::SortWays: Worst_.SortMs = std::max(Worst_.SortMs, elapsedMs); break;
    case Stage::MergeWays: Worst_.MergeMs = std::max(Worst_.MergeMs, elapsedMs); break;
    case Stage::ReservePointCopy:
    case Stage::ReserveOwnerCopy:
    case Stage::ReserveWayCopy: Worst_.ReserveMs = std::max(Worst_.ReserveMs, elapsedMs); break;
    case Stage::CopyWays: Worst_.CopyMs = std::max(Worst_.CopyMs, elapsedMs); break;
    case Stage::BeginSnap: Worst_.BeginSnapMs = std::max(Worst_.BeginSnapMs, elapsedMs); break;
    case Stage::SnapPoints: Worst_.SnapMs = std::max(Worst_.SnapMs, elapsedMs); break;
    case Stage::BuildEdges: Worst_.EdgesMs = std::max(Worst_.EdgesMs, elapsedMs); break;
    case Stage::IndexEdges: Worst_.IndexMs = std::max(Worst_.IndexMs, elapsedMs); break;
    case Stage::BeginAdjacency:
      Worst_.AdjacencyBeginMs = std::max(Worst_.AdjacencyBeginMs, elapsedMs);
      break;
    case Stage::BuildAdjacency: Worst_.AdjacencyMs = std::max(Worst_.AdjacencyMs, elapsedMs); break;
    case Stage::TieEnds: Worst_.TieMs = std::max(Worst_.TieMs, elapsedMs); break;
    case Stage::Publish: Worst_.PublishMs = std::max(Worst_.PublishMs, elapsedMs); break;
    case Stage::Done: break;
  }
  return Stage_ == Stage::Done;
}

std::expected<Network, std::string_view> NetworkWeaveJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("network weave is incomplete"); }
  return std::move(Network_);
}

std::expected<bool, std::string_view> NetworkWeaveJob::ReleaseTemporary(size_t itemsMost) {
  if (itemsMost == 0) { return std::unexpected("network weave cleanup budget is zero"); }
  if (Stage_ != Stage::Done) { return std::unexpected("network weave is incomplete"); }
  size_t released = 0;
  switch (ReleaseStage_) {
    case ReleaseStage::Cells:
      NodeOf_.clear();
      while (!ByCell_.empty() && released < itemsMost) {
        ByCell_.erase(ByCell_.begin());
        ++released;
      }
      if (ByCell_.empty()) { ReleaseStage_ = ReleaseStage::Outgoing; }
      break;
    case ReleaseStage::Outgoing:
      while (NextRelease_ < Outgoing_.size() && released < itemsMost) {
        Network::OutgoingEdges::value_type{}.swap(Outgoing_[NextRelease_++]);
        ++released;
      }
      if (NextRelease_ == Outgoing_.size()) {
        Outgoing_.clear();
        NextRelease_ = 0;
        ReleaseStage_ = ReleaseStage::EdgeCells;
      }
      break;
    case ReleaseStage::EdgeCells:
      if (ByEdgeCell_.Release(itemsMost)) { ReleaseStage_ = ReleaseStage::Adjacency; }
      break;
    case ReleaseStage::Adjacency:
      if (Adjacency_.Release(itemsMost)) { ReleaseStage_ = ReleaseStage::Done; }
      break;
    case ReleaseStage::Done: return true;
  }
  return ReleaseStage_ == ReleaseStage::Done;
}

}
