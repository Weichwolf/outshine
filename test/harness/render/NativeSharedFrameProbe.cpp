#include "NativeGpuCheck.h"
#include "Check.h"
#include "GpuPlacement.h"
#include "TangentFrame.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <optional>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
using Json = nlohmann::json;

struct alignas(16) Point {
  std::array<float, 4> Position{};
  std::array<float, 4> Shift{};
};

static_assert(sizeof(Point) == 32 && alignof(Point) == 16 && offsetof(Point, Shift) == 16);

struct Group {
  std::string Name;
  bool Shared = false;
  size_t First = 0;
  size_t Count = 0;
  double CpuGap = 0;
};

struct Inputs {
  std::vector<GpuPlacement> Placements;
  std::vector<Point> Points;
  std::vector<Vec3> Reference;
  std::vector<Group> Groups;
};

LongitudeLatitude Anchor(const Json &value) {
  return {.LongitudeDeg = value[0].get<double>(), .LatitudeDeg = value[1].get<double>()};
}

Vec3 Vector(const Json &value) {
  return {{value[0].get<double>(), value[1].get<double>(), value[2].get<double>()}};
}

GpuPlacement Placement(const TangentFrame &frame) {
  GpuPlacement placement;
  for (size_t axis = 0; axis < 3; ++axis) {
    placement.Current[axis] = static_cast<float>(frame.EastEcef()[axis]);
    placement.Current[4 + axis] = static_cast<float>(frame.UpEcef()[axis]);
    placement.Current[8 + axis] = static_cast<float>(-frame.NorthEcef()[axis]);
  }
  placement.Current[15] = 1;
  placement.Previous = placement.Current;
  return placement;
}

std::vector<Vec3> AppendProduct(
    Inputs &into, const Json &source, const TangentFrame &frame, const Vec3 &camera, bool shared) {
  const auto rotation = Placement(frame);
  std::vector<Vec3> reconstructed;
  for (size_t at = 0; at < source["ids"].size(); ++at) {
    const auto ecef = Vector(source["ecef"][at]);
    EastNorthUp local;
    if (shared) {
      local = frame.ToLocalPosition(ecef);
    } else {
      const auto position = Vector(source["local_boundary"][at]);
      local = {.EastM = position[0], .NorthM = position[1], .UpM = position[2]};
    }
    Point point{.Position = {static_cast<float>(local.EastM),
                             static_cast<float>(local.UpM),
                             static_cast<float>(-local.NorthM),
                             1},
                .Shift = {}};
    for (size_t axis = 0; axis < 3; ++axis) {
      point.Shift[axis] = static_cast<float>(frame.OriginEcef()[axis] - camera[axis]);
    }
    into.Points.push_back(point);
    into.Placements.push_back(rotation);
    into.Reference.push_back(ecef - camera);
    reconstructed.push_back(frame.ToEcefPosition({.EastM = point.Position[0],
                                                  .NorthM = -static_cast<double>(point.Position[2]),
                                                  .UpM = point.Position[1]}));
  }
  return reconstructed;
}

Inputs Prepare(const char *path) {
  std::ifstream input(path);
  CHECK(input, "the original shared-boundary input is readable");
  if (!input) { Failed(); }
  const auto fixture = Json::parse(input, nullptr, false);
  CHECK(!fixture.is_discarded(), "the paired reference metadata decodes");
  if (fixture.is_discarded()) { Failed(); }
  Inputs into;
  for (const auto &item : fixture["cases"]) {
    const auto canonical = TangentFrame::At(Anchor(item["common_anchor"]));
    const auto camera = canonical.ToEcefPosition({.EastM = 100, .NorthM = 50, .UpM = 20});
    const size_t count = item["sides"][0]["ids"].size();
    CHECK(item["sides"][0]["ids"] == item["sides"][1]["ids"] &&
              item["sides"][0]["version"] == item["sides"][1]["version"] &&
              item["sides"][0]["ecef"] == item["sides"][1]["ecef"],
          "both products retain the same canonical IDs, version and exact physical boundary");
    for (const bool shared : {false, true}) {
      Group group{.Name = item["name"].get<std::string>(),
                  .Shared = shared,
                  .First = into.Points.size(),
                  .Count = count,
                  .CpuGap = 0};
      std::array<std::vector<Vec3>, 2> reconstructed;
      for (size_t side = 0; side < 2; ++side) {
        const auto &source = item["sides"][side];
        const auto frame = shared ? canonical : TangentFrame::At(Anchor(source["anchor"]));
        reconstructed[side] = AppendProduct(into, source, frame, camera, shared);
      }
      for (size_t at = 0; at < count; ++at) {
        group.CpuGap = std::max(group.CpuGap, Length(reconstructed[0][at] - reconstructed[1][at]));
      }
      into.Groups.push_back(group);
    }
  }
  return into;
}

std::unique_ptr<Buffer> UploadBytes(Device &device,
                                    Commands &commands,
                                    std::span<const std::byte> bytes,
                                    std::vector<std::unique_ptr<Buffer>> &staging) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto target = Require(device.CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::Storage | BufferUse::CopyDestination}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device.Unmap(*upload));
  Require(commands.Copy(*upload, *target, {.Bytes = bytes.size()}));
  staging.push_back(std::move(upload));
  return target;
}

