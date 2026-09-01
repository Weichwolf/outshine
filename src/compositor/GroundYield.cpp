#include "GroundYield.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <cmath>
#include <numbers>
#include <unordered_map>

namespace outshine {

namespace {

constexpr int kMostPasses = 6;
constexpr double kWeldM = 0.01;
constexpr double kMovedM = 0.01;
constexpr double kBatterRise = 1.0 / 1.5;
constexpr double kCoarsestM = 40.0;
constexpr double kEdgeGrade = 0.35;

uint64_t PlaceKey(double eastM, double southM) {
  const auto atE = (int64_t)std::llround(eastM / kWeldM);
  const auto atS = (int64_t)std::llround(southM / kWeldM);
  return ((uint64_t)(atE + 0x2000000000LL) << 24u) ^ (uint64_t)(atS + 0x2000000000LL);
}

constexpr double kBucketM = 32.0;

uint64_t BucketAt(double eastM, double southM) {
  const auto atE = (uint64_t)((int64_t)std::floor(eastM / kBucketM) + 0x20000000LL);
  const auto atS = (uint64_t)((int64_t)std::floor(southM / kBucketM) + 0x20000000LL);
  return (atE << 32u) | atS;
}

using Buckets = std::unordered_map<uint64_t, std::vector<uint32_t>>;

Buckets BucketOver(std::span<const Yields> these) {
  Buckets out;
  for (size_t at = 0; at < these.size(); ++at) {
    const Yields &one = these[at];
    const auto fromE = (int64_t)std::floor((one.LowE - one.ApronM) / kBucketM);
    const auto toE = (int64_t)std::floor((one.HighE + one.ApronM) / kBucketM);
    const auto fromS = (int64_t)std::floor((one.LowS - one.ApronM) / kBucketM);
    const auto toS = (int64_t)std::floor((one.HighS + one.ApronM) / kBucketM);
    for (int64_t cellE = fromE; cellE <= toE; ++cellE) {
      for (int64_t cellS = fromS; cellS <= toS; ++cellS) {
        const auto atE = (uint64_t)(cellE + 0x20000000LL);
        const auto atS = (uint64_t)(cellS + 0x20000000LL);
        out[(atE << 32u) | atS].push_back((uint32_t)at);
      }
    }
  }
  return out;
}

class Attributes {
public:
  explicit Attributes(const GroundMesh &mesh) : Mesh_(mesh) {}

  [[nodiscard]] uint32_t Midpoint(uint32_t a, uint32_t b) const {
    Lerp(*Mesh_.PositionM, 3u, a, b);
    Lerp(*Mesh_.NormalM, 3u, a, b);
    if (Mesh_.ColourRgba != nullptr) { Lerp(*Mesh_.ColourRgba, 4u, a, b); }
    if (Mesh_.Uv != nullptr) { Lerp(*Mesh_.Uv, 2u, a, b); }
    return (uint32_t)(Mesh_.PositionM->size() / 3u) - 1u;
  }

private:
  static void Lerp(std::vector<float> &held, uint32_t wide, uint32_t a, uint32_t b) {
    if (held.empty()) { return; }
    for (uint32_t axis = 0; axis < wide; ++axis) {
      held.push_back(0.5f * (held[(size_t)a * wide + axis] + held[(size_t)b * wide + axis]));
    }
  }

