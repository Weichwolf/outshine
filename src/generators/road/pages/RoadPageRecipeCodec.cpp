#include "RoadPageRecipeCodec.h"
#include "RoadPageAsset.h"
#include "Sha256.h"
#include "BudgetedBinaryValueReader.h"
#include "RoadValueFields.h"
#include "HeightSolid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Reader = BudgetedBinaryValueReader<RoadPageRecipeCodecError>;
using Bodies = std::vector<std::shared_ptr<const HeightSolid>>;
constexpr uint64_t kFormat = 0x000131504352ULL;
constexpr uint32_t kNoBody = std::numeric_limits<uint32_t>::max();
constexpr size_t kIntervalWireBytes = 54;
constexpr size_t kJunctionWireBytes = 44;
constexpr size_t kBodyWireBytes = 32;

Bodies BodyTable(const RoadPageRecipe &recipe) {
  Bodies bodies;
  for (const auto &interval : recipe.Intervals) {
    if (interval.Body && std::ranges::find(bodies, interval.Body) == bodies.end()) {
      bodies.push_back(interval.Body);
    }
  }
  return bodies;
}

template <class Archive, class Sweep> bool SweepScalars(Archive &archive, Sweep &sweep) {
  return archive(
      sweep.HalfWidthM, sweep.Profile, sweep.WearsLinear.Axis, sweep.Crossfall, sweep.Form);
}

template <class Archive, class Port> bool PortFields(Archive &archive, Port &port) {
  return RoadValueFields::Gate(archive, port.Gate) && archive(port.Plane.SlopeE, port.Plane.SlopeN);
}

bool SweepFields(BinaryValueWriter &out, const RoadSweep &sweep) {
  if (!SweepScalars(out, sweep)) { return false; }
  return std::ranges::all_of(sweep.EndPorts, [&](const auto &port) {
    return out(port.has_value()) && (!port || PortFields(out, *port));
  });
}

bool SweepFields(Reader &in, RoadSweep &sweep) {
  if (!SweepScalars(in, sweep)) { return false; }
  for (auto &port : sweep.EndPorts) {
    bool present = false;
    if (!in(present)) { return false; }
    if (present) {
      port.emplace();
      if (!PortFields(in, *port)) { return false; }
    }
  }
  return true;
}

bool WriteInterval(BinaryValueWriter &out, const RoadPageInterval &interval, const Bodies &bodies) {
  const auto body =
      interval.Body
          ? static_cast<uint32_t>(std::ranges::find(bodies, interval.Body) - bodies.begin())
          : kNoBody;
  return out(interval.Edge, interval.Lane) &&
         RoadValueFields::WriteList(
             out,
             std::span(interval.Stations),
             [&](const auto &station) { return RoadValueFields::Station(out, station); }) &&
         SweepFields(out, interval.Surface) && out(body);
}

bool ReadInterval(Reader &in, RoadPageInterval &interval, const Bodies &bodies) {
  uint32_t body = 0;
  if (!in(interval.Edge, interval.Lane) ||
      !in.List(interval.Stations,
               3 * sizeof(double) + sizeof(uint64_t),
               [&](auto &station) { return RoadValueFields::Station(in, station); }) ||
      !SweepFields(in, interval.Surface) || !in(body)) {
    return false;
  }
  if (body != kNoBody) {
    if (body >= bodies.size()) { return false; }
    interval.Body = bodies[body];
  }
  return true;
}

bool JunctionFields(BinaryValueWriter &out, const RoadPageJunction &junction) {
  return out(junction.Index, junction.Node) &&
         RoadValueFields::WriteList(
             out,
             std::span(junction.Gates),
             [&](const auto &gate) { return RoadValueFields::Gate(out, gate); }) &&
         out(junction.Plane.SlopeE, junction.Plane.SlopeN, junction.WearsLinear.Axis);
}

bool JunctionFields(Reader &in, RoadPageJunction &junction) {
  return in(junction.Index, junction.Node) &&
         in.List(junction.Gates,
                 8 * sizeof(double),
                 [&](auto &gate) { return RoadValueFields::Gate(in, gate); }) &&
         in(junction.Plane.SlopeE, junction.Plane.SlopeN, junction.WearsLinear.Axis);
}

bool ReadBody(Reader &in, std::shared_ptr<const HeightSolid> &body) {
  if (!in.Charge(sizeof(HeightSolid))) { return false; }
  auto value = std::make_shared<HeightSolid>();
  if (!RoadValueFields::Body(in, *value)) { return false; }
  body = std::move(value);
  return true;
}

bool ValidGate(const RoadGate &gate) {
  return std::ranges::all_of(std::array{gate.EastM,
                                        gate.NorthM,
                                        gate.GradeM,
                                        gate.OutE,
                                        gate.OutN,
                                        gate.HalfWidthM,
                                        gate.ShoulderM,
                                        gate.ThicknessM},
                             [](double v) { return std::isfinite(v); });
}

bool ValidSurface(const RoadSweep &surface) {
  return std::isfinite(surface.HalfWidthM) && std::isfinite(surface.Crossfall) &&
         (surface.Profile == RoadProfile::Rounded || surface.Profile == RoadProfile::Simple ||
          surface.Profile == RoadProfile::Kerbed) &&
         (surface.Form == RibbonForm::Surface || surface.Form == RibbonForm::ClosedShell) &&
         std::ranges::all_of(surface.WearsLinear, [](float v) { return std::isfinite(v); }) &&
         std::ranges::all_of(surface.EndPorts, [](const auto &port) {
           return !port || (ValidGate(port->Gate) && std::isfinite(port->Plane.SlopeE) &&
                            std::isfinite(port->Plane.SlopeN));
         });
}

