#include <chrono>
#include <cstdio>
#include "FlatMap.h"
#include "math/Units.h"
#include "GroundYield.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <ratio>
#include <span>
#include <utility>
#include <vector>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace outshine {

constexpr uint32_t kNoVertex = 0xffffffffu;

namespace {

constexpr uint64_t kGoldenWord = 0x9e3779b97f4a7c15ULL;
constexpr uint64_t kCellBias = 0x2000000000ULL;
constexpr double kNoNearestYet = 1.0e29;
constexpr unsigned kNorthingShift = 24u;

constexpr int kMostPasses = 6;
constexpr double kWeldM = 0.01;
constexpr double kMovedM = 0.01;
constexpr double kBatterRise = 1.0 / 1.5;
constexpr double kCoarsestM = 40.0;
constexpr double kEdgeGrade = 0.35;
constexpr double kRiseM = 1.0;
constexpr double kNearM = 16.0;
constexpr int kSewPasses = 6;
constexpr uint32_t kUnclaimed = 0xffffffffu;
constexpr double kSewCellM = 16.0;
constexpr int kCutPasses = 5;
constexpr double kOffEndM = 0.02;
constexpr int64_t kMostSpreadCells = 256;
constexpr size_t kMostDenseCells = 1u << 22u;

bool LowestFirst(EastSouth from, EastSouth to) {
  return from.EastM < to.EastM || (from.EastM == to.EastM && from.SouthM < to.SouthM);
}

uint64_t EdgeKey(EastSouth from, EastSouth to) {
  const auto one = static_cast<uint64_t>(static_cast<int64_t>(std::llround(from.EastM / kWeldM)) +
                                         0x2000000000LL);
  const auto two = static_cast<uint64_t>(static_cast<int64_t>(std::llround(from.SouthM / kWeldM)) +
                                         0x2000000000LL);
  const auto three =
      static_cast<uint64_t>(static_cast<int64_t>(std::llround(to.EastM / kWeldM)) + 0x2000000000LL);
  const auto four = static_cast<uint64_t>(static_cast<int64_t>(std::llround(to.SouthM / kWeldM)) +
                                          0x2000000000LL);
  const uint64_t here = (one << 24U) ^ two;
  const uint64_t there = (three << 24U) ^ four;
  return here < there ? (here * kGoldenWord) ^ there : (there * kGoldenWord) ^ here;
}

uint64_t PlaceKey(EastSouth at) {
  const auto atE = static_cast<int64_t>(std::llround(at.EastM / kWeldM));
  const auto atS = static_cast<int64_t>(std::llround(at.SouthM / kWeldM));
  return (static_cast<uint64_t>(atE + kCellBias) << kNorthingShift) ^
         static_cast<uint64_t>(atS + 0x2000000000LL);
}

constexpr double kBucketM = 32.0;

class CellGrid {
public:
  static constexpr int64_t kBias = 0x20000000LL;
  static constexpr int64_t kSpreadsAnywhere = std::numeric_limits<int64_t>::max();

  struct Sized {
    double CellM = 1.0;
    int64_t MostCells = kSpreadsAnywhere;
  };

  explicit CellGrid(Sized over) : CellM_(over.CellM), MostCells_(over.MostCells) {}

  [[nodiscard]] int64_t CellOf(double metres) const {
    return static_cast<int64_t>(std::floor(metres / CellM_));
  }

  static constexpr uint64_t KeyOf(int64_t cellE, int64_t cellS) {
    return (static_cast<uint64_t>(cellE + kBias) << 32U) | static_cast<uint64_t>(cellS + kBias);
  }

  [[nodiscard]] uint64_t KeyAt(EastSouth at) const {
    return KeyOf(CellOf(at.EastM), CellOf(at.SouthM));
  }

  void Expects(size_t entries) {
    Held_.reserve(entries);
    What_.reserve(entries);
  }

