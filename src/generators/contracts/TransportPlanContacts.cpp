#include "TransportPlanFormat.h"
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Generators::TransportFormat {
namespace {
template <class Archive, class Profile> bool ProfileFields(Archive &archive, Profile &profile) {
  return archive(profile.CorridorKey,
                 profile.BeginM.EastM,
                 profile.BeginM.NorthM,
                 profile.EndM.EastM,
                 profile.EndM.NorthM,
                 profile.BeginDerivativeM.EastM,
                 profile.BeginDerivativeM.NorthM,
                 profile.BeginDerivativeM.UpM,
                 profile.EndDerivativeM.EastM,
                 profile.EndDerivativeM.NorthM,
                 profile.EndDerivativeM.UpM,
                 profile.StationLengthM,
                 profile.BeginBedM,
                 profile.EndBedM,
                 profile.BeginPavementHalfWidthM,
                 profile.EndPavementHalfWidthM,
                 profile.BeginHalfWidthM,
                 profile.EndHalfWidthM);
}

template <class Archive, class Stamp> bool ScalarFields(Archive &archive, Stamp &stamp) {
  return archive(stamp.LowE,
                 stamp.HighE,
                 stamp.LowN,
                 stamp.HighN,
                 stamp.AtE,
                 stamp.AtN,
                 stamp.PlateauM,
                 stamp.SlopeE,
                 stamp.SlopeN,
                 stamp.CorridorKey,
                 stamp.ApronM,
                 stamp.YieldM,
                 stamp.SagInv,
                 stamp.Fills,
                 stamp.Kind);
}
}

bool Contact(BinaryValueWriter &out, const EarthworkStamp &stamp) {
  if (!out.Array(std::span(stamp.RingEastNorthM)) ||
      !List(out,
            std::span(stamp.HoleRingsEastNorthM),
            [&](const auto &hole) { return out.Array(std::span(hole)); }) ||
      !out.Array(std::span(stamp.SeamEastNorthM)) || !ScalarFields(out, stamp) ||
      !out(stamp.Profile.has_value()) || (stamp.Profile && !ProfileFields(out, *stamp.Profile)) ||
      !out(stamp.PlanarSupportM.has_value())) {
    return false;
  }
  return !stamp.PlanarSupportM || out(*stamp.PlanarSupportM);
}

bool Contact(Reader &in, EarthworkStamp &stamp) {
  bool profile = false;
  bool support = false;
  if (!in.List(stamp.RingEastNorthM, sizeof(double), [&](auto &value) { return in(value); }) ||
      !in.List(stamp.HoleRingsEastNorthM,
               sizeof(uint32_t),
               [&](auto &hole) {
                 return in.List(hole, sizeof(double), [&](auto &value) { return in(value); });
               }) ||
      !in.List(stamp.SeamEastNorthM, sizeof(double), [&](auto &value) { return in(value); }) ||
      !ScalarFields(in, stamp) || !in(profile)) {
    return false;
  }
  if (profile) {
    stamp.Profile.emplace();
    if (!ProfileFields(in, *stamp.Profile)) { return false; }
  }
  if (!in(support)) { return false; }
  if (support) {
    stamp.PlanarSupportM.emplace();
    return in(*stamp.PlanarSupportM);
  }
  return true;
}
}
