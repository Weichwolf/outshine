#include "TransportProvenance.h"
#include "TransportTopology.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>

namespace outshine::World {
namespace {
auto SourceKey(const TransportSource &source) {
  return std::tie(source.Identity.DatasetId, source.Identity.Revision, source.Item, source.Content);
}

bool SourcesValid(const TransportProvenance &provenance) {
  return std::ranges::all_of(provenance.Sources,
                             [](const TransportSource &source) {
                               return !source.Identity.DatasetId.empty() && !source.Item.empty();
                             }) &&
         std::ranges::adjacent_find(provenance.Sources,
                                    [](const auto &a, const auto &b) {
                                      return SourceKey(a) >= SourceKey(b);
                                    }) == provenance.Sources.end() &&
         std::ranges::all_of(provenance.References, [&provenance](const auto &reference) {
           return reference.Source < provenance.Sources.size();
         });
}

bool PathsValid(const TransportProvenance &provenance) {
  constexpr uint16_t knownFields = 1023;
  size_t next = 0;
  uint64_t previous = 0;
  for (const auto &path : provenance.Paths) {
    if (path.PathId == 0 || path.PathId <= previous || path.FirstReference != next ||
        path.ReferenceCount == 0 || path.ReferenceCount > provenance.References.size() - next ||
        ((path.SuppliedFields | path.DerivedFields) & ~knownFields) != 0) {
      return false;
    }
    previous = path.PathId;
    next += path.ReferenceCount;
  }
  return next == provenance.References.size();
}
}

bool ValidateTransportProvenance(const TransportTopology &topology,
                                 const TransportProvenance &provenance) {
  if (provenance.IdentitySpace != TransportIdentitySpace::SourceElements &&
      provenance.IdentitySpace != TransportIdentitySpace::DerivedFragments) {
    return false;
  }
  if (provenance.Sources.empty()) {
    return provenance.IdentitySpace == TransportIdentitySpace::SourceElements &&
           provenance.Paths.empty() && provenance.References.empty();
  }
  if (!SourcesValid(provenance) || !PathsValid(provenance)) { return false; }
  const auto edges = topology.Edges();
  size_t at = 0;
  for (const auto &path : provenance.Paths) {
    if (at == edges.size() || edges[at].Id.PathId != path.PathId) { return false; }
    do { ++at; } while (at < edges.size() && edges[at].Id.PathId == path.PathId);
  }
  return at == edges.size();
}
}