  void Spread(EastSouth low, EastSouth high, uint32_t what) {
    const int64_t fromE = CellOf(low.EastM);
    const int64_t toE = CellOf(high.EastM);
    const int64_t fromS = CellOf(low.SouthM);
    const int64_t toS = CellOf(high.SouthM);
    if ((toE - fromE + 1) * (toS - fromS + 1) > MostCells_) { return; }
    for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
      for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
        Held_.push_back(KeyOf(cellE, cellS));
        What_.push_back(what);
      }
    }
    Settled_ = false;
  }

  void Settles() {
    Settled_ = true;
    First_.clear();
    Seats_.clear();
    Wide_ = 0;
    if (Held_.empty()) { return; }
    LowE_ = std::numeric_limits<int64_t>::max();
    LowS_ = std::numeric_limits<int64_t>::max();
    int64_t highE = std::numeric_limits<int64_t>::min();
    int64_t highS = std::numeric_limits<int64_t>::min();
    for (const uint64_t key : Held_) {
      const auto cellE = static_cast<int64_t>(key >> 32U) - kBias;
      const auto cellS = static_cast<int64_t>(key & 0xffffffffU) - kBias;
      LowE_ = std::min(LowE_, cellE);
      LowS_ = std::min(LowS_, cellS);
      highE = std::max(highE, cellE);
      highS = std::max(highS, cellS);
    }
    const int64_t wide = highE - LowE_ + 1;
    const size_t cells = static_cast<size_t>(wide) * static_cast<size_t>(highS - LowS_ + 1);
    if (cells > kMostDenseCells) { return; }
    Wide_ = wide;
    First_.assign(cells + 1u, 0u);
    for (const uint64_t key : Held_) { ++First_[SeatOf(key) + 1u]; }
    for (size_t cell = 1; cell < First_.size(); ++cell) { First_[cell] += First_[cell - 1u]; }
    Seats_.resize(Held_.size());
    std::vector<uint32_t> at(First_.begin(), First_.end() - 1);
    for (size_t entry = 0; entry < Held_.size(); ++entry) {
      Seats_[at[SeatOf(Held_[entry])]++] = What_[entry];
    }
  }

  [[nodiscard]] std::span<const uint32_t> At(EastSouth where) const {
    if (Wide_ == 0) { return {}; }
    const int64_t cellE = CellOf(where.EastM) - LowE_;
    const int64_t cellS = CellOf(where.SouthM) - LowS_;
    if (cellE < 0 || cellS < 0 || cellE >= Wide_) { return {}; }
    const size_t cell =
        static_cast<size_t>(cellS) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
    if (cell + 1u >= First_.size()) { return {}; }
    return {Seats_.data() + First_[cell], First_[cell + 1u] - First_[cell]};
  }

  [[nodiscard]] bool Empty() const { return Held_.empty(); }

private:
  [[nodiscard]] size_t SeatOf(uint64_t key) const {
    const int64_t cellE = static_cast<int64_t>(key >> 32U) - kBias - LowE_;
    const int64_t cellS = static_cast<int64_t>(key & 0xffffffffU) - kBias - LowS_;
    return static_cast<size_t>(cellS) * static_cast<size_t>(Wide_) + static_cast<size_t>(cellE);
  }

  double CellM_;
  int64_t MostCells_;
  bool Settled_ = false;
  int64_t LowE_ = 0;
  int64_t LowS_ = 0;
  int64_t Wide_ = 0;
  std::vector<uint64_t> Held_;
  std::vector<uint32_t> What_;
  std::vector<uint32_t> First_;
  std::vector<uint32_t> Seats_;
};

CellGrid BucketOver(std::span<const Yields> these) {
  CellGrid out({.CellM = kBucketM});
  for (size_t at = 0; at < these.size(); ++at) {
    const Yields &one = these[at];
    out.Spread({.EastM = one.LowE - one.ApronM, .SouthM = one.LowS - one.ApronM},
               {.EastM = one.HighE + one.ApronM, .SouthM = one.HighS + one.ApronM},
               static_cast<uint32_t>(at));
  }
  out.Settles();
  return out;
}

struct Rows {
  std::vector<float> &Held;
  uint32_t Wide = 0;

  [[nodiscard]] bool Empty() const { return Held.empty(); }

  [[nodiscard]] double At(uint32_t vertex, uint32_t axis) const {
    return static_cast<double>(Held[static_cast<size_t>(vertex) * Wide + axis]);
  }

  void Push(double value) { Held.push_back(static_cast<float>(value)); }
};

struct Edge {
  uint32_t From = 0;
  uint32_t To = 0;
};

struct Corners {
  uint32_t A = 0;
  uint32_t B = 0;
  uint32_t C = 0;
};

struct Weights {
  double One = 0.0;
  double Two = 0.0;
};

EastSouth PlaceOf(const float *positionM, uint32_t vertex) {
  const size_t three = static_cast<size_t>(vertex) * 3u;
  return {.EastM = static_cast<double>(positionM[three]),
          .SouthM = static_cast<double>(positionM[three + 2u])};
}

std::optional<Weights> WeighOver(EastSouth a, EastSouth b, EastSouth c, EastSouth at) {
  const double twice =
      (b.SouthM - c.SouthM) * (a.EastM - c.EastM) + (c.EastM - b.EastM) * (a.SouthM - c.SouthM);
  if (std::fabs(twice) < kLeastTurnRad) { return std::nullopt; }
  return Weights{.One = ((b.SouthM - c.SouthM) * (at.EastM - c.EastM) +
                         (c.EastM - b.EastM) * (at.SouthM - c.SouthM)) /
                        twice,
                 .Two = ((c.SouthM - a.SouthM) * (at.EastM - c.EastM) +
                         (a.EastM - c.EastM) * (at.SouthM - c.SouthM)) /
                        twice};
}