  const GroundMesh &Mesh_;
};

double AwayFrom(const Yields &one, double eastM, double southM) {
  const double offE = std::max({one.LowE - eastM, 0.0, eastM - one.HighE});
  const double offS = std::max({one.LowS - southM, 0.0, southM - one.HighS});
  return std::sqrt(offE * offE + offS * offS);
}

double WantedEdgeM(const Buckets &buckets,
                   std::span<const Yields> these,
                   double finestM,
                   const float *positionM,
                   const uint32_t face[3]) {
  double centreE = 0.0;
  double centreS = 0.0;
  for (int at = 0; at < 3; ++at) {
    centreE += (double)positionM[(size_t)face[at] * 3u];
    centreS += (double)positionM[(size_t)face[at] * 3u + 2u];
  }
  centreE /= 3.0;
  centreS /= 3.0;
  const auto bucket = buckets.find(BucketAt(centreE, centreS));
  if (bucket == buckets.end()) { return 0.0; }
  double nearest = 1.0e30;
  for (const uint32_t which : bucket->second) {
    nearest = std::min(nearest, AwayFrom(these[which], centreE, centreS));
  }
  if (nearest > 1.0e29) { return 0.0; }
  return std::clamp(finestM + kEdgeGrade * nearest, finestM, kCoarsestM);
}

double LongestEdgeM(const float *positionM, const uint32_t face[3]) {
  double most = 0.0;
  for (int at = 0; at < 3; ++at) {
    const uint32_t a = face[at];
    const uint32_t b = face[(at + 1) % 3];
    const double runE = (double)positionM[(size_t)a * 3u] - (double)positionM[(size_t)b * 3u];
    const double runS =
        (double)positionM[(size_t)a * 3u + 2u] - (double)positionM[(size_t)b * 3u + 2u];
    most = std::max(most, std::sqrt(runE * runE + runS * runS));
  }
  return most;
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
      const uint32_t face[3] = {index[at], index[at + 1u], index[at + 2u]};
      const double wanted = WantedEdgeM(buckets, these, finestM, positionM, face);
      if (!(wanted > 0.0)) { continue; }
      if (LongestEdgeM(positionM, face) <= wanted) { continue; }
      any = true;
      for (int edge = 0; edge < 3; ++edge) {
        const uint32_t a = face[edge];
        const uint32_t b = face[(edge + 1) % 3];
        const uint64_t key =
            PlaceKey(0.5 * ((double)positionM[(size_t)a * 3u] + (double)positionM[(size_t)b * 3u]),
                     0.5 * ((double)positionM[(size_t)a * 3u + 2u] +
                            (double)positionM[(size_t)b * 3u + 2u]));
        split.emplace(key, 0xffffffffu);
      }
    }
    if (!any) { break; }
    told.Passes = (size_t)pass + 1u;

    const auto midpoint = [&](uint32_t a, uint32_t b) {
      const float *held = mesh.PositionM->data();
      const uint64_t key =
          PlaceKey(0.5 * ((double)held[(size_t)a * 3u] + (double)held[(size_t)b * 3u]),
                   0.5 * ((double)held[(size_t)a * 3u + 2u] + (double)held[(size_t)b * 3u + 2u]));
      const auto found = split.find(key);
      if (found == split.end()) { return 0xffffffffu; }
      if (found->second == 0xffffffffu) {
        found->second = lerp.Midpoint(a, b);
        ++told.VerticesAdded;
      }
      return found->second;
    };

