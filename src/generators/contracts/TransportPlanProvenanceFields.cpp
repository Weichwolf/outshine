#include "TransportPlanFormat.h"
#include "TransportProvenance.h"
#include <cstddef>
#include <cstdint>
#include <span>

namespace outshine::Generators::TransportFormat {
namespace {
template <class Archive, class Source> bool SourceFields(Archive &archive, Source &source) {
  return archive.Text(source.Identity.DatasetId) && archive.Text(source.Identity.Revision) &&
         archive.Text(source.Item) && archive(source.Content);
}

template <class Archive, class Path> bool PathFields(Archive &archive, Path &path) {
  return archive(path.PathId,
                 path.FirstReference,
                 path.ReferenceCount,
                 path.SuppliedFields,
                 path.DerivedFields);
}

bool ReferenceFields(BinaryValueWriter &out, const World::TransportSourceReference &reference) {
  return out(reference.Source, reference.ProviderElementId.has_value()) &&
         (!reference.ProviderElementId || out(*reference.ProviderElementId)) &&
         out(reference.Fragment, reference.SegmentOrdinal);
}

bool ReferenceFields(Reader &in, World::TransportSourceReference &reference) {
  bool present = false;
  if (!in(reference.Source, present)) { return false; }
  if (present) {
    uint64_t id = 0;
    if (!in(id)) { return false; }
    reference.ProviderElementId = id;
  } else {
    reference.ProviderElementId.reset();
  }
  return in(reference.Fragment, reference.SegmentOrdinal);
}
}

bool Provenance(BinaryValueWriter &out, const World::TransportProvenance &provenance) {
  return out(provenance.IdentitySpace) &&
         List(out,
              std::span(provenance.Sources),
              [&](const auto &source) { return SourceFields(out, source); }) &&
         List(out,
              std::span(provenance.Paths),
              [&](const auto &path) { return PathFields(out, path); }) &&
         List(out, std::span(provenance.References), [&](const auto &reference) {
           return ReferenceFields(out, reference);
         });
}

bool Provenance(Reader &in, World::TransportProvenance &provenance) {
  constexpr size_t sourceBytes = 3 * sizeof(uint32_t) + 32;
  constexpr size_t pathBytes = sizeof(uint64_t) + 2 * sizeof(uint32_t) + 2 * sizeof(uint16_t);
  constexpr size_t referenceBytes = 2 * sizeof(uint32_t) + sizeof(uint8_t) + 32;
  return in(provenance.IdentitySpace) &&
         in.List(provenance.Sources,
                 sourceBytes,
                 [&](auto &source) { return SourceFields(in, source); }) &&
         in.List(provenance.Paths, pathBytes, [&](auto &path) { return PathFields(in, path); }) &&
         in.List(provenance.References, referenceBytes, [&](auto &reference) {
           return ReferenceFields(in, reference);
         });
}
}