bool HoldsPlace(Weights by) {
  const double three = 1.0 - by.One - by.Two;
  return by.One >= -kLeastTurnRad && by.Two >= -kLeastTurnRad && three >= -kLeastTurnRad;
}

class Attributes {
public:
  explicit Attributes(const GroundMesh &mesh) : Mesh_(mesh) {}

  [[nodiscard]] uint32_t Inside(Corners face, EastSouth at) const {
    const float *held = Mesh_.PositionM->data();
    const Weights over =
        WeighOver(PlaceOf(held, face.A), PlaceOf(held, face.B), PlaceOf(held, face.C), at)
            .value_or(Weights{.One = 1.0 / 3.0, .Two = 1.0 / 3.0});
    Blend({.Held = *Mesh_.PositionM, .Wide = 3u}, face, over);
    Blend({.Held = *Mesh_.NormalM, .Wide = 3u}, face, over);
    if (Mesh_.ColourRgba != nullptr) { Blend({.Held = *Mesh_.ColourRgba, .Wide = 4u}, face, over); }
    if (Mesh_.Uv != nullptr) { Blend({.Held = *Mesh_.Uv, .Wide = 2u}, face, over); }
    const auto point = static_cast<uint32_t>(Mesh_.PositionM->size() / 3u) - 1u;
    (*Mesh_.PositionM)[static_cast<size_t>(point) * 3u] = static_cast<float>(at.EastM);
    (*Mesh_.PositionM)[static_cast<size_t>(point) * 3u + 2u] = static_cast<float>(at.SouthM);
    return point;
  }

  [[nodiscard]] uint32_t Along(Edge over, double part) const {
    Lerp({.Held = *Mesh_.PositionM, .Wide = 3u}, over, part);
    Lerp({.Held = *Mesh_.NormalM, .Wide = 3u}, over, part);
    if (Mesh_.ColourRgba != nullptr) { Lerp({.Held = *Mesh_.ColourRgba, .Wide = 4u}, over, part); }
    if (Mesh_.Uv != nullptr) { Lerp({.Held = *Mesh_.Uv, .Wide = 2u}, over, part); }
    return static_cast<uint32_t>(Mesh_.PositionM->size() / 3u) - 1u;
  }

  [[nodiscard]] uint32_t Midpoint(Edge over) const { return Along(over, 0.5); }

private:
  static void Blend(Rows over, Corners face, Weights by) {
    if (over.Empty()) { return; }
    const double three = 1.0 - by.One - by.Two;
    for (uint32_t axis = 0; axis < over.Wide; ++axis) {
      over.Push(by.One * over.At(face.A, axis) + by.Two * over.At(face.B, axis) +
                three * over.At(face.C, axis));
    }
  }

  static void Lerp(Rows over, Edge along, double part) {
    if (over.Empty()) { return; }
    for (uint32_t axis = 0; axis < over.Wide; ++axis) {
      const double was = over.At(along.From, axis);
      over.Push(was + (over.At(along.To, axis) - was) * part);
    }
  }

  const GroundMesh &Mesh_;
};

double LongestEdgeM(const float *positionM, std::span<const uint32_t, 3> face) {
  double most = 0.0;
  for (int at = 0; at < 3; ++at) {
    const uint32_t a = face[at];
    const uint32_t b = face[(at + 1) % 3];
    const double runE = static_cast<double>(positionM[static_cast<size_t>(a) * 3u]) -
                        static_cast<double>(positionM[static_cast<size_t>(b) * 3u]);
    const double runS = static_cast<double>(positionM[static_cast<size_t>(a) * 3u + 2u]) -
                        static_cast<double>(positionM[static_cast<size_t>(b) * 3u + 2u]);
    most = std::max(most, std::sqrt(runE * runE + runS * runS));
  }
  return most;
}

double AwayFrom(const Yields &one, EastSouth at) {
  const double offE = std::max({one.LowE - at.EastM, 0.0, at.EastM - one.HighE});
  const double offS = std::max({one.LowS - at.SouthM, 0.0, at.SouthM - one.HighS});
  return std::sqrt(offE * offE + offS * offS);
}

