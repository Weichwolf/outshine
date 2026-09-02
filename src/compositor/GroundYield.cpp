#include "Units.h"
#include "GroundYield.h"
#include "math/Vec3.h"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
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
constexpr double kSewCellM = 16.0;
constexpr int kCutPasses = 5;
constexpr double kOffEndM = 0.02;

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

uint64_t BucketAt(EastSouth at) {
  const auto atE =
      static_cast<uint64_t>(static_cast<int64_t>(std::floor(at.EastM / kBucketM)) + 0x20000000LL);
  const auto atS =
      static_cast<uint64_t>(static_cast<int64_t>(std::floor(at.SouthM / kBucketM)) + 0x20000000LL);
  return (atE << 32U) | atS;
}

using Buckets = std::unordered_map<uint64_t, std::vector<uint32_t>>;

Buckets BucketOver(std::span<const Yields> these) {
  Buckets out;
  for (size_t at = 0; at < these.size(); ++at) {
    const Yields &one = these[at];
    const auto fromE = static_cast<int64_t>(std::floor((one.LowE - one.ApronM) / kBucketM));
    const auto toE = static_cast<int64_t>(std::floor((one.HighE + one.ApronM) / kBucketM));
    const auto fromS = static_cast<int64_t>(std::floor((one.LowS - one.ApronM) / kBucketM));
    const auto toS = static_cast<int64_t>(std::floor((one.HighS + one.ApronM) / kBucketM));
    for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
      for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
        const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
        const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
        out[(atE << 32U) | atS].push_back(static_cast<uint32_t>(at));
      }
    }
  }
  return out;
}

class Attributes {
public:
  explicit Attributes(const GroundMesh &mesh) : Mesh_(mesh) {}

  [[nodiscard]] uint32_t Inside(uint32_t a, uint32_t b, uint32_t c, EastSouth at) const {
    const double eastM = at.EastM;
    const double southM = at.SouthM;
    const std::vector<float> &held = *Mesh_.PositionM;
    const auto aE = static_cast<double>(held[static_cast<size_t>(a) * 3u]);
    const auto aS = static_cast<double>(held[static_cast<size_t>(a) * 3u + 2u]);
    const auto bE = static_cast<double>(held[static_cast<size_t>(b) * 3u]);
    const auto bS = static_cast<double>(held[static_cast<size_t>(b) * 3u + 2u]);
    const auto cE = static_cast<double>(held[static_cast<size_t>(c) * 3u]);
    const auto cS = static_cast<double>(held[static_cast<size_t>(c) * 3u + 2u]);
    const double twice = (bS - cS) * (aE - cE) + (cE - bE) * (aS - cS);
    const double one = std::fabs(twice) > kLeastTurnRad
                           ? ((bS - cS) * (eastM - cE) + (cE - bE) * (southM - cS)) / twice
                           : 1.0 / 3.0;
    const double two = std::fabs(twice) > kLeastTurnRad
                           ? ((cS - aS) * (eastM - cE) + (aE - cE) * (southM - cS)) / twice
                           : 1.0 / 3.0;
    Blend(*Mesh_.PositionM, 3u, a, b, c, one, two);
    Blend(*Mesh_.NormalM, 3u, a, b, c, one, two);
    if (Mesh_.ColourRgba != nullptr) { Blend(*Mesh_.ColourRgba, 4u, a, b, c, one, two); }
    if (Mesh_.Uv != nullptr) { Blend(*Mesh_.Uv, 2u, a, b, c, one, two); }
    const auto point = static_cast<uint32_t>(Mesh_.PositionM->size() / 3u) - 1u;
    (*Mesh_.PositionM)[static_cast<size_t>(point) * 3u] = static_cast<float>(eastM);
    (*Mesh_.PositionM)[static_cast<size_t>(point) * 3u + 2u] = static_cast<float>(southM);
    return point;
  }

  [[nodiscard]] uint32_t Along(uint32_t a, uint32_t b, double part) const {
    Lerp(*Mesh_.PositionM, 3u, a, b, part);
    Lerp(*Mesh_.NormalM, 3u, a, b, part);
    if (Mesh_.ColourRgba != nullptr) { Lerp(*Mesh_.ColourRgba, 4u, a, b, part); }
    if (Mesh_.Uv != nullptr) { Lerp(*Mesh_.Uv, 2u, a, b, part); }
    return static_cast<uint32_t>(Mesh_.PositionM->size() / 3u) - 1u;
  }