bool Valid(const RoadPageRecipe &recipe) {
  return std::ranges::all_of(
             recipe.Intervals,
             [](const auto &interval) {
               return ValidSurface(interval.Surface) &&
                      (!interval.Body || RoadValueFields::ValidBody(*interval.Body)) &&
                      std::ranges::all_of(interval.Stations, [](const auto &station) {
                        return std::isfinite(station.EastM) && std::isfinite(station.NorthM) &&
                               std::isfinite(station.GradeM);
                      });
             }) &&
         std::ranges::all_of(recipe.Junctions, [](const auto &junction) {
           return std::ranges::all_of(junction.Gates, ValidGate) &&
                  std::isfinite(junction.Plane.SlopeE) && std::isfinite(junction.Plane.SlopeN) &&
                  std::ranges::all_of(junction.WearsLinear,
                                      [](float v) { return std::isfinite(v); });
         });
}
}

std::string RoadPageRecipeKey(const RoadPageAddress &address) {
  auto original = address;
  original.PositionErrorM.reset();
  original.DisplayPolicy = RoadDisplayPolicy::UpperOnly;
  original.Level = 0;
  original.Clustered = false;
  const auto base = RoadPageKey(original);
  return base.empty() ? std::string{} : Sha256Hex("road-page-recipe-1\n" + base);
}

size_t RoadPageRecipeCapacityBytes(const RoadPageRecipe &recipe) {
  size_t result = sizeof(recipe) + recipe.Intervals.capacity() * sizeof(RoadPageInterval) +
                  recipe.Junctions.capacity() * sizeof(RoadPageJunction);
  for (const auto &interval : recipe.Intervals) {
    result += interval.Stations.capacity() * sizeof(RoadStation);
  }
  for (const auto &junction : recipe.Junctions) {
    result += junction.Gates.capacity() * sizeof(RoadGate);
  }
  for (const auto &body : BodyTable(recipe)) {
    result += sizeof(*body) + body->Points.capacity() * sizeof(body->Points[0]) +
              body->Faces.capacity() * sizeof(body->Faces[0]) +
              body->WallParents.capacity() * sizeof(body->WallParents[0]) +
              body->WallKinds.capacity() * sizeof(body->WallKinds[0]);
  }
  return result;
}

std::expected<size_t, RoadPageRecipeCodecError> RoadPageRecipeDecodeCapacity(size_t bytes) {
  constexpr size_t expansion = std::max(
      {size_t{1},
       (sizeof(RoadPageInterval) + kIntervalWireBytes - 1) / kIntervalWireBytes,
       (sizeof(RoadPageJunction) + kJunctionWireBytes - 1) / kJunctionWireBytes,
       (sizeof(HeightSolid) + sizeof(std::shared_ptr<const HeightSolid>) + kBodyWireBytes - 1) /
           kBodyWireBytes});
  if (bytes > (std::numeric_limits<size_t>::max() - sizeof(RoadPageRecipe)) / expansion) {
    return std::unexpected(RoadPageRecipeCodecError::CapacityExceeded);
  }
  return bytes * expansion + sizeof(RoadPageRecipe);
}

std::expected<std::vector<uint8_t>, RoadPageRecipeCodecError>
EncodeRoadPageRecipe(std::string_view key, const RoadPageRecipe &recipe, size_t bytesMost) {
  if (key.size() != 64 || !Valid(recipe)) {
    return std::unexpected(RoadPageRecipeCodecError::Malformed);
  }
  const auto bodies = BodyTable(recipe);
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat) || !out.Text(key) ||
      !RoadValueFields::WriteList(
          out,
          std::span(bodies),
          [&](const auto &body) { return RoadValueFields::Body(out, *body); }) ||
      !RoadValueFields::WriteList(
          out,
          std::span(recipe.Intervals),
          [&](const auto &interval) { return WriteInterval(out, interval, bodies); }) ||
      !RoadValueFields::WriteList(out, std::span(recipe.Junctions), [&](const auto &junction) {
        return JunctionFields(out, junction);
      })) {
    return std::unexpected(RoadPageRecipeCodecError::CapacityExceeded);
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<RoadPageRecipe, RoadPageRecipeCodecError> DecodeRoadPageRecipe(
    std::string_view key, std::span<const uint8_t> bytes, RoadPageRecipeCodecLimits limits) {
  if (bytes.size() > limits.BytesMost) {
    return std::unexpected(RoadPageRecipeCodecError::CapacityExceeded);
  }
  Reader in(bytes, limits.CapacityMost);
  uint64_t format = 0;
  if (!in(format)) { return std::unexpected(in.Failure()); }
  if (format != kFormat) { return std::unexpected(RoadPageRecipeCodecError::UnsupportedFormat); }
  std::string storedKey;
  if (!in.Text(storedKey)) { return std::unexpected(in.Failure()); }
  if (storedKey != key) { return std::unexpected(RoadPageRecipeCodecError::WrongProduct); }
  RoadPageRecipe result;
  Bodies bodies;
  if (!in.Charge(sizeof(result)) ||
      !in.List(bodies, kBodyWireBytes, [&](auto &body) { return ReadBody(in, body); }) ||
      !in.List(result.Intervals,
               kIntervalWireBytes,
               [&](auto &interval) { return ReadInterval(in, interval, bodies); }) ||
      !in.List(result.Junctions,
               kJunctionWireBytes,
               [&](auto &junction) { return JunctionFields(in, junction); }) ||
      in.RemainingBytes() != 0) {
    return std::unexpected(in.Failure());
  }
  if (!Valid(result)) { return std::unexpected(RoadPageRecipeCodecError::Malformed); }
  return result;
}
}