double WantedEdgeM(const CellGrid &buckets,
                   std::span<const Yields> these,
                   double finestM,
                   const float *positionM,
                   std::span<const uint32_t, 3> face) {
  double centreE = 0.0;
  double centreS = 0.0;
  for (int at = 0; at < 3; ++at) {
    centreE += static_cast<double>(positionM[static_cast<size_t>(face[at]) * 3u]);
    centreS += static_cast<double>(positionM[static_cast<size_t>(face[at]) * 3u + 2u]);
  }
  centreE /= 3.0;
  centreS /= 3.0;
  const EastSouth centre = {.EastM = centreE, .SouthM = centreS};
  double nearest = kBeyondAnyCoordinate;
  for (const uint32_t which : buckets.At(centre)) {
    nearest = std::min(nearest, AwayFrom(these[which], centre));
  }
  if (nearest > kNoNearestYet) { return 0.0; }
  double wanted = std::clamp(finestM + kEdgeGrade * nearest, finestM, kCoarsestM);
  if (nearest > kNearM) { return wanted; }
  double lowest = kBeyondAnyCoordinate;
  double highest = -kBeyondAnyCoordinate;
  for (int at = 0; at < 3; ++at) {
    const auto upM = static_cast<double>(positionM[static_cast<size_t>(face[at]) * 3u + 1u]);
    lowest = std::min(lowest, upM);
    highest = std::max(highest, upM);
  }
  const double rises = highest - lowest;
  if (rises > kRiseM) {
    const double longest = LongestEdgeM(positionM, face);
    wanted = std::min(wanted, longest * kRiseM / rises);
  }
  return wanted;
}

void LayCutFace(const std::array<uint32_t, 3> &face,
                const std::array<uint32_t, 3> &cut,
                std::vector<uint32_t> &next) {
  const auto lay = [&next](uint32_t a, uint32_t b, uint32_t c) {
    next.push_back(a);
    next.push_back(b);
    next.push_back(c);
  };
  const int cuts =
      (cut[0] != kNoVertex ? 1 : 0) + (cut[1] != kNoVertex ? 1 : 0) + (cut[2] != kNoVertex ? 1 : 0);
  if (cuts == 0) {
    lay(face[0], face[1], face[2]);
    return;
  }
  if (cuts == 3) {
    lay(face[0], cut[0], cut[2]);
    lay(cut[0], face[1], cut[1]);
    lay(cut[2], cut[1], face[2]);
    lay(cut[0], cut[1], cut[2]);
    return;
  }
  if (cuts == 1) {
    for (int edge = 0; edge < 3; ++edge) {
      if (cut[edge] == kNoVertex) { continue; }
      lay(face[edge], cut[edge], face[(edge + 2) % 3]);
      lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
      return;
    }
    return;
  }
  for (int edge = 0; edge < 3; ++edge) {
    if (cut[edge] != kNoVertex) { continue; }
    lay(face[edge], face[(edge + 1) % 3], cut[(edge + 1) % 3]);
    lay(face[edge], cut[(edge + 1) % 3], cut[(edge + 2) % 3]);
    lay(cut[(edge + 2) % 3], cut[(edge + 1) % 3], face[(edge + 2) % 3]);
    return;
  }
}

uint64_t MiddleKey(const float *positionM, Edge along) {
  const EastSouth from = PlaceOf(positionM, along.From);
  const EastSouth to = PlaceOf(positionM, along.To);
  return PlaceKey(
      {.EastM = 0.5 * (from.EastM + to.EastM), .SouthM = 0.5 * (from.SouthM + to.SouthM)});
}

void Refine(std::span<const Yields> these, double finestM, GroundMesh &mesh, Yielded &told) {
  const CellGrid buckets = BucketOver(these);
  const Attributes lerp(mesh);
  std::vector<uint32_t> &index = *mesh.Index;
  std::vector<uint32_t> next;
  FlatMap<uint32_t> split;

  for (int pass = 0; pass < kMostPasses; ++pass) {
    const float *positionM = mesh.PositionM->data();
    split.Clear();
    bool any = false;
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const double wanted = WantedEdgeM(buckets, these, finestM, positionM, face);
      if (!(wanted > 0.0)) { continue; }
      if (LongestEdgeM(positionM, face) <= wanted) { continue; }
      any = true;
      for (int edge = 0; edge < 3; ++edge) {
        (void)split.Emplace(MiddleKey(positionM, {.From = face[edge], .To = face[(edge + 1) % 3]}),
                            kNoVertex);
      }
    }
    if (!any) { break; }
    told.Passes = static_cast<size_t>(pass) + 1u;

    const auto midpoint = [&](uint32_t a, uint32_t b) {
      uint32_t *const found = split.Find(MiddleKey(mesh.PositionM->data(), {.From = a, .To = b}));
      if (found == nullptr) { return kNoVertex; }
      if (*found == kNoVertex) {
        *found = lerp.Midpoint({.From = a, .To = b});
        ++told.VerticesAdded;
      }
      return *found;
    };

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const std::array<uint32_t, 3> cut = {
          {midpoint(face[0], face[1]), midpoint(face[1], face[2]), midpoint(face[2], face[0])}};
      LayCutFace(face, cut, next);
    }
    told.TrianglesAdded += (next.size() - index.size()) / 3u;
    index.swap(next);
  }
}

struct Segment {
  EastSouth From;
  EastSouth To;
};