  [[nodiscard]] uint32_t Midpoint(uint32_t a, uint32_t b) const {
    Lerp(*Mesh_.PositionM, 3u, a, b);
    Lerp(*Mesh_.NormalM, 3u, a, b);
    if (Mesh_.ColourRgba != nullptr) { Lerp(*Mesh_.ColourRgba, 4u, a, b); }
    if (Mesh_.Uv != nullptr) { Lerp(*Mesh_.Uv, 2u, a, b); }
    return static_cast<uint32_t>(Mesh_.PositionM->size() / 3u) - 1u;
  }

private:
  static void Blend(std::vector<float> &held,
                    uint32_t wide,
                    uint32_t a,
                    uint32_t b,
                    uint32_t c,
                    double one,
                    double two) {
    if (held.empty()) { return; }
    const double three = 1.0 - one - two;
    for (uint32_t axis = 0; axis < wide; ++axis) {
      held.push_back(static_cast<float>(
          one * static_cast<double>(held[static_cast<size_t>(a) * wide + axis]) +
          two * static_cast<double>(held[static_cast<size_t>(b) * wide + axis]) +
          three * static_cast<double>(held[static_cast<size_t>(c) * wide + axis])));
    }
  }

  static void Lerp(std::vector<float> &held, uint32_t wide, uint32_t a, uint32_t b) {
    Lerp(held, wide, a, b, 0.5);
  }