std::vector<std::array<float, 4>>
Capture(Device &device, const Inputs &input, const char *vertexPath, const char *fragmentPath) {
  const auto vertex = Spirv(vertexPath);
  const auto fragment = Spirv(fragmentPath);
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto graphics =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Streams = {},
                                             .Attributes = {},
                                             .Colours = targets,
                                             .Depth = std::nullopt,
                                             .Culling = Cull::None,
                                             .Label = {}}));
  CHECK(input.Points.size() <= std::numeric_limits<uint32_t>::max(),
        "the finite diagnostic fits native addressing");
  if (input.Points.empty() || input.Points.size() > std::numeric_limits<uint32_t>::max()) {
    Failed();
  }
  const auto width = static_cast<uint32_t>(input.Points.size());
  const uint64_t rowBytes = (uint64_t{width} * 16 + 255) / 256 * 256;
  auto image =
      Require(device.CreateTexture({.Width = width,
                                    .Height = 1,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = rowBytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  std::vector<std::unique_ptr<Buffer>> staging;
  auto placement =
      UploadBytes(device, *commands, std::as_bytes(std::span(input.Placements)), staging);
  auto points = UploadBytes(device, *commands, std::as_bytes(std::span(input.Points)), staging);
  constexpr size_t viewWords = 3 * 16 + 2 * 4;
  std::array<float, viewWords> view{};
  view[0] = view[5] = view[10] = view[15] = 1;
  const std::array bindings{
      Storage(0, 0, *placement),
      Storage(0, 1, *points),
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(view))}}};
  const std::array colours{ColourAttachment{.Image = {.Image = image.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = graphics.get(),
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = static_cast<float>(width), .Height = 1},
                                    .Clip = {.Width = width, .Height = 1}},
                          .Range = {.Count = 6, .Instances = width}}));
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *image,
      *readback,
      {.BufferOffset = 0, .RowBytes = rowBytes, .Region = {.Width = width, .Height = 1}}));
  const auto token = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(token, std::chrono::seconds(10))) == CompletionStatus::Ready,
        "the bounded diagnostic vertex output completes");
  const auto bytes = Require(device.MapRead(*readback, {.Bytes = rowBytes}));
  std::vector<std::array<float, 4>> result(width);
  std::memcpy(result.data(), bytes.data(), result.size() * sizeof(result[0]));
  Require(device.Unmap(*readback));
  return result;
}

void Compare(const Inputs &input,
             const std::vector<std::array<float, 4>> &output,
             const std::string &device,
             const char *path) {
  Json rows = Json::array();
  bool cpuControl = false;
  for (const auto &group : input.Groups) {
    size_t mismatched = 0;
    double delta = 0;
    double error = 0;
    for (size_t at = 0; at < group.Count; ++at) {
      const auto &a = output[group.First + at];
      const auto &b = output[group.First + group.Count + at];
      mismatched += static_cast<size_t>(std::bit_cast<std::array<uint32_t, 4>>(a) !=
                                        std::bit_cast<std::array<uint32_t, 4>>(b));
      const Vec3 difference{{static_cast<double>(a[0]) - b[0],
                             static_cast<double>(a[1]) - b[1],
                             static_cast<double>(a[2]) - b[2]}};
      delta = std::max(delta, Length(difference));
      for (size_t side = 0; side < 2; ++side) {
        const auto index = group.First + side * group.Count + at;
        const auto &p = output[index];
        CHECK(std::ranges::all_of(p, [](float x) { return std::isfinite(x); }) && p[3] == 1,
              "the raw diagnostic position is finite with preserved homogeneous weight");
        error = std::max(error, Length(Vec3{{p[0], p[1], p[2]}} - input.Reference[index]));
      }
    }
    if (group.Shared) {
      CHECK(mismatched == 0 && delta == 0,
            "a shared canonical render frame keeps original paired boundary words identical on the "
            "actual GPU");
    } else {
      cpuControl = cpuControl || group.CpuGap > 0;
    }
    rows.push_back({{"case", group.Name},
                    {"common_render_frame", group.Shared},
                    {"pairs", group.Count},
                    {"cpu_local_f32_ecef_gap_m", group.CpuGap},
                    {"gpu_paired_word_mismatches", mismatched},
                    {"gpu_paired_relative_gap_m", delta},
                    {"gpu_max_physical_position_error_m", error}});
  }
  CHECK(cpuControl,
        "the independent-frame control exercises an original pre-GPU narrowing difference");
  std::ofstream raw(std::string(path) + ".f32", std::ios::binary);
  raw.write(reinterpret_cast<const char *>(output.data()),
            static_cast<std::streamsize>(output.size() * sizeof(output[0])));
  CHECK(raw.good(), "original GPU float words are retained separately from the report");
  std::ofstream report(path);
  report << Json{{"device", device},
                 {"vertex_records", input.Points.size()},
                 {"rows", rows},
                 {"scope",
                  "diagnostic native vertex arithmetic and flat RGBA32F readback; existing "
                  "placement/shift/viewProj expression; not an unmodified product PSO, world "
                  "raster/LOD/normal/temporal/shadow or device-budget certification"}}
                .dump(2)
         << '\n';
  CHECK(report.good(), "the original native GPU result is saved");
}
}

int main(int argc, char **argv) {
  if (argc != 6) { return 2; }
  auto device = Require(CreateDevice({.Validation = true}));
  CHECK(device->Limits().Name == argv[5], "the hardware proof uses the explicitly declared device");
  const auto input = Prepare(argv[1]);
  const auto output = Capture(*device, input, argv[2], argv[3]);
  Compare(input, output, device->Limits().Name, argv[4]);
  return Report();
}