bool MeetsAt(Segment mine_, Segment yours_, double *part) {
  const double aE = mine_.From.EastM;
  const double aS = mine_.From.SouthM;
  const double cE = yours_.From.EastM;
  const double cS = yours_.From.SouthM;
  const double runE = mine_.To.EastM - aE;
  const double runS = mine_.To.SouthM - aS;
  const double overE = yours_.To.EastM - cE;
  const double overS = yours_.To.SouthM - cS;
  const double under = runE * overS - runS * overE;
  if (std::fabs(under) < kParallelCross) { return false; }
  const double mine = ((cE - aE) * overS - (cS - aS) * overE) / under;
  const double yours = ((cE - aE) * runS - (cS - aS) * runE) / under;
  if (mine <= kOffEndM || mine >= 1.0 - kOffEndM || yours < 0.0 || yours > 1.0) { return false; }
  *part = mine;
  return true;
}

struct Seaming {
  std::vector<double> Runs;
  CellGrid Over{{.CellM = kSewCellM, .MostCells = kMostSpreadCells}};

  [[nodiscard]] Segment At(uint32_t which) const {
    const size_t four = static_cast<size_t>(which) * 4u;
    return {.From = {.EastM = Runs[four], .SouthM = Runs[four + 1u]},
            .To = {.EastM = Runs[four + 2u], .SouthM = Runs[four + 3u]}};
  }
};

Seaming SeamsOver(std::span<const Yields> these) {
  Seaming out;
  for (const Yields &one : these) {
    const size_t corners = one.SeamEastSouthM.size() / 2u;
    for (size_t at = 0, last = corners > 0 ? corners - 1u : 0; at < corners; last = at++) {
      const double aE = one.SeamEastSouthM[last * 2u];
      const double aS = one.SeamEastSouthM[last * 2u + 1u];
      const double bE = one.SeamEastSouthM[at * 2u];
      const double bS = one.SeamEastSouthM[at * 2u + 1u];
      const auto which = static_cast<uint32_t>(out.Runs.size() / 4u);
      out.Runs.insert(out.Runs.end(), {aE, aS, bE, bS});
      out.Over.Spread({.EastM = std::min(aE, bE), .SouthM = std::min(aS, bS)},
                      {.EastM = std::max(aE, bE), .SouthM = std::max(aS, bS)},
                      which);
    }
  }
  out.Over.Settles();
  return out;
}

void MarkCutEdges(std::span<const uint32_t> index,
                  const float *positionM,
                  const Seaming &seaming,
                  FlatMap<std::pair<uint32_t, double>> &split) {
  for (size_t at = 0; at + 2 < index.size(); at += 3) {
    for (int edge = 0; edge < 3; ++edge) {
      const EastSouth from = PlaceOf(positionM, index[at + static_cast<size_t>(edge)]);
      const EastSouth to = PlaceOf(positionM, index[at + static_cast<size_t>((edge + 1) % 3)]);
      const uint64_t key = EdgeKey(from, to);
      if (split.Holds(key)) { continue; }
      double part = 0.0;
      bool met = false;
      for (const uint32_t which : seaming.Over.At({.EastM = 0.5 * (from.EastM + to.EastM),
                                                   .SouthM = 0.5 * (from.SouthM + to.SouthM)})) {
        if (MeetsAt({.From = from, .To = to}, seaming.At(which), &part)) {
          met = true;
          break;
        }
      }
      if (!met) { continue; }
      (void)split.Emplace(
          key, std::pair<uint32_t, double>{kNoVertex, LowestFirst(from, to) ? part : 1.0 - part});
    }
  }
}

void Cut(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  const Seaming seaming = SeamsOver(these);
  if (seaming.Runs.empty()) { return; }

  std::vector<uint32_t> &index = *mesh.Index;
  const Attributes lerp(mesh);
  std::vector<uint32_t> next;
  FlatMap<std::pair<uint32_t, double>> split;

  for (int pass = 0; pass < kCutPasses; ++pass) {
    split.Clear();
    MarkCutEdges(index, mesh.PositionM->data(), seaming, split);
    if (split.Empty()) { break; }
    told.Passes = std::max(told.Passes, static_cast<size_t>(pass) + 1u);

    const auto cutOf = [&](uint32_t a, uint32_t b) {
      const float *held = mesh.PositionM->data();
      const EastSouth from = PlaceOf(held, a);
      const EastSouth to = PlaceOf(held, b);
      auto *const found = split.Find(EdgeKey(from, to));
      if (found == nullptr) { return kNoVertex; }
      if (found->first == kNoVertex) {
        const double part = LowestFirst(from, to) ? found->second : 1.0 - found->second;
        found->first = lerp.Along({.From = a, .To = b}, part);
        ++told.VerticesAdded;
      }
      return found->first;
    };

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const std::array<uint32_t, 3> cut = {
          {cutOf(face[0], face[1]), cutOf(face[1], face[2]), cutOf(face[2], face[0])}};
      LayCutFace(face, cut, next);
    }
    told.TrianglesAdded += (next.size() - index.size()) / 3u;
    index.swap(next);
  }
}

