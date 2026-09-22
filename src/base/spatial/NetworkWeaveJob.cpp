#include "Wayfinding.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Path {

NetworkWeaveJob::NetworkWeaveJob(Network &&network) : Network_(std::move(network)) {}

std::expected<NetworkWeaveJob, std::string> NetworkWeaveJob::Begin(Network &&network) {
  NetworkWeaveJob job(std::move(network));
  std::string error;
  if (!job.Network_.PrepareWeave(error)) { return std::unexpected(std::move(error)); }
  job.Network_.SortWaysIntoDeclaredOrder();
  job.NodeOf_.resize(job.Network_.Points_.size() / 2u);
  return job;
}

void NetworkWeaveJob::SnapPoints(size_t itemsMost) {
  const size_t end = NextPoint_ + std::min(itemsMost, NodeOf_.size() - NextPoint_);
  for (; NextPoint_ < end; ++NextPoint_) { Network_.SnapOnePoint(NextPoint_, NodeOf_, ByCell_); }
  if (NextPoint_ == NodeOf_.size()) {
    Outgoing_.resize(Network_.Nodes_.size());
    Network_.EdgesFromWays(NodeOf_, Outgoing_);
    TieReachM_ = Network_.TieReachM();
    Network_.Tied_ = 0;
    Stage_ = Stage::IndexEdges;
  }
}

std::expected<void, std::string> NetworkWeaveJob::IndexEdges(size_t itemsMost) {
  size_t visited = 0;
  while (NextNode_ < Outgoing_.size() && visited < itemsMost) {
    if (NextEdge_ == Outgoing_[NextNode_].size()) {
      ++NextNode_;
      NextEdge_ = 0;
      continue;
    }
    const Network::Edge &edge = Outgoing_[NextNode_][NextEdge_++];
    ++visited;
    if (!Indexed_.insert(Network::PhysicalEdgeKey(NextNode_, edge.To)).second) { continue; }
    std::string error;
    if (!Network_.IndexOneEdge(
            {.From = std::min(NextNode_, edge.To), .To = std::max(NextNode_, edge.To)},
            TieReachM_,
            ByEdgeCell_,
            error)) {
      return std::unexpected(std::move(error));
    }
  }
  if (NextNode_ == Outgoing_.size()) {
    NextNode_ = 0;
    NextEdge_ = 0;
    Indexed_.clear();
    Adjacency_ = Network::PhysicalAdjacency(Network_.Nodes_.size());
    Stage_ = Stage::BuildAdjacency;
  }
  return {};
}

void NetworkWeaveJob::BuildAdjacency(size_t itemsMost) {
  size_t visited = 0;
  while (NextNode_ < Outgoing_.size() && visited < itemsMost) {
    if (NextEdge_ == Outgoing_[NextNode_].size()) {
      ++NextNode_;
      NextEdge_ = 0;
      continue;
    }
    Adjacency_.Connect(NextNode_, Outgoing_[NextNode_][NextEdge_++].To);
    ++visited;
  }
  if (NextNode_ == Outgoing_.size()) {
    NextNode_ = 0;
    NextEdge_ = 0;
    Stage_ = Stage::TieEnds;
  }
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
  switch (Stage_) {
    case Stage::SnapPoints: SnapPoints(itemsMost); break;
    case Stage::IndexEdges:
      if (auto indexed = IndexEdges(itemsMost); !indexed) {
        return std::unexpected(indexed.error());
      }
      break;
    case Stage::BuildAdjacency: BuildAdjacency(itemsMost); break;
    case Stage::TieEnds: TieEnds(itemsMost); break;
    case Stage::Publish: Publish(); break;
    case Stage::Done: return true;
  }
  return Stage_ == Stage::Done;
}

std::expected<Network, std::string_view> NetworkWeaveJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("network weave is incomplete"); }
  return std::move(Network_);
}

}
