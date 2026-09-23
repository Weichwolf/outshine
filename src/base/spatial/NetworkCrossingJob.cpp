#include "Wayfinding.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <ratio>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Path {

NetworkCrossingJob::NetworkCrossingJob(Network &&network) : Network_(std::move(network)) {}

std::expected<NetworkCrossingJob, std::string_view> NetworkCrossingJob::Begin(Network &&network) {
  NetworkCrossingJob job(std::move(network));
  if (job.Network_.CachedSweep_) {
    job.Statistics_ = *job.Network_.CachedSweep_;
    job.Stage_ = Stage::Done;
    return job;
  }
  const size_t points = job.Network_.Points_.size() / 2;
  if (job.Network_.Ways_.size() < 2 || points < 4) {
    job.Stage_ = Stage::Publish;
    return job;
  }

  auto phaseBegan = std::chrono::steady_clock::now();
  const auto phaseMs = [&phaseBegan] {
    const auto ended = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double, std::milli>(ended - phaseBegan).count();
    phaseBegan = ended;
    return elapsed;
  };
  job.LongitudeDeg_.assign(points, 0.0);
  job.Span_ = job.Network_.SpanOfPoints(job.LongitudeDeg_);
  job.Statistics_.SpanMs = phaseMs();
  const size_t segments = job.Network_.SegmentsOfWays(job.SegmentWay_, job.SegmentAt_);
  job.Statistics_.SegmentsMs = phaseMs();
  if (segments < 2) {
    job.Stage_ = Stage::Publish;
    return job;
  }
  auto grid = job.Network_.GridOver(
      {.Lon = job.LongitudeDeg_, .SegAt = job.SegmentAt_, .Segments = segments}, job.Span_);
  if (!grid) { return std::unexpected(grid.error()); }
  job.Grid_ = *grid;
  job.Statistics_.GridMs = phaseMs();

  job.Holds_.assign(job.Grid_.Cells + 1u, 0);
  job.Statistics_.FilingMs = phaseMs();
  return job;
}

NetworkCrossingJob::SquareCursor NetworkCrossingJob::SquaresOf(size_t segment) const {
  const size_t first = SegmentAt_[segment];
  const double lowLongitude = std::fmin(LongitudeDeg_[first], LongitudeDeg_[first + 1]);
  const double highLongitude = std::fmax(LongitudeDeg_[first], LongitudeDeg_[first + 1]);
  const double lowLatitude =
      std::fmin(Network_.Points_[2 * first], Network_.Points_[2 * first + 2]);
  const double highLatitude =
      std::fmax(Network_.Points_[2 * first], Network_.Points_[2 * first + 2]);
  const uint32_t from =
      Network::SquareIn(Grid_, Span_, {.LongitudeDeg = lowLongitude, .LatitudeDeg = lowLatitude});
  const uint32_t to =
      Network::SquareIn(Grid_, Span_, {.LongitudeDeg = highLongitude, .LatitudeDeg = highLatitude});
  const auto wide = static_cast<uint32_t>(Grid_.Wide);
  return {.FirstX = from % wide,
          .LastX = to % wide,
          .LastY = to / wide,
          .X = from % wide,
          .Y = from / wide,
          .Filed = {.Seg = static_cast<uint32_t>(segment),
                    .Way = SegmentWay_[segment],
                    .Ax = LongitudeDeg_[first],
                    .Ay = Network_.Points_[2 * first],
                    .Bx = LongitudeDeg_[first + 1],
                    .By = Network_.Points_[2 * first + 2]}};
}

void NetworkCrossingJob::AdvanceSquares(size_t itemsMost) {
  size_t visited = 0;
  while (NextSegment_ < SegmentAt_.size() && visited < itemsMost) {
    if (!SquareCursor_) {
      SquareCursor_ = SquaresOf(NextSegment_);
      ++visited;
      continue;
    }
    SquareCursor &cursor = *SquareCursor_;
    if (cursor.Y > cursor.LastY || cursor.X > cursor.LastX) {
      SquareCursor_.reset();
      ++NextSegment_;
      continue;
    }
    const size_t square = static_cast<size_t>(cursor.Y) * Grid_.Wide + cursor.X;
    if (Stage_ == Stage::CountCells) {
      ++Holds_[square + 1u];
    } else {
      FiledInCell_[Filled_[square]++] = cursor.Filed;
    }
    ++visited;
    if (cursor.X == cursor.LastX) {
      if (cursor.Y == cursor.LastY) {
        SquareCursor_.reset();
        ++NextSegment_;
      } else {
        cursor.X = cursor.FirstX;
        ++cursor.Y;
      }
    } else {
      ++cursor.X;
    }
  }
  if (NextSegment_ == SegmentAt_.size()) {
    NextSegment_ = 0;
    Stage_ = Stage_ == Stage::CountCells ? Stage::PrefixCells : Stage::CountPairs;
  }
}

void NetworkCrossingJob::PrefixCells(size_t itemsMost) {
  const size_t end = NextSetupCell_ + std::min(itemsMost, Grid_.Cells - NextSetupCell_);
  for (; NextSetupCell_ < end; ++NextSetupCell_) {
    Holds_[NextSetupCell_ + 1u] += Holds_[NextSetupCell_];
  }
  if (NextSetupCell_ == Grid_.Cells) { Stage_ = Stage::AllocateCells; }
}

