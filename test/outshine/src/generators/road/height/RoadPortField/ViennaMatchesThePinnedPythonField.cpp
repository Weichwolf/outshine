#include "Check.h"
#include "Json.h"
#include "RoadPortField.h"
#include "Sha256.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <ios>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr size_t kPlanes = 1536;
constexpr size_t kModules = 3317;
constexpr size_t kVertices = 30256;
constexpr size_t kFaces = 31780;
constexpr size_t kPorts = 3888;
constexpr std::string_view kDirectory = "test/experiments/transport/reference/port-planes/native/";

std::string Read(const char *name) {
  std::ifstream input(std::string(kDirectory) + name, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct Slice {
  std::span<const char> Bytes;

  [[nodiscard]] uint64_t Word(size_t element, size_t width) const {
    uint64_t value = 0;
    for (size_t byte = 0; byte < width; ++byte) {
      value |= static_cast<uint64_t>(static_cast<uint8_t>(Bytes[element * width + byte]))
               << (8 * byte);
    }
    return value;
  }

  [[nodiscard]] double Number(size_t element) const {
    return std::bit_cast<double>(Word(element, 8));
  }

  [[nodiscard]] uint32_t Index(size_t element) const {
    return static_cast<uint32_t>(Word(element, 4));
  }

  [[nodiscard]] EastNorth Point(size_t element) const {
    return {.EastM = Number(2 * element), .NorthM = Number(2 * element + 1)};
  }
};

class Reference {
public:
  bool Load() {
    const auto metadata = Read("Vienna-pr181-fields.json");
    Bytes_ = Read("Vienna-pr181-fields.bin");
    const bool parsed = Metadata_.Parse(metadata.data(), metadata.size());
    CHECK(parsed, "the committed portable reference metadata parses");
    if (!parsed) { return false; }
    const bool bound =
        Sha256Hex(metadata) == "632d46b734814625e71212669eaf8a46d12782f73b82f4ede96c227a4eb7fec8" &&
        Bytes_.size() == 5093952 && Bytes_.starts_with("OSPTREF1") &&
        Sha256Hex(Bytes_) == "9b93a2a12bf2afc49916d2183a21cd36190f3ceeab84b69968cc69a7c506da09";
    CHECK(bound, "the complete reference is bound to the independently exported Python answers");
    return bound;
  }

  Slice Array(const char *name, size_t count, const char *type) const {
    const auto array = Metadata_.Root()["arrays"][name];
    const auto offset = static_cast<size_t>(array["offset"].Int());
    const auto bytes = static_cast<size_t>(array["bytes"].Int());
    const size_t width = std::string_view(type) == "<f8" ? 8 : 4;
    size_t elements = 1;
    const auto shape = array["shape"];
    for (size_t axis = 0; axis < shape.Size(); ++axis) {
      elements *= static_cast<size_t>(shape[axis].Int());
    }
    const bool valid = array.Valid() && array["dtype"].StrEquals(type) && elements == count &&
                       bytes == count * width && offset >= 8 && offset % 8 == 0 &&
                       offset <= Bytes_.size() && bytes <= Bytes_.size() - offset;
    CHECK(valid, "named arrays retain their declared type, extent and aligned file range");
    if (!valid) { return {}; }
    return {std::span<const char>(Bytes_).subspan(offset, bytes)};
  }

private:
  Json Metadata_;
  std::string Bytes_;
};

auto Field(const Reference &reference) {
  const auto origins = reference.Array("plane_origins", kPlanes * 2, "<f8");
  const auto coefficients = reference.Array("plane_coefficients", kPlanes * 3, "<f8");
  const auto parents = reference.Array("plane_ids", kModules * 2, "<u4");
  const auto cuts = reference.Array("cut_centres", kModules * 4, "<f8");
  const auto normals = reference.Array("inward_normals", kModules * 4, "<f8");
  const auto scales = reference.Array("scales", kModules, "<f8");
  std::vector<RoadPortField::Plane> planes;
  std::vector<RoadPortField::Patch> patches;
  planes.reserve(kPlanes);
  patches.reserve(kModules);
  for (size_t index = 0; index < kPlanes; ++index) {
    planes.push_back({.Origin = origins.Point(index),
                      .HeightM = coefficients.Number(3 * index),
                      .Gradient = {.SlopeE = coefficients.Number(3 * index + 1),
                                   .SlopeN = coefficients.Number(3 * index + 2)}});
  }
  for (size_t index = 0; index < kModules; ++index) {
    patches.push_back({.Parents = {parents.Index(2 * index), parents.Index(2 * index + 1)},
                       .CutCentres = {cuts.Point(2 * index), cuts.Point(2 * index + 1)},
                       .InwardNormals = {normals.Point(2 * index), normals.Point(2 * index + 1)},
                       .ScaleM = scales.Number(index)});
  }
  return RoadPortField::Make(planes, patches);
}

void CompareSample(RoadPortField::Sample sample,
                   size_t offset,
                   const Slice &expected,
                   double datum,
                   std::array<double, 3> &errors) {
  const std::array actual{sample.HeightM + datum, sample.Gradient.SlopeE, sample.Gradient.SlopeN};
  for (size_t channel = 0; channel < actual.size(); ++channel) {
    CHECK(std::isfinite(actual[channel]), "native field answers remain finite throughout the plan");
    errors[channel] =
        std::max(errors[channel], std::abs(actual[channel] - expected.Number(offset + channel)));
  }
}

void CheckFaces(const Reference &reference, const RoadPortField &field) {
  const auto vertices = reference.Array("road_xy", kVertices * 2, "<f8");
  const auto faces = reference.Array("source_faces", kFaces * 3, "<u4");
  const auto ids = reference.Array("source_face_ids", kFaces, "<u4");
  const auto modules = reference.Array("face_parts", kFaces, "<u4");
  const auto points = reference.Array("face_points", kFaces * 2, "<f8");
  const auto values = reference.Array("face_values", kFaces * 3, "<f8");
  const auto frames = reference.Array("grade_frames", kFaces * 4, "<f8");
  const auto bounds = reference.Array("face_bounds", kFaces * 2, "<f8");
  const double datum = reference.Array("datum_m", 1, "<f8").Number(0);
  std::array<double, 3> errors{};
  std::array<double, 2> boundErrors{};
  std::array<bool, kModules> covered{};
  for (size_t index = 0; index < kFaces; ++index) {
    CHECK(ids.Index(index) == index && modules.Index(index) < kModules,
          "original face and owner identities are preserved");
    const size_t owner = modules.Index(index);
    if (owner >= kModules) { return; }
    covered[owner] = true;
    const auto sampled = field.Evaluate(owner, points.Point(index));
    CHECK(sampled.has_value(), "every original face centroid is in the analytic field domain");
    if (!sampled) { return; }
    CompareSample(*sampled, 3 * index, values, datum, errors);
    std::array<EastNorth, 3> triangle;
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto vertex = faces.Index(3 * index + corner);
      CHECK(vertex < kVertices, "original triangles reference original vertices");
      if (vertex >= kVertices) { return; }
      triangle[corner] = vertices.Point(vertex);
    }
    std::array<double, 4> frame;
    for (size_t at = 0; at < frame.size(); ++at) { frame[at] = frames.Number(4 * index + at); }
    const auto bounded = field.Bounds(owner, triangle, frame);
    CHECK(bounded.has_value(), "every entire original triangle has a finite certificate");
    if (!bounded) { return; }
    const std::array actual{bounded->NormalizedGradientMaximum, bounded->HeightChordErrorM};
    for (size_t channel = 0; channel < actual.size(); ++channel) {
      CHECK(std::isfinite(actual[channel]), "native whole-face certificates remain finite");
      const double expected = bounds.Number(2 * index + channel);
      boundErrors[channel] = std::max(
          boundErrors[channel], std::abs(actual[channel] - expected) / (1 + std::abs(expected)));
    }
  }
  CHECK(std::ranges::all_of(covered, [](bool value) { return value; }),
        "all 3317 original modules participate in the native comparison");
  CHECK_NEAR(errors[0], 0, 1e-8, "m", "all absolute face heights match Python");
  for (size_t axis = 1; axis < errors.size(); ++axis) {
    CHECK_NEAR(errors[axis], 0, 1e-10, "m/m", "all face XY gradients match Python");
  }
  for (const double error : boundErrors) {
    CHECK_NEAR(error, 0, 1e-9, "relative/absolute", "whole-face Bernstein bounds match Python");
  }
}

void CheckPorts(const Reference &reference, const RoadPortField &field) {
  const auto ids = reference.Array("port_ids", kPorts, "<u4");
  const auto modules = reference.Array("port_parts", kPorts * 2, "<u4");
  const auto points = reference.Array("port_points", kPorts * 3 * 2, "<f8");
  const auto values = reference.Array("port_values", kPorts * 2 * 3 * 3, "<f8");
  const double datum = reference.Array("datum_m", 1, "<f8").Number(0);
  std::array<double, 3> errors{};
  std::array<double, 3> jumps{};
  for (size_t index = 0; index < kPorts; ++index) {
    CHECK(ids.Index(index) == index, "every original whole-port identity is preserved");
    for (size_t point = 0; point < 3; ++point) {
      std::array<RoadPortField::Sample, 2> samples;
      for (size_t side = 0; side < 2; ++side) {
        const auto sampled =
            field.Evaluate(modules.Index(2 * index + side), points.Point(3 * index + point));
        CHECK(sampled.has_value(),
              "both modules evaluate at each whole-port endpoint and midpoint");
        if (!sampled) { return; }
        samples[side] = *sampled;
        CompareSample(*sampled, ((index * 2 + side) * 3 + point) * 3, values, datum, errors);
      }
      const std::array left{
          samples[0].HeightM, samples[0].Gradient.SlopeE, samples[0].Gradient.SlopeN};
      const std::array right{
          samples[1].HeightM, samples[1].Gradient.SlopeE, samples[1].Gradient.SlopeN};
      for (size_t channel = 0; channel < left.size(); ++channel) {
        jumps[channel] = std::max(jumps[channel], std::abs(left[channel] - right[channel]));
      }
    }
  }
  CHECK_NEAR(errors[0], 0, 1e-8, "m", "all absolute port-side heights match Python");
  CHECK_NEAR(jumps[0], 0, 1e-7, "m", "whole-port analytic heights meet across modules");
  for (size_t axis = 1; axis < errors.size(); ++axis) {
    CHECK_NEAR(errors[axis], 0, 1e-10, "m/m", "all port-side gradients match Python");
    CHECK_NEAR(jumps[axis], 0, 1e-10, "m/m", "whole-port analytic gradients meet across modules");
  }
}
}

int main() {
  Reference reference;
  if (!reference.Load()) { return Report(); }
  const auto field = Field(reference);
  CHECK(field.has_value(), "the complete pinned Python plan is accepted by the native field");
  if (field) {
    CheckFaces(reference, *field);
    CheckPorts(reference, *field);
  }
  return Report();
}