CellGrid FacesOver(std::span<const uint32_t> index, const float *positionM) {
  CellGrid out({.CellM = kSewCellM, .MostCells = kMostSpreadCells});
  out.Expects(index.size() / 3u);
  for (size_t at = 0; at + 2 < index.size(); at += 3) {
    EastSouth low = {.EastM = kBeyondAnyCoordinate, .SouthM = kBeyondAnyCoordinate};
    EastSouth high = {.EastM = -kBeyondAnyCoordinate, .SouthM = -kBeyondAnyCoordinate};
    for (int corner = 0; corner < 3; ++corner) {
      const EastSouth one = PlaceOf(positionM, index[at + static_cast<size_t>(corner)]);
      low.EastM = std::min(low.EastM, one.EastM);
      low.SouthM = std::min(low.SouthM, one.SouthM);
      high.EastM = std::max(high.EastM, one.EastM);
      high.SouthM = std::max(high.SouthM, one.SouthM);
    }
    out.Spread(low, high, static_cast<uint32_t>(at));
  }
  out.Settles();
  return out;
}

struct Standing {
  std::span<const uint32_t> Index;
  const float *PositionM = nullptr;
  const CellGrid &Over;
};

size_t ClaimFaces(Standing mesh,
                  std::span<const double> seams,
                  std::span<uint8_t> sewn,
                  std::vector<uint32_t> &claimed) {
  claimed.assign(mesh.Index.size() / 3u, kUnclaimed);
  size_t claims = 0;
  for (uint32_t which = 0; which < static_cast<uint32_t>(sewn.size()); ++which) {
    if (sewn[which] != 0u) { continue; }
    const EastSouth at = {.EastM = seams[static_cast<size_t>(which) * 2u],
                          .SouthM = seams[static_cast<size_t>(which) * 2u + 1u]};
    bool held = false;
    for (const uint32_t face : mesh.Over.At(at)) {
      const std::optional<Weights> by = WeighOver(PlaceOf(mesh.PositionM, mesh.Index[face]),
                                                  PlaceOf(mesh.PositionM, mesh.Index[face + 1u]),
                                                  PlaceOf(mesh.PositionM, mesh.Index[face + 2u]),
                                                  at);
      if (!by.has_value() || !HoldsPlace(*by)) { continue; }
      held = true;
      uint32_t &seat = claimed[face / 3u];
      if (seat == kUnclaimed) {
        seat = which;
        ++claims;
      }
    }
    if (held) { sewn[which] = 1u; }
  }
  return claims;
}

void Sew(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  std::vector<double> seams;
  for (const Yields &one : these) {
    seams.insert(seams.end(), one.SeamEastSouthM.begin(), one.SeamEastSouthM.end());
  }
  if (seams.empty()) { return; }

  std::vector<uint32_t> &index = *mesh.Index;
  std::vector<float> &positionM = *mesh.PositionM;
  std::vector<uint8_t> sewn(seams.size() / 2u, 0u);
  std::vector<uint32_t> next;
  std::vector<uint32_t> claimed;
  std::unordered_map<uint64_t, uint32_t> made;
  const Attributes lerp(mesh);

  for (int pass = 0; pass < kSewPasses; ++pass) {
    const CellGrid facesAt = FacesOver(index, positionM.data());

    made.clear();
    const size_t claims = ClaimFaces(
        {.Index = index, .PositionM = positionM.data(), .Over = facesAt}, seams, sewn, claimed);
    if (claims == 0) { break; }
    told.Passes = std::max(told.Passes, static_cast<size_t>(pass) + 1u);

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const uint32_t which = claimed[at / 3u];
      if (which == kUnclaimed) {
        next.push_back(index[at]);
        next.push_back(index[at + 1u]);
        next.push_back(index[at + 2u]);
        continue;
      }
      const double eastM = seams[static_cast<size_t>(which) * 2u];
      const double southM = seams[static_cast<size_t>(which) * 2u + 1u];
      const uint64_t key = PlaceKey({.EastM = eastM, .SouthM = southM});
      auto stood = made.find(key);
      if (stood == made.end()) {
        stood = made.emplace(key,
                             lerp.Inside({.A = index[at], .B = index[at + 1u], .C = index[at + 2u]},
                                         {.EastM = eastM, .SouthM = southM}))
                    .first;
        ++told.VerticesAdded;
      }
      const uint32_t point = stood->second;
      for (int corner = 0; corner < 3; ++corner) {
        const uint32_t a = index[at + static_cast<size_t>(corner)];
        const uint32_t b = index[at + static_cast<size_t>((corner + 1) % 3)];
        if (a == point || b == point) { continue; }
        next.push_back(a);
        next.push_back(b);
        next.push_back(point);
      }
    }
    told.TrianglesAdded += (next.size() - index.size()) / 3u;
    index.swap(next);
  }
}

