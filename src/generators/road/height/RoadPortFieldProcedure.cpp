#include "RoadPortFieldProgram.h"
#include "HeightProgramInternal.h"
#include "RoadGroundAffine.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <stop_token>
#include <string>
#include <interfaces/highs_c_api.h>

namespace outshine::Generators {
namespace {
using Error = RoadProfiles::HeightFitError;
constexpr size_t kProcedureBytesMost = size_t{16} * 1024 * 1024;
constexpr size_t kHeaderBytesMost = 1024;
constexpr size_t kPlaneBytes = 2 * sizeof(double);
constexpr size_t kPatchBytes = 2 * sizeof(uint32_t) + sizeof(uint8_t) + 9 * sizeof(double);
constexpr size_t kPriorBytes = sizeof(uint32_t) + 3 * sizeof(double);
constexpr size_t kFaceBytes = sizeof(uint32_t) + 10 * sizeof(double);

std::expected<size_t, Error> EncodingCapacity(const RoadPortFieldFitInput &input,
                                              bool geometry = true) {
  if ((geometry && (input.Planes.size() > kProcedureBytesMost / kPlaneBytes ||
                    input.Patches.size() > kProcedureBytesMost / kPatchBytes)) ||
      input.Priors.size() > kProcedureBytesMost / kPriorBytes ||
      input.Faces.size() > kProcedureBytesMost / kFaceBytes) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const size_t geometryBytes =
      geometry ? input.Planes.size() * kPlaneBytes + input.Patches.size() * kPatchBytes : 0;
  const size_t bytes = kHeaderBytesMost + geometryBytes + input.Priors.size() * kPriorBytes +
                       input.Faces.size() * kFaceBytes;
  if (bytes > kProcedureBytesMost) { return std::unexpected(Error::CapacityExceeded); }
  return bytes;
}

std::expected<void, Error>
Geometry(BinaryValueWriter &out, const RoadPortFieldFitInput &input, const std::stop_token &stop) {
  for (const auto &plane : input.Planes) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!out(plane.Origin.EastM, plane.Origin.NorthM)) {
      return std::unexpected(Error::CapacityExceeded);
    }
  }
  for (const auto &patch : input.Patches) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const bool blend = patch.Parents[0] != patch.Parents[1];
    if (!out(patch.Parents, blend)) { return std::unexpected(Error::CapacityExceeded); }
    if (!blend) { continue; }
    for (size_t side = 0; side < 2; ++side) {
      if (!out(patch.CutCentres[side].EastM,
               patch.CutCentres[side].NorthM,
               patch.InwardNormals[side].EastM,
               patch.InwardNormals[side].NorthM)) {
        return std::unexpected(Error::CapacityExceeded);
      }
    }
    if (!out(patch.ScaleM)) { return std::unexpected(Error::CapacityExceeded); }
  }
  return {};
}

std::expected<void, Error> Observations(BinaryValueWriter &out,
                                        const RoadPortFieldFitInput &input,
                                        const std::stop_token &stop) {
  for (const auto &prior : input.Priors) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!out(prior.Patch, prior.At.EastM, prior.At.NorthM, prior.HeightM)) {
      return std::unexpected(Error::CapacityExceeded);
    }
  }
  for (const auto &face : input.Faces) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!out(face.Patch, face.GradeFrame)) { return std::unexpected(Error::CapacityExceeded); }
    for (const auto at : face.Triangle) {
      if (!out(at.EastM, at.NorthM)) { return std::unexpected(Error::CapacityExceeded); }
    }
  }
  return {};
}
}

std::expected<RoadPortProcedure, RoadProfiles::HeightFitError>
BindRoadPortProcedure(const RoadPortFieldFitInput &input, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto capacity = EncodingCapacity(input);
  if (!capacity) { return std::unexpected(capacity.error()); }
  BinaryValueWriter out(kProcedureBytesMost);
  out.Out.Reserve(*capacity);
  if (!out.Text("outshine/native-c1-ground-field") ||
      !out(kPortFitProcedureRevision,
           static_cast<uint32_t>(kPortFitPolygonSides),
           RoadProfiles::kHeightSolverThreads,
           RoadProfiles::kHeightSolverSimplexStrategy,
           RoadProfiles::kHeightSolverRandomSeed,
           RoadProfiles::kHeightProgramTolerance,
           kPortFitVerificationTolerance,
           input.MaximumPriorChangeM,
           static_cast<uint64_t>(input.Planes.size()),
           static_cast<uint64_t>(input.Patches.size()),
           static_cast<uint64_t>(input.Priors.size()),
           static_cast<uint64_t>(input.Faces.size())) ||
      !out.Text(RoadProfiles::kHeightSolverName) ||
      !out.Text(RoadProfiles::kHeightSolverParallel) ||
      !out.Text(RoadProfiles::kHeightSolverPresolve)) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (const auto geometry = Geometry(out, input, stop); !geometry) {
    return std::unexpected(geometry.error());
  }
  if (const auto observations = Observations(out, input, stop); !observations) {
    return std::unexpected(observations.error());
  }
  const char *const version = Highs_version();
  const char *const revision = Highs_githash();
  if (version == nullptr || revision == nullptr) { return std::unexpected(Error::SolverFailure); }
  return RoadPortProcedure{
      .Input = {.DatasetId = "outshine/native-c1-ground-field",
                .Version = std::to_string(kPortFitProcedureRevision),
                .Revision = std::string(version) + "+" + revision,
                .ItemId = "ordered-minimax-lp/" + std::to_string(kPortFitPolygonSides),
                .Content = Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size())},
      .Bytes = out.Out.Bytes().size()};
}

std::expected<RoadPortProcedure, RoadProfiles::HeightFitError>
BindRoadGroundAffineProcedure(const RoadPortFieldFitInput &input, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto capacity = EncodingCapacity(input, false);
  if (!capacity) { return std::unexpected(capacity.error()); }
  BinaryValueWriter out(kProcedureBytesMost);
  out.Out.Reserve(*capacity);
  if (!out.Text("outshine/native-affine-ground-field") ||
      !out(kRoadGroundAffineRevision,
           static_cast<uint64_t>(kRoadGroundAffineRoundingRounds),
           input.MaximumPriorChangeM,
           static_cast<uint64_t>(input.Patches.size()),
           static_cast<uint64_t>(input.Priors.size()),
           static_cast<uint64_t>(input.Faces.size()))) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (const auto observations = Observations(out, input, stop); !observations) {
    return std::unexpected(observations.error());
  }
  return RoadPortProcedure{
      .Input = {.DatasetId = "outshine/native-affine-ground-field",
                .Version = std::to_string(kRoadGroundAffineRevision),
                .Revision = "streaming-givens-qr/common-grade-scaling",
                .ItemId = "fixed-gradient-minimax-intercept",
                .Content = Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size())},
      .Bytes = out.Out.Bytes().size()};
}
}
