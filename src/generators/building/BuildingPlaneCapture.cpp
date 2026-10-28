#include "BuildingPlaneCapture.h"
#include "ImpostorViewFrame.h"
#include "PlaneSurfacePatches.h"
#include "Box.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Capture = BuildingPlaneCapture;

bool Finite(const Vec3 &point) {
  return std::ranges::all_of(point, [](double value) { return std::isfinite(value); });
}

std::optional<ImpostorViewFrame> CaptureFrame(const Capture::View &view) {
  const auto standard = ImpostorViewFrame::From(view.TowardEye);
  if (!standard || !view.Frame) { return standard; }
  const auto &frame = *view.Frame;
  constexpr double tolerance = 1e-12;
  if (!Finite(frame.Right) || !Finite(frame.Up) ||
      std::abs(Dot(frame.Right, frame.Right) - 1) > tolerance ||
      std::abs(Dot(frame.Up, frame.Up) - 1) > tolerance ||
      std::abs(Dot(frame.Right, view.TowardEye)) > tolerance ||
      Dot(Cross(view.TowardEye, frame.Right), frame.Up) < 1 - tolerance) {
    return std::nullopt;
  }
  return frame;
}

struct Work {
  std::span<const BuildingSurface *const> Sources;
  const PlanHierarchy &Hierarchy;
  const Capture::View &View;
  Capture::Budget Budget;
  ImpostorViewFrame Frame;
  Capture::Result Output;
  std::optional<Capture::Error> Failure;
  std::vector<double> Cuts;
  std::stop_token Stopping;

  [[nodiscard]] Vec3 Origin(double x, double y) const {
    const double step = 2 * View.HalfExtentM / View.Pixels;
    return View.Centre + View.TowardEye * (3 * View.HalfExtentM) +
           Frame.Right * (x * step - View.HalfExtentM) + Frame.Up * (View.HalfExtentM - y * step);
  }

  PlaneSurfaceSample Sample(const Vec3 &origin) {
    const Vec3 direction = View.TowardEye * -1.0;
    uint32_t owner = 0;
    BuildingSurface::Hit nearest;
    const auto intersect = [&](uint32_t index, double maximum) -> std::optional<double> {
      if (Failure) { return std::nullopt; }
      if (Stopping.stop_requested()) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::Cancelled, .Source = index};
        return std::nullopt;
      }
      if (Output.SourceTests >= Budget.SourceTests) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::WorkBudgetExceeded, .Source = index};
        return std::nullopt;
      }
      ++Output.SourceTests;
      const auto &source = *Sources[index];
      if (!source.SupportsProjection()) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::UnsupportedSource, .Source = index};
        return std::nullopt;
      }
      if (!source.RequireShapes()) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::SourceUnavailable, .Source = index};
        return std::nullopt;
      }
      if (Stopping.stop_requested()) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::Cancelled, .Source = index};
        return std::nullopt;
      }
      if (source.FaceCount() > std::numeric_limits<uint32_t>::max() / 2) {
        Failure = Capture::Error{.Kind = Capture::ErrorKind::WorkBudgetExceeded, .Source = index};
        return std::nullopt;
      }
      const auto hit =
          source.Trace({.Origin = origin, .Direction = direction}, View.HalfExtentM, maximum, Cuts);
      if (!hit || Dot(hit->Normal, direction) >= 0) { return std::nullopt; }
      owner = index;
      nearest = *hit;
      return hit->Along;
    };
    ++Output.Rays;
    const auto closest = Hierarchy.TraceClosest(
        origin, direction, View.HalfExtentM, 5 * View.HalfExtentM, intersect);
    if (!closest || Failure) { return {}; }
    ++Output.Covered;
    const auto face = static_cast<uint32_t>(Sources[owner]->FaceIndex(nearest));
    const uint64_t identity = (static_cast<uint64_t>(owner) << 32U) |
                              (static_cast<uint64_t>(face) * 2 + (nearest.Gable ? 1U : 0U));
    return {.Surface = identity + 1,
            .Point = origin + direction * nearest.Along,
            .Normal = nearest.Normal};
  }

  [[nodiscard]] std::optional<BuildingSurfacePatch>
  Patch(const PlaneSurfaceSample &sample, const PlaneSurfacePatch &rectangle) const {
    const uint64_t identity = sample.Surface - 1;
    const auto owner = static_cast<uint32_t>(identity >> 32U);
    const auto face = Sources[owner]->FaceAt(static_cast<uint32_t>(identity) >> 1U);
    BuildingSurfacePatch patch{.Sample = {.Normal = sample.Normal,
                                          .Part = face.Part,
                                          .Face = face.Side,
                                          .Gable = (identity & 1U) != 0},
                               .Corners = {}};
    const std::array origins{Origin(rectangle.Left, rectangle.Top),
                             Origin(rectangle.Right, rectangle.Top),
                             Origin(rectangle.Right, rectangle.Bottom),
                             Origin(rectangle.Left, rectangle.Bottom)};
    const Vec3 direction = View.TowardEye * -1.0;
    const double denominator = Dot(direction, sample.Normal);
    if (denominator == 0 || !std::isfinite(denominator)) { return std::nullopt; }
    for (size_t at = 0; at < origins.size(); ++at) {
      const double along = Dot(sample.Point - origins[at], sample.Normal) / denominator;
      patch.Corners[at] = origins[at] + direction * along;
      if (!Finite(patch.Corners[at]) ||
          std::ranges::any_of(patch.Corners[at], [](double coordinate) {
            return std::abs(coordinate) > std::numeric_limits<float>::max();
          })) {
        return std::nullopt;
      }
    }
    return patch;
  }

  std::expected<Capture::Result, Capture::Error> Run() {
    const size_t count = static_cast<size_t>(View.Pixels) * View.Pixels;
    std::vector<PlaneSurfaceSample> samples(count);
    for (uint32_t y = 0; y < View.Pixels; ++y) {
      if (Stopping.stop_requested()) {
        return std::unexpected(Capture::Error{.Kind = Capture::ErrorKind::Cancelled});
      }
      for (uint32_t x = 0; x < View.Pixels; ++x) {
        samples[static_cast<size_t>(y) * View.Pixels + x] = Sample(Origin(x + .5, y + .5));
        if (Failure) { return std::unexpected(*Failure); }
      }
    }
    const auto rectangles = BuildPlaneSurfacePatches(samples, View.Pixels, View.Pixels);
    if (!rectangles) {
      return std::unexpected(Capture::Error{.Kind = Capture::ErrorKind::InvalidInput});
    }
    std::vector<uint8_t> used(Sources.size());
    for (const auto &rectangle : *rectangles) {
      if (Stopping.stop_requested()) {
        return std::unexpected(Capture::Error{.Kind = Capture::ErrorKind::Cancelled});
      }
      const auto &sample = samples[rectangle.Sample];
      const auto owner = static_cast<uint32_t>((sample.Surface - 1) >> 32U);
      const auto patch = Patch(sample, rectangle);
      if (!patch) {
        return std::unexpected(
            Capture::Error{.Kind = Capture::ErrorKind::SourceUnavailable, .Source = owner});
      }
      Sources[owner]->MeshPatches(std::span(&*patch, 1), Output.Mesh);
      if (used[owner] == 0) {
        Output.Sources.push_back(owner);
        used[owner] = 1;
      }
    }
    return std::move(Output);
  }
};
}