double OutsideRingM(const Yields &held, EastSouth at) {
  const size_t corners = held.RingEastSouthM.size() / 2u;
  if (corners < 3) { return kBeyondAnyCoordinate; }
  bool inside = false;
  double nearest = kBeyondAnyCoordinate;
  for (size_t edge = 0, last = corners - 1u; edge < corners; last = edge++) {
    const double aE = held.RingEastSouthM[edge * 2u];
    const double aS = held.RingEastSouthM[edge * 2u + 1u];
    const double bE = held.RingEastSouthM[last * 2u];
    const double bS = held.RingEastSouthM[last * 2u + 1u];
    if ((aS > at.SouthM) != (bS > at.SouthM) &&
        at.EastM < (bE - aE) * (at.SouthM - aS) / (bS - aS) + aE) {
      inside = !inside;
    }
    const double runE = bE - aE;
    const double runS = bS - aS;
    const double runM = runE * runE + runS * runS;
    const double part =
        runM > kLeastTurnRad
            ? std::clamp(((at.EastM - aE) * runE + (at.SouthM - aS) * runS) / runM, 0.0, 1.0)
            : 0.0;
    const double offE = at.EastM - (aE + runE * part);
    const double offS = at.SouthM - (aS + runS * part);
    nearest = std::min(nearest, std::sqrt(offE * offE + offS * offS));
  }
  return inside ? 0.0 : nearest;
}

void RelightMoved(const GroundMesh &mesh, std::span<const uint8_t> moved) {
  std::vector<float> &normalM = *mesh.NormalM;
  const std::vector<uint32_t> &index = *mesh.Index;
  const std::vector<float> &positionM = *mesh.PositionM;
  for (size_t at = 0; at < moved.size(); ++at) {
    if (moved[at] == 0u) { continue; }
    normalM[at * 3u] = 0.0f;
    normalM[at * 3u + 1u] = 0.0f;
    normalM[at * 3u + 2u] = 0.0f;
  }
  for (size_t at = 0; at + 2 < index.size(); at += 3) {
    const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
    if (moved[face[0]] == 0u && moved[face[1]] == 0u && moved[face[2]] == 0u) { continue; }
    Vec3 edgeA = {{}};
    Vec3 edgeB = {{}};
    for (int axis = 0; axis < 3; ++axis) {
      edgeA[axis] =
          static_cast<double>(
              positionM[static_cast<size_t>(face[1]) * 3u + static_cast<uint32_t>(axis)]) -
          static_cast<double>(
              positionM[static_cast<size_t>(face[0]) * 3u + static_cast<uint32_t>(axis)]);
      edgeB[axis] =
          static_cast<double>(
              positionM[static_cast<size_t>(face[2]) * 3u + static_cast<uint32_t>(axis)]) -
          static_cast<double>(
              positionM[static_cast<size_t>(face[0]) * 3u + static_cast<uint32_t>(axis)]);
    }
    const Vec3 up = {{edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1],
                      edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2],
                      edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0]}};
    for (const uint32_t one : face) {
      if (moved[one] == 0u) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        normalM[static_cast<size_t>(one) * 3u + static_cast<uint32_t>(axis)] +=
            static_cast<float>(up[axis]);
      }
    }
  }
  for (size_t at = 0; at < moved.size(); ++at) {
    if (moved[at] == 0u) { continue; }
    const double len = std::sqrt(
        static_cast<double>(normalM[at * 3u]) * static_cast<double>(normalM[at * 3u]) +
        static_cast<double>(normalM[at * 3u + 1u]) * static_cast<double>(normalM[at * 3u + 1u]) +
        static_cast<double>(normalM[at * 3u + 2u]) * static_cast<double>(normalM[at * 3u + 2u]));
    if (!(len > kLeastTurnRad)) {
      normalM[at * 3u + 1u] = 1.0f;
      continue;
    }
    for (int axis = 0; axis < 3; ++axis) {
      normalM[at * 3u + static_cast<uint32_t>(axis)] = static_cast<float>(
          static_cast<double>(normalM[at * 3u + static_cast<uint32_t>(axis)]) / len);
    }
  }
}

struct Pressing {
  double WantedM = 0.0;
  bool Moves = false;
};