void NetworkCrossingJob::AllocateCells() {
  Filled_.assign(Holds_.begin(), Holds_.end() - 1);
  FiledInCell_.assign(Holds_[Grid_.Cells], Network::Filed{});
  CellStarts_ = std::move(Holds_);
  NextSetupCell_ = 0;
  Stage_ = Stage::FillCells;
}

void NetworkCrossingJob::CountPairs(size_t itemsMost) {
  const size_t end = NextSetupCell_ + std::min(itemsMost, Grid_.Cells - NextSetupCell_);
  for (; NextSetupCell_ < end; ++NextSetupCell_) {
    const size_t held = CellStarts_[NextSetupCell_ + 1u] - CellStarts_[NextSetupCell_];
    Statistics_.FullestCell = std::max(held, Statistics_.FullestCell);
    Statistics_.CandidatePairs += held * (held - static_cast<size_t>(held > 0)) / 2;
  }
  if (NextSetupCell_ == Grid_.Cells) { Stage_ = Stage::TestPairs; }
}

void NetworkCrossingJob::TestPairs(size_t pairsMost) {
  size_t visited = 0;
  while (NextCell_ < Grid_.Cells && visited < pairsMost) {
    const uint32_t begins = CellStarts_[NextCell_];
    const uint32_t ends = CellStarts_[NextCell_ + 1];
    if (NextOne_ < begins || NextTwo_ <= NextOne_) {
      NextOne_ = begins;
      NextTwo_ = begins + 1u;
    }
    if (NextOne_ + 1u >= ends) {
      ++NextCell_;
      NextOne_ = 0;
      NextTwo_ = 0;
      continue;
    }
    const Network::Filed &ours = FiledInCell_[NextOne_];
    const Network::Filed &yours = FiledInCell_[NextTwo_];
    if (ours.Way != yours.Way) {
      const double lowX = std::fmin(ours.Ax, ours.Bx);
      const double highX = std::fmax(ours.Ax, ours.Bx);
      const double lowY = std::fmin(ours.Ay, ours.By);
      const double highY = std::fmax(ours.Ay, ours.By);
      if (highX < std::fmin(yours.Ax, yours.Bx) || std::fmax(yours.Ax, yours.Bx) < lowX ||
          highY < std::fmin(yours.Ay, yours.By) || std::fmax(yours.Ay, yours.By) < lowY) {
        ++Statistics_.PairsPruned;
      } else {
        ++Statistics_.PairsTested;
        const std::optional<LongitudeLatitude> met = Network::CrossingOf(ours, yours);
        if (met && Network::SquareIn(Grid_, Span_, *met) == NextCell_) {
          Found_.push_back(
              Network::Crossing{.OverWay = ours.Way,
                                .UnderWay = yours.Way,
                                .LatitudeDeg = met->LatitudeDeg,
                                .LongitudeDeg = met->LongitudeDeg,
                                .OverAt = static_cast<uint32_t>(SegmentAt_[ours.Seg]),
                                .UnderAt = static_cast<uint32_t>(SegmentAt_[yours.Seg])});
        }
      }
    }
    ++visited;
    ++NextTwo_;
    if (NextTwo_ >= ends) {
      ++NextOne_;
      NextTwo_ = NextOne_ + 1u;
    }
  }
  if (NextCell_ == Grid_.Cells) { Stage_ = Stage::Publish; }
}

void NetworkCrossingJob::Publish() {
  const auto began = std::chrono::steady_clock::now();
  Statistics_.Found = Found_.size();
  Network_.CachedCrossings_ = std::move(Found_);
  Network_.CachedSweep_ = Statistics_;
  Statistics_.CacheMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  Network_.CachedSweep_ = Statistics_;
  Stage_ = Stage::Done;
}

std::expected<bool, std::string_view> NetworkCrossingJob::Advance(size_t pairsMost) {
  if (pairsMost == 0) { return std::unexpected("network crossing pair budget is zero"); }
  constexpr size_t kSetupItemsPerAdvance = 512;
  const size_t setupItemsMost = std::min(pairsMost, kSetupItemsPerAdvance);
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  switch (Stage_) {
    case Stage::CountCells: AdvanceSquares(setupItemsMost); break;
    case Stage::PrefixCells: PrefixCells(setupItemsMost); break;
    case Stage::AllocateCells: AllocateCells(); break;
    case Stage::FillCells: AdvanceSquares(setupItemsMost); break;
    case Stage::CountPairs: CountPairs(setupItemsMost); break;
    case Stage::TestPairs: TestPairs(pairsMost); break;
    case Stage::Publish: Publish(); break;
    case Stage::Done: return true;
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  switch (before) {
    case Stage::CountCells:
    case Stage::PrefixCells:
    case Stage::AllocateCells:
    case Stage::FillCells:
    case Stage::CountPairs:
      Worst_.SetupMs = std::max(Worst_.SetupMs, elapsedMs);
      Statistics_.FilingMs += elapsedMs;
      break;
    case Stage::TestPairs: Worst_.TestMs = std::max(Worst_.TestMs, elapsedMs); break;
    case Stage::Publish: Worst_.PublishMs = std::max(Worst_.PublishMs, elapsedMs); break;
    case Stage::Done: break;
  }
  Statistics_.TestMs += before == Stage::TestPairs ? elapsedMs : 0.0;
  return Stage_ == Stage::Done;
}

std::expected<NetworkCrossingJob::Result, std::string_view> NetworkCrossingJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("network crossings are incomplete"); }
  return Result{.Graph = std::move(Network_), .Statistics = Statistics_};
}

}
