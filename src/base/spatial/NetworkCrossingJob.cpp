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

  std::vector<uint32_t> holds(job.Grid_.Cells + 1u, 0);
  const auto squareOf = [&job](double longitudeDeg, double latitudeDeg) {
    return Network::SquareIn(
        job.Grid_, job.Span_, {.LongitudeDeg = longitudeDeg, .LatitudeDeg = latitudeDeg});
  };
  const auto overSquares = [&job, &squareOf](size_t segment, auto &&visit) {
    const size_t first = job.SegmentAt_[segment];
    const double lowLongitude = std::fmin(job.LongitudeDeg_[first], job.LongitudeDeg_[first + 1]);
    const double highLongitude = std::fmax(job.LongitudeDeg_[first], job.LongitudeDeg_[first + 1]);
    const double lowLatitude =
        std::fmin(job.Network_.Points_[2 * first], job.Network_.Points_[2 * first + 2]);
    const double highLatitude =
        std::fmax(job.Network_.Points_[2 * first], job.Network_.Points_[2 * first + 2]);
    const uint32_t from = squareOf(lowLongitude, lowLatitude);
    const uint32_t to = squareOf(highLongitude, highLatitude);
    const auto wide = static_cast<uint32_t>(job.Grid_.Wide);
    for (uint32_t y = from / wide; y <= to / wide; ++y) {
      for (uint32_t x = from % wide; x <= to % wide; ++x) {
        visit(static_cast<uint32_t>(y * job.Grid_.Wide + x));
      }
    }
  };

  for (size_t segment = 0; segment < segments; ++segment) {
    overSquares(segment, [&holds](uint32_t square) { ++holds[static_cast<size_t>(square) + 1u]; });
  }
  for (size_t cell = 0; cell < job.Grid_.Cells; ++cell) { holds[cell + 1u] += holds[cell]; }
  std::vector<uint32_t> filled(holds.begin(), holds.end() - 1);
  job.FiledInCell_.assign(holds[job.Grid_.Cells], Network::Filed{});
  for (size_t segment = 0; segment < segments; ++segment) {
    const size_t first = job.SegmentAt_[segment];
    const Network::Filed filed{.Seg = static_cast<uint32_t>(segment),
                               .Way = job.SegmentWay_[segment],
                               .Ax = job.LongitudeDeg_[first],
                               .Ay = job.Network_.Points_[2 * first],
                               .Bx = job.LongitudeDeg_[first + 1],
                               .By = job.Network_.Points_[2 * first + 2]};
    overSquares(segment, [&job, &filled, filed](uint32_t square) {
      job.FiledInCell_[filled[square]++] = filed;
    });
  }
  for (size_t cell = 0; cell < job.Grid_.Cells; ++cell) {
    const size_t held = holds[cell + 1u] - holds[cell];
    job.Statistics_.FullestCell = std::max(held, job.Statistics_.FullestCell);
    job.Statistics_.CandidatePairs += held * (held - static_cast<size_t>(held > 0)) / 2;
  }
  job.CellStarts_ = std::move(holds);
  job.Statistics_.FilingMs = phaseMs();
  return job;
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
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  switch (Stage_) {
    case Stage::TestPairs: TestPairs(pairsMost); break;
    case Stage::Publish: Publish(); break;
    case Stage::Done: return true;
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  switch (before) {
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