  static void
  Lerp(std::vector<float> &held, uint32_t wide, uint32_t from, uint32_t to, double part) {
    if (held.empty()) { return; }
    for (uint32_t axis = 0; axis < wide; ++axis) {
      const auto was = static_cast<double>(held[static_cast<size_t>(from) * wide + axis]);
      const auto now = static_cast<double>(held[static_cast<size_t>(to) * wide + axis]);
      held.push_back(static_cast<float>(was + (now - was) * part));
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

double WantedEdgeM(const Buckets &buckets,
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
  const auto bucket = buckets.find(BucketAt({.EastM = centreE, .SouthM = centreS}));
  if (bucket == buckets.end()) { return 0.0; }
  double nearest = kBeyondAnyCoordinate;
  for (const uint32_t which : bucket->second) {
    nearest = std::min(nearest, AwayFrom(these[which], {.EastM = centreE, .SouthM = centreS}));
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

void Refine(std::span<const Yields> these, double finestM, GroundMesh &mesh, Yielded &told) {
  const Buckets buckets = BucketOver(these);
  const Attributes lerp(mesh);
  std::vector<uint32_t> &index = *mesh.Index;
  std::vector<uint32_t> next;

  for (int pass = 0; pass < kMostPasses; ++pass) {
    const float *positionM = mesh.PositionM->data();
    std::unordered_map<uint64_t, uint32_t> split;
    bool any = false;
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const double wanted = WantedEdgeM(buckets, these, finestM, positionM, face);
      if (!(wanted > 0.0)) { continue; }
      if (LongestEdgeM(positionM, face) <= wanted) { continue; }
      any = true;
      for (int edge = 0; edge < 3; ++edge) {
        const uint32_t a = face[edge];
        const uint32_t b = face[(edge + 1) % 3];
        const uint64_t key = PlaceKey(
            {.EastM = 0.5 * (static_cast<double>(positionM[static_cast<size_t>(a) * 3u]) +
                             static_cast<double>(positionM[static_cast<size_t>(b) * 3u])),
             .SouthM = 0.5 * (static_cast<double>(positionM[static_cast<size_t>(a) * 3u + 2u]) +
                              static_cast<double>(positionM[static_cast<size_t>(b) * 3u + 2u]))});
        split.emplace(key, kNoVertex);
      }
    }
    if (!any) { break; }
    told.Passes = static_cast<size_t>(pass) + 1u;

    const auto midpoint = [&](uint32_t a, uint32_t b) {
      const float *held = mesh.PositionM->data();
      const uint64_t key =
          PlaceKey({.EastM = 0.5 * (static_cast<double>(held[static_cast<size_t>(a) * 3u]) +
                                    static_cast<double>(held[static_cast<size_t>(b) * 3u])),
                    .SouthM = 0.5 * (static_cast<double>(held[static_cast<size_t>(a) * 3u + 2u]) +
                                     static_cast<double>(held[static_cast<size_t>(b) * 3u + 2u]))});
      const auto found = split.find(key);
      if (found == split.end()) { return kNoVertex; }
      if (found->second == kNoVertex) {
        found->second = lerp.Midpoint(a, b);
        ++told.VerticesAdded;
      }
      return found->second;
    };

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const std::array<uint32_t, 3> cut = {
          {midpoint(face[0], face[1]), midpoint(face[1], face[2]), midpoint(face[2], face[0])}};
      const int cuts = (cut[0] != kNoVertex ? 1 : 0) + (cut[1] != kNoVertex ? 1 : 0) +
                       (cut[2] != kNoVertex ? 1 : 0);
      const auto lay = [&next](uint32_t a, uint32_t b, uint32_t c) {
        next.push_back(a);
        next.push_back(b);
        next.push_back(c);
      };
      if (cuts == 0) {
        lay(face[0], face[1], face[2]);
        continue;
      }
      if (cuts == 3) {
        lay(face[0], cut[0], cut[2]);
        lay(cut[0], face[1], cut[1]);
        lay(cut[2], cut[1], face[2]);
        lay(cut[0], cut[1], cut[2]);
        continue;
      }
      if (cuts == 1) {
        for (int edge = 0; edge < 3; ++edge) {
          if (cut[edge] == kNoVertex) { continue; }
          lay(face[edge], cut[edge], face[(edge + 2) % 3]);
          lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
        }
        continue;
      }
      for (int edge = 0; edge < 3; ++edge) {
        if (cut[edge] != kNoVertex) { continue; }
        const uint32_t a = face[edge];
        const uint32_t b = face[(edge + 1) % 3];
        const uint32_t c = face[(edge + 2) % 3];
        const uint32_t onBc = cut[(edge + 1) % 3];
        const uint32_t onCa = cut[(edge + 2) % 3];
        lay(a, b, onBc);
        lay(a, onBc, onCa);
        lay(onCa, onBc, c);
        break;
      }
    }
    told.TrianglesAdded += (next.size() - index.size()) / 3u;
    index.swap(next);
  }
}

bool MeetsAt(double aE,
             double aS,
             double bE,
             double bS,
             double cE,
             double cS,
             double dE,
             double dS,
             double *part) {
  const double runE = bE - aE;
  const double runS = bS - aS;
  const double overE = dE - cE;
  const double overS = dS - cS;
  const double under = runE * overS - runS * overE;
  if (std::fabs(under) < kParallelCross) { return false; }
  const double mine = ((cE - aE) * overS - (cS - aS) * overE) / under;
  const double yours = ((cE - aE) * runS - (cS - aS) * runE) / under;
  if (mine <= kOffEndM || mine >= 1.0 - kOffEndM || yours < 0.0 || yours > 1.0) { return false; }
  *part = mine;
  return true;
}

void Cut(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  std::unordered_map<uint64_t, std::vector<uint32_t>> seamsAt;
  std::vector<double> seams;
  for (const Yields &one : these) {
    const size_t corners = one.SeamEastSouthM.size() / 2u;
    for (size_t at = 0, last = corners > 0 ? corners - 1u : 0; at < corners; last = at++) {
      const double aE = one.SeamEastSouthM[last * 2u];
      const double aS = one.SeamEastSouthM[last * 2u + 1u];
      const double bE = one.SeamEastSouthM[at * 2u];
      const double bS = one.SeamEastSouthM[at * 2u + 1u];
      const auto which = static_cast<uint32_t>(seams.size() / 4u);
      seams.push_back(aE);
      seams.push_back(aS);
      seams.push_back(bE);
      seams.push_back(bS);
      const auto fromE = static_cast<int64_t>(std::floor(std::min(aE, bE) / kSewCellM));
      const auto toE = static_cast<int64_t>(std::floor(std::max(aE, bE) / kSewCellM));
      const auto fromS = static_cast<int64_t>(std::floor(std::min(aS, bS) / kSewCellM));
      const auto toS = static_cast<int64_t>(std::floor(std::max(aS, bS) / kSewCellM));
      if ((toE - fromE + 1) * (toS - fromS + 1) > 256) { continue; }
      for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
        for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
          const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
          const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
          seamsAt[(atE << 32U) | atS].push_back(which);
        }
      }
    }
  }
  if (seams.empty()) { return; }

  std::vector<uint32_t> &index = *mesh.Index;
  const Attributes lerp(mesh);
  std::vector<uint32_t> next;

  for (int pass = 0; pass < kCutPasses; ++pass) {
    const float *positionM = mesh.PositionM->data();
    std::unordered_map<uint64_t, std::pair<uint32_t, double>> split;
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      for (int edge = 0; edge < 3; ++edge) {
        const uint32_t a = index[at + static_cast<size_t>(edge)];
        const uint32_t b = index[at + static_cast<size_t>((edge + 1) % 3)];
        const auto aE = static_cast<double>(positionM[static_cast<size_t>(a) * 3u]);
        const auto aS = static_cast<double>(positionM[static_cast<size_t>(a) * 3u + 2u]);
        const auto bE = static_cast<double>(positionM[static_cast<size_t>(b) * 3u]);
        const auto bS = static_cast<double>(positionM[static_cast<size_t>(b) * 3u + 2u]);
        const uint64_t key = EdgeKey({.EastM = aE, .SouthM = aS}, {.EastM = bE, .SouthM = bS});
        if (split.contains(key)) { continue; }
        const auto atE = static_cast<uint64_t>(
            static_cast<int64_t>(std::floor(0.5 * (aE + bE) / kSewCellM)) + 0x20000000LL);
        const auto atS = static_cast<uint64_t>(
            static_cast<int64_t>(std::floor(0.5 * (aS + bS) / kSewCellM)) + 0x20000000LL);
        const auto bucket = seamsAt.find((atE << 32U) | atS);
        if (bucket == seamsAt.end()) { continue; }
        double part = 0.0;
        bool met = false;
        for (const uint32_t which : bucket->second) {
          if (MeetsAt(aE,
                      aS,
                      bE,
                      bS,
                      seams[static_cast<size_t>(which) * 4u],
                      seams[static_cast<size_t>(which) * 4u + 1u],
                      seams[static_cast<size_t>(which) * 4u + 2u],
                      seams[static_cast<size_t>(which) * 4u + 3u],
                      &part)) {
            met = true;
            break;
          }
        }
        if (!met) { continue; }
        split.emplace(key,
                      std::pair<uint32_t, double>{
                          kNoVertex, aE < bE || (aE == bE && aS < bS) ? part : 1.0 - part});
      }
    }
    if (split.empty()) { break; }
    told.Passes = std::max(told.Passes, static_cast<size_t>(pass) + 1u);

    const auto cutOf = [&](uint32_t a, uint32_t b) {
      const float *held = mesh.PositionM->data();
      const auto aE = static_cast<double>(held[static_cast<size_t>(a) * 3u]);
      const auto aS = static_cast<double>(held[static_cast<size_t>(a) * 3u + 2u]);
      const auto bE = static_cast<double>(held[static_cast<size_t>(b) * 3u]);
      const auto bS = static_cast<double>(held[static_cast<size_t>(b) * 3u + 2u]);
      const auto found =
          split.find(EdgeKey({.EastM = aE, .SouthM = aS}, {.EastM = bE, .SouthM = bS}));
      if (found == split.end()) { return kNoVertex; }
      if (found->second.first == kNoVertex) {
        const bool forward = aE < bE || (aE == bE && aS < bS);
        found->second.first =
            lerp.Along(a, b, forward ? found->second.second : 1.0 - found->second.second);
        ++told.VerticesAdded;
      }
      return found->second.first;
    };

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const std::array<uint32_t, 3> face = {{index[at], index[at + 1u], index[at + 2u]}};
      const std::array<uint32_t, 3> cut = {
          {cutOf(face[0], face[1]), cutOf(face[1], face[2]), cutOf(face[2], face[0])}};
      const int cuts = (cut[0] != kNoVertex ? 1 : 0) + (cut[1] != kNoVertex ? 1 : 0) +
                       (cut[2] != kNoVertex ? 1 : 0);
      const auto lay = [&next](uint32_t a, uint32_t b, uint32_t c) {
        next.push_back(a);
        next.push_back(b);
        next.push_back(c);
      };
      if (cuts == 0) {
        lay(face[0], face[1], face[2]);
        continue;
      }
      if (cuts == 3) {
        lay(face[0], cut[0], cut[2]);
        lay(cut[0], face[1], cut[1]);
        lay(cut[2], cut[1], face[2]);
        lay(cut[0], cut[1], cut[2]);
        continue;
      }
      if (cuts == 1) {
        for (int edge = 0; edge < 3; ++edge) {
          if (cut[edge] == kNoVertex) { continue; }
          lay(face[edge], cut[edge], face[(edge + 2) % 3]);
          lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
        }
        continue;
      }
      for (int edge = 0; edge < 3; ++edge) {
        if (cut[edge] != kNoVertex) { continue; }
        const uint32_t a = face[edge];
        const uint32_t b = face[(edge + 1) % 3];
        const uint32_t c = face[(edge + 2) % 3];
        lay(a, b, cut[(edge + 1) % 3]);
        lay(a, cut[(edge + 1) % 3], cut[(edge + 2) % 3]);
        lay(cut[(edge + 2) % 3], cut[(edge + 1) % 3], c);
        break;
      }
    }
    told.TrianglesAdded += (next.size() - index.size()) / 3u;
    index.swap(next);
  }
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
  const Attributes lerp(mesh);

  for (int pass = 0; pass < kSewPasses; ++pass) {
    std::unordered_map<uint64_t, std::vector<uint32_t>> facesAt;
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const float *held = positionM.data();
      double lowE = kBeyondAnyCoordinate;
      double highE = -kBeyondAnyCoordinate;
      double lowS = kBeyondAnyCoordinate;
      double highS = -kBeyondAnyCoordinate;
      for (int corner = 0; corner < 3; ++corner) {
        const size_t one = static_cast<size_t>(index[at + static_cast<size_t>(corner)]) * 3u;
        lowE = std::min(lowE, static_cast<double>(held[one]));
        highE = std::max(highE, static_cast<double>(held[one]));
        lowS = std::min(lowS, static_cast<double>(held[one + 2u]));
        highS = std::max(highS, static_cast<double>(held[one + 2u]));
      }
      const auto fromE = static_cast<int64_t>(std::floor(lowE / kSewCellM));
      const auto toE = static_cast<int64_t>(std::floor(highE / kSewCellM));
      const auto fromS = static_cast<int64_t>(std::floor(lowS / kSewCellM));
      const auto toS = static_cast<int64_t>(std::floor(highS / kSewCellM));
      if ((toE - fromE + 1) * (toS - fromS + 1) > 256) { continue; }
      for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
        for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
          const auto atE = static_cast<uint64_t>(cellE + 0x20000000LL);
          const auto atS = static_cast<uint64_t>(cellS + 0x20000000LL);
          facesAt[(atE << 32U) | atS].push_back(static_cast<uint32_t>(at));
        }
      }
    }

    std::unordered_map<uint32_t, uint32_t> claimed;
    std::unordered_map<uint64_t, uint32_t> made;
    bool any = false;
    for (uint32_t which = 0; which < static_cast<uint32_t>(sewn.size()); ++which) {
      if (sewn[which] != 0u) { continue; }
      const double eastM = seams[static_cast<size_t>(which) * 2u];
      const double southM = seams[static_cast<size_t>(which) * 2u + 1u];
      const auto atE =
          static_cast<uint64_t>(static_cast<int64_t>(std::floor(eastM / kSewCellM)) + 0x20000000LL);
      const auto atS = static_cast<uint64_t>(static_cast<int64_t>(std::floor(southM / kSewCellM)) +
                                             0x20000000LL);
      const auto bucket = facesAt.find((atE << 32U) | atS);
      if (bucket == facesAt.end()) { continue; }
      bool held = false;
      for (const uint32_t face : bucket->second) {
        const size_t a = static_cast<size_t>(index[face]) * 3u;
        const size_t b = static_cast<size_t>(index[face + 1u]) * 3u;
        const size_t c = static_cast<size_t>(index[face + 2u]) * 3u;
        const auto aE = static_cast<double>(positionM[a]);
        const auto aS = static_cast<double>(positionM[a + 2u]);
        const auto bE = static_cast<double>(positionM[b]);
        const auto bS = static_cast<double>(positionM[b + 2u]);
        const auto cE = static_cast<double>(positionM[c]);
        const auto cS = static_cast<double>(positionM[c + 2u]);
        const double twice = (bS - cS) * (aE - cE) + (cE - bE) * (aS - cS);
        if (std::fabs(twice) < kLeastTurnRad) { continue; }
        const double one = ((bS - cS) * (eastM - cE) + (cE - bE) * (southM - cS)) / twice;
        const double two = ((cS - aS) * (eastM - cE) + (aE - cE) * (southM - cS)) / twice;
        const double three = 1.0 - one - two;
        if (one < -kLeastTurnRad || two < -kLeastTurnRad || three < -kLeastTurnRad) { continue; }
        if (claimed.contains(face)) {
          held = true;
          continue;
        }
        claimed[face] = which;
        any = true;
        held = true;
      }
      if (held) { sewn[which] = 1u; }
    }
    if (!any) { break; }
    told.Passes = std::max(told.Passes, static_cast<size_t>(pass) + 1u);

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const auto found = claimed.find(static_cast<uint32_t>(at));
      if (found == claimed.end()) {
        next.push_back(index[at]);
        next.push_back(index[at + 1u]);
        next.push_back(index[at + 2u]);
        continue;
      }
      const uint32_t which = found->second;
      const double eastM = seams[static_cast<size_t>(which) * 2u];
      const double southM = seams[static_cast<size_t>(which) * 2u + 1u];
      const uint64_t key = PlaceKey({.EastM = eastM, .SouthM = southM});
      auto stood = made.find(key);
      if (stood == made.end()) {
        stood = made.emplace(key,
                             lerp.Inside(index[at],
                                         index[at + 1u],
                                         index[at + 2u],
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

void Press(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  const Buckets buckets = BucketOver(these);
  std::vector<float> &positionM = *mesh.PositionM;
  std::vector<uint8_t> moved(positionM.size() / 3u, 0u);
  for (size_t one = 0; one + 2 < positionM.size(); one += 3) {
    const auto eastM = static_cast<double>(positionM[one]);
    const auto southM = static_cast<double>(positionM[one + 2u]);
    const auto was = static_cast<double>(positionM[one + 1u]);
    double lowest = was;
    double highest = was;
    double roofM = kBeyondAnyCoordinate;
    const auto bucket = buckets.find(BucketAt({.EastM = eastM, .SouthM = southM}));
    if (bucket == buckets.end()) { continue; }
    for (const uint32_t which : bucket->second) {
      const Yields &held = these[which];
      if (eastM < held.LowE - held.ApronM || eastM > held.HighE + held.ApronM ||
          southM < held.LowS - held.ApronM || southM > held.HighS + held.ApronM) {
        continue;
      }
      const size_t corners = held.RingEastSouthM.size() / 2u;
      if (corners < 3) { continue; }
      bool inside = false;
      double nearest = kBeyondAnyCoordinate;
      for (size_t edge = 0, last = corners - 1u; edge < corners; last = edge++) {
        const double aE = held.RingEastSouthM[edge * 2u];
        const double aS = held.RingEastSouthM[edge * 2u + 1u];
        const double bE = held.RingEastSouthM[last * 2u];
        const double bS = held.RingEastSouthM[last * 2u + 1u];
        if ((aS > southM) != (bS > southM) && eastM < (bE - aE) * (southM - aS) / (bS - aS) + aE) {
          inside = !inside;
        }
        const double runE = bE - aE;
        const double runS = bS - aS;
        const double runM = runE * runE + runS * runS;
        const double part =
            runM > kLeastTurnRad
                ? std::clamp(((eastM - aE) * runE + (southM - aS) * runS) / runM, 0.0, 1.0)
                : 0.0;
        const double offE = eastM - (aE + runE * part);
        const double offS = southM - (aS + runS * part);
        nearest = std::min(nearest, std::sqrt(offE * offE + offS * offS));
      }
      const double out = inside ? 0.0 : nearest;
      if (out > held.ApronM) { continue; }
      const double onRoad = held.WantsAt({.EastM = eastM, .SouthM = southM});
      const double cutAt = onRoad + out * kBatterRise;
      const double fillAt = onRoad - out * kBatterRise;
      lowest = std::min(lowest, cutAt);
      roofM = std::min(roofM, cutAt);
      if (held.Fills) { highest = std::max(highest, fillAt); }
    }
    double weight = 0.0;
    double wanted = was;
    if (lowest < was) {
      wanted = lowest;
      weight = 1.0;
    } else if (highest > was) {
      wanted = std::min(highest, roofM);
      weight = wanted > was ? 1.0 : 0.0;
    }
    if (!(weight > 0.0)) { continue; }
    const double now = wanted;
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
  std::vector<float> &normalM = *mesh.NormalM;
  const std::vector<uint32_t> &index = *mesh.Index;
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
  Refine(held, finestM, mesh, told);
  Cut(held, mesh, told);
  Sew(held, mesh, told);
  Press(held, mesh, told);
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
}

} // namespace outshine