    next.clear();
    next.reserve(index.size() * 2u);
    for (size_t at = 0; at + 2 < index.size(); at += 3) {
      const uint32_t face[3] = {index[at], index[at + 1u], index[at + 2u]};
      const uint32_t cut[3] = {
          midpoint(face[0], face[1]), midpoint(face[1], face[2]), midpoint(face[2], face[0])};
      const int cuts = (cut[0] != 0xffffffffu ? 1 : 0) + (cut[1] != 0xffffffffu ? 1 : 0) +
                       (cut[2] != 0xffffffffu ? 1 : 0);
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
          if (cut[edge] == 0xffffffffu) { continue; }
          lay(face[edge], cut[edge], face[(edge + 2) % 3]);
          lay(cut[edge], face[(edge + 1) % 3], face[(edge + 2) % 3]);
        }
        continue;
      }
      for (int edge = 0; edge < 3; ++edge) {
        if (cut[edge] != 0xffffffffu) { continue; }
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

void Press(std::span<const Yields> these, const GroundMesh &mesh, Yielded &told) {
  const Buckets buckets = BucketOver(these);
  std::vector<float> &positionM = *mesh.PositionM;
  std::vector<uint8_t> moved(positionM.size() / 3u, 0u);
  for (size_t one = 0; one + 2 < positionM.size(); one += 3) {
    const auto eastM = (double)positionM[one];
    const auto southM = (double)positionM[one + 2u];
    const auto was = (double)positionM[one + 1u];
    double lowest = was;
    double highest = was;
    const auto bucket = buckets.find(BucketAt(eastM, southM));
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
      double nearest = 1.0e30;
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
            runM > 1.0e-9
                ? std::clamp(((eastM - aE) * runE + (southM - aS) * runS) / runM, 0.0, 1.0)
                : 0.0;
        const double offE = eastM - (aE + runE * part);
        const double offS = southM - (aS + runS * part);
        nearest = std::min(nearest, std::sqrt(offE * offE + offS * offS));
      }
      const double out = inside ? 0.0 : nearest;
      if (out > held.ApronM) { continue; }
      const double onRoad = held.WantsAt(eastM, southM);
      const double cutAt = onRoad + out * kBatterRise;
      const double fillAt = onRoad - out * kBatterRise;
      lowest = std::min(lowest, cutAt);
      if (held.Fills) { highest = std::max(highest, fillAt); }
    }
    double weight = 0.0;
    double wanted = was;
    if (lowest < was) {
      wanted = lowest;
      weight = 1.0;
    } else if (highest > was) {
      wanted = highest;
      weight = 1.0;
    }
    if (!(weight > 0.0)) { continue; }
    const double now = wanted;
    positionM[one + 1u] = (float)now;
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
    const uint32_t face[3] = {index[at], index[at + 1u], index[at + 2u]};
    if (moved[face[0]] == 0u && moved[face[1]] == 0u && moved[face[2]] == 0u) { continue; }
    double edgeA[3] = {};
    double edgeB[3] = {};
    for (int axis = 0; axis < 3; ++axis) {
      edgeA[axis] = (double)positionM[(size_t)face[1] * 3u + (uint32_t)axis] -
                    (double)positionM[(size_t)face[0] * 3u + (uint32_t)axis];
      edgeB[axis] = (double)positionM[(size_t)face[2] * 3u + (uint32_t)axis] -
                    (double)positionM[(size_t)face[0] * 3u + (uint32_t)axis];
    }
    const double up[3] = {edgeA[1] * edgeB[2] - edgeA[2] * edgeB[1],
                          edgeA[2] * edgeB[0] - edgeA[0] * edgeB[2],
                          edgeA[0] * edgeB[1] - edgeA[1] * edgeB[0]};
    for (const uint32_t one : face) {
      if (moved[one] == 0u) { continue; }
      for (int axis = 0; axis < 3; ++axis) {
        normalM[(size_t)one * 3u + (uint32_t)axis] += (float)up[axis];
      }
    }
  }
  for (size_t at = 0; at < moved.size(); ++at) {
    if (moved[at] == 0u) { continue; }
    const double len = std::sqrt((double)normalM[at * 3u] * (double)normalM[at * 3u] +
                                 (double)normalM[at * 3u + 1u] * (double)normalM[at * 3u + 1u] +
                                 (double)normalM[at * 3u + 2u] * (double)normalM[at * 3u + 2u]);
    if (!(len > 1.0e-9)) {
      normalM[at * 3u + 1u] = 1.0f;
      continue;
    }
    for (int axis = 0; axis < 3; ++axis) {
      normalM[at * 3u + (uint32_t)axis] = (float)((double)normalM[at * 3u + (uint32_t)axis] / len);
    }
  }
}

} // namespace

void YieldGround(std::span<const Yields> these,
                 double finestM,
                 size_t mostTriangles,
                 GroundMesh mesh,
                 Yielded &told) {
  if (these.empty() || mesh.PositionM == nullptr || mesh.Index == nullptr) { return; }
  std::vector<uint32_t> deepestFirst(these.size());
  for (size_t at = 0; at < deepestFirst.size(); ++at) { deepestFirst[at] = (uint32_t)at; }
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
    if (wouldCost + costs > (double)mostTriangles && !taking.empty()) {
      ++told.Refused;
      continue;
    }
    wouldCost += costs;
    taking.push_back(one);
  }
  told.Taken = taking.size();
  const std::span<const Yields> held(taking);
  Refine(held, finestM, mesh, told);
  Press(held, mesh, told);
}

} // namespace outshine