Pressing PressesAt(std::span<const Yields> these,
                   std::span<const uint32_t> over,
                   EastSouth at,
                   double wasM) {
  double lowest = wasM;
  double highest = wasM;
  double roofM = kBeyondAnyCoordinate;
  for (const uint32_t which : over) {
    const Yields &held = these[which];
    if (at.EastM < held.LowE - held.ApronM || at.EastM > held.HighE + held.ApronM ||
        at.SouthM < held.LowS - held.ApronM || at.SouthM > held.HighS + held.ApronM) {
      continue;
    }
    const double out = OutsideRingM(held, at);
    if (out > held.ApronM) { continue; }
    const double onRoad = held.WantsAt(at);
    const double cutAt = onRoad + out * kBatterRise;
    lowest = std::min(lowest, cutAt);
    roofM = std::min(roofM, cutAt);
    if (held.Fills) { highest = std::max(highest, onRoad - out * kBatterRise); }
  }
  if (lowest < wasM) { return {.WantedM = lowest, .Moves = true}; }
  if (highest > wasM) {
    const double wanted = std::min(highest, roofM);
    return {.WantedM = wanted, .Moves = wanted > wasM};
  }
  return {};
}

void Press(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  const CellGrid buckets = BucketOver(these);
  std::vector<float> &positionM = *mesh.PositionM;
  std::vector<uint8_t> moved(positionM.size() / 3u, 0u);
  for (size_t one = 0; one + 2 < positionM.size(); one += 3) {
    const auto eastM = static_cast<double>(positionM[one]);
    const auto southM = static_cast<double>(positionM[one + 2u]);
    const auto was = static_cast<double>(positionM[one + 1u]);
    const EastSouth at = {.EastM = eastM, .SouthM = southM};
    const Pressing under = PressesAt(these, buckets.At(at), at, was);
    if (!under.Moves) { continue; }
    const double now = under.WantedM;
    positionM[one + 1u] = static_cast<float>(now);
    if (std::fabs(now - was) > kMovedM) {
      ++told.Pressed;
      moved[one / 3u] = 1u;
      if (now < was) {
        told.DeepestM = std::max(told.DeepestM, was - now);
      } else {
        told.RaisedM = std::max(told.RaisedM, now - was);
      }
    }
  }

  if (told.Pressed == 0 || mesh.NormalM == nullptr || mesh.NormalM->empty()) { return; }
  RelightMoved(mesh, moved);
}

} // namespace

void YieldGround(std::span<const Yields> these, Budget within, GroundMesh mesh, Yielded &told) {
  const double finestM = within.FinestM;
  const size_t mostTriangles = within.MostTriangles;
  if (these.empty() || mesh.PositionM == nullptr || mesh.Index == nullptr) { return; }
  std::vector<uint32_t> deepestFirst(these.size());
  for (size_t at = 0; at < deepestFirst.size(); ++at) {
    deepestFirst[at] = static_cast<uint32_t>(at);
  }
  std::ranges::sort(deepestFirst,
                    [these](uint32_t a, uint32_t b) { return these[a].YieldM > these[b].YieldM; });
  const double perTriangle = 0.5 * finestM * finestM;
  std::vector<Yields> taking;
  taking.reserve(deepestFirst.size());
  double wouldCost = 0.0;
  for (const uint32_t which : deepestFirst) {
    const Yields &one = these[which];
    const double wide = one.HighE - one.LowE;
    const double deep = one.HighS - one.LowS;
    const double onRoad = perTriangle > 0.0 ? wide * deep / perTriangle : 0.0;
    const double batter = (wide + 2.0 * one.ApronM) * (deep + 2.0 * one.ApronM) - wide * deep;
    const double onBatter = batter / (0.5 * kCoarsestM * kCoarsestM);
    const double costs = onRoad + onBatter;
    if (wouldCost + costs > static_cast<double>(mostTriangles) && !taking.empty()) {
      ++told.Refused;
      continue;
    }
    wouldCost += costs;
    taking.push_back(one);
  }
  told.Taken = taking.size();
  const std::span<const Yields> held(taking);
  auto tookFrom = std::chrono::steady_clock::now();
  const auto since = [&tookFrom] {
    const auto was = tookFrom;
    tookFrom = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(tookFrom - was).count();
  };
  Refine(held, finestM, mesh, told);
  told.RefineMs = since();
  Cut(held, mesh, told);
  told.CutMs = since();
  Sew(held, mesh, told);
  told.SewMs = since();
  Press(held, mesh, told);
  told.PressMs = since();
  {
    std::unordered_map<uint64_t, uint32_t> standing;
    const std::vector<float> &positionM = *mesh.PositionM;
    for (size_t at = 0; at + 2 < positionM.size(); at += 3) {
      ++standing[PlaceKey({.EastM = static_cast<double>(positionM[at]),
                           .SouthM = static_cast<double>(positionM[at + 2u])})];
    }
    for (const Yields &one : taking) {
      for (size_t at = 0; at + 1 < one.SeamEastSouthM.size(); at += 2) {
        ++told.Seams;
        told.SeamsShared += standing.contains(PlaceKey({.EastM = one.SeamEastSouthM[at],
                                                        .SouthM = one.SeamEastSouthM[at + 1u]}))
                                ? 1u
                                : 0u;
      }
    }
  }
  told.SeamMs = since();
}

} // namespace outshine