std::expected<BuildingPlaneCapture, BuildingPlaneCapture::Error>
BuildingPlaneCapture::Prepare(std::span<const BuildingSurface *const> sources) {
  if (sources.empty() || sources.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error{.Kind = ErrorKind::InvalidInput});
  }
  std::vector<Box> bounds;
  bounds.reserve(sources.size());
  for (uint32_t at = 0; at < sources.size(); ++at) {
    if (sources[at] == nullptr) {
      return std::unexpected(Error{.Kind = ErrorKind::InvalidInput, .Source = at});
    }
    const auto &sourceBounds = sources[at]->Bounds();
    if (sourceBounds.Empty() || !Finite(sourceBounds.Min) || !Finite(sourceBounds.Max)) {
      return std::unexpected(Error{.Kind = ErrorKind::InvalidInput, .Source = at});
    }
    bounds.push_back(sourceBounds);
  }
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(Error{.Kind = ErrorKind::InvalidInput}); }
  return BuildingPlaneCapture({sources.begin(), sources.end()}, std::move(*hierarchy));
}

std::expected<BuildingPlaneCapture::Result, BuildingPlaneCapture::Error>
BuildingPlaneCapture::Capture(const View &view, Budget budget, std::stop_token stopping) const {
  if (stopping.stop_requested()) { return std::unexpected(Error{.Kind = ErrorKind::Cancelled}); }
  const auto frame = CaptureFrame(view);
  if (!frame || !Finite(view.Centre) || !std::isfinite(view.HalfExtentM) || view.HalfExtentM <= 0 ||
      view.HalfExtentM > std::numeric_limits<double>::max() / 5 || view.Pixels == 0) {
    return std::unexpected(Error{.Kind = ErrorKind::InvalidInput});
  }
  const uint64_t samples = static_cast<uint64_t>(view.Pixels) * view.Pixels;
  if (samples > budget.Samples || samples > std::numeric_limits<uint32_t>::max() / 4) {
    return std::unexpected(Error{.Kind = ErrorKind::WorkBudgetExceeded});
  }
  Work work{.Sources = Sources_,
            .Hierarchy = Hierarchy_,
            .View = view,
            .Budget = budget,
            .Frame = *frame,
            .Output = {},
            .Failure = {},
            .Cuts = {},
            .Stopping = std::move(stopping)};
  if (!Finite(work.Origin(0, 0)) || !Finite(work.Origin(view.Pixels, view.Pixels))) {
    return std::unexpected(Error{.Kind = ErrorKind::InvalidInput});
  }
  return work.Run();
}
}
