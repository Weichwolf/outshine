#include "RenderAsset.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <span>
#include <system_error>
#include <expected>
#include <numbers>
#include <optional>
#include <print>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <Outshine.h>
#include <import/GltfImporter.h>
#include "format/Number.h"

namespace outshine::Client {
namespace {
namespace Says {
constexpr auto InvalidLighting = "lighting must be auto, authored or studio";
constexpr auto UnknownOption = "unknown render option: ";
constexpr auto DuplicateOption = "duplicate render option: ";
constexpr auto Usage =
    "render <asset.gltf|asset.glb> <width>x<height> <output.png> [--camera auto|index] [--time "
    "seconds] [--animation index] [--variant name] [--position x,y,z] [--look-at x,y,z] [--fov "
    "degrees] [--lighting auto|authored|studio] [--exposure multiplier]";
constexpr auto InvalidExtent = "resolution must contain two integers in [1,4096], separated by x";
constexpr auto InvalidIndex = "camera and animation indices must be nonnegative integers";
constexpr auto InvalidNumber = "option requires a finite number in its declared range";
constexpr auto InvalidVector =
    "camera vectors require three finite comma-separated coordinates in metres";
constexpr auto CameraUnavailable = "selected camera has no unambiguous valid placement";
constexpr auto CannotFrame = "asset has no finite nonzero bounds for automatic framing";
constexpr auto InvalidAim =
    "--position and --look-at must be supplied together with distinct points";
}

struct RenderOption {
  std::string_view Name;
  std::string_view Value;
};

enum class CameraMode { Default, Automatic, Indexed };
enum class LightingMode { Automatic, Authored, Studio };

struct AssetRenderOptions {
  std::string_view Asset;
  std::string_view Output;
  Extent Frame;
  CameraMode Camera = CameraMode::Default;
  int CameraIndex = 0;
  LightingMode Lighting = LightingMode::Automatic;
  double TimeS = 0;
  std::optional<int> Animation;
  std::string_view Variant;
  std::optional<Vec3> Position;
  std::optional<Vec3> Target;
  std::optional<double> FovDeg;
  double Exposure = 1;
};

[[nodiscard]] std::optional<int> ParseIndex(std::string_view value) {
  int index = 0;
  const auto parsed = std::from_chars(value.data(), value.data() + value.size(), index);
  if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || index < 0) {
    return std::nullopt;
  }
  return index;
}

[[nodiscard]] std::optional<Vec3> ParseVector3(std::string_view value) {
  Vec3 vector;
  for (size_t axis = 0; axis < 3; ++axis) {
    const size_t comma = value.find(',');
    if ((axis < 2) == (comma == std::string_view::npos)) { return std::nullopt; }
    const auto number = ParseFiniteNumber(value.substr(0, comma));
    if (!number) { return std::nullopt; }
    vector[axis] = *number;
    if (comma != std::string_view::npos) { value.remove_prefix(comma + 1); }
  }
  return vector;
}

[[nodiscard]] Result SetCameraOption(AssetRenderOptions &options, RenderOption option) {
  const auto [name, value] = option;
  if (name == "--camera") {
    if (value == "auto") {
      options.Camera = CameraMode::Automatic;
      return {};
    }
    const auto index = ParseIndex(value);
    if (!index) { return std::unexpected(Says::InvalidIndex); }
    options.Camera = CameraMode::Indexed;
    options.CameraIndex = *index;
    return {};
  }
  if (name == "--position" || name == "--look-at") {
    const auto vector = ParseVector3(value);
    if (!vector) { return std::unexpected(Says::InvalidVector); }
    (name == "--position" ? options.Position : options.Target) = *vector;
    return {};
  }
  const auto number = ParseFiniteNumber(value);
  constexpr double kStraightAngleDeg = 180;
  if (!number || *number <= 0 || *number >= kStraightAngleDeg) {
    return std::unexpected(Says::InvalidNumber);
  }
  options.FovDeg = *number;
  return {};
}

[[nodiscard]] Result SetOption(AssetRenderOptions &options, RenderOption option) {
  const auto [name, value] = option;
  if (name == "--camera" || name == "--position" || name == "--look-at" || name == "--fov") {
    return SetCameraOption(options, option);
  }
  if (name == "--variant") {
    options.Variant = value;
    return {};
  }
  if (name == "--animation") {
    options.Animation = ParseIndex(value);
    return options.Animation ? Result{} : std::unexpected(Says::InvalidIndex);
  }
  if (name == "--lighting") {
    if (value == "auto") {
      options.Lighting = LightingMode::Automatic;
    } else if (value == "authored") {
      options.Lighting = LightingMode::Authored;
    } else if (value == "studio") {
      options.Lighting = LightingMode::Studio;
    } else {
      return std::unexpected(Says::InvalidLighting);
    }
    return {};
  }
  if (name != "--time" && name != "--exposure") {
    return std::unexpected(Says::UnknownOption + std::string(name));
  }
  const auto number = ParseFiniteNumber(value);
  if (!number || *number < 0 || (name == "--exposure" && *number == 0)) {
    return std::unexpected(Says::InvalidNumber);
  }
  (name == "--time" ? options.TimeS : options.Exposure) = *number;
  return {};
}

[[nodiscard]] Holds<AssetRenderOptions> ParseRenderOptions(std::span<char *const> arguments) {
  if (arguments.size() < 3 || (arguments.size() - 3) % 2 != 0) {
    return std::unexpected(Says::Usage);
  }
  AssetRenderOptions options;
  options.Asset = arguments[0];
  options.Output = arguments[2];
  const std::string_view extent(arguments[1]);
  const size_t split = extent.find('x');
  if (split == std::string_view::npos) { return std::unexpected(Says::InvalidExtent); }
  const auto width = ParseIndex(extent.substr(0, split));
  const auto height = ParseIndex(extent.substr(split + 1));
  constexpr int kLargestCaptureAxisPx = 4096;
  if (!width || !height || *width == 0 || *height == 0 || *width > kLargestCaptureAxisPx ||
      *height > kLargestCaptureAxisPx) {
    return std::unexpected(Says::InvalidExtent);
  }
  options.Frame = {.WidthPx = *width, .HeightPx = *height};
  std::set<std::string_view> seen;
  for (size_t at = 3; at < arguments.size(); at += 2) {
    if (!seen.insert(arguments[at]).second) {
      return std::unexpected(Says::DuplicateOption + std::string(arguments[at]));
    }
    auto set = SetOption(options, {.Name = arguments[at], .Value = arguments[at + 1]});
    if (!set) { return std::unexpected(std::move(set.error())); }
  }
  if (options.Position.has_value() != options.Target.has_value()) {
    return std::unexpected(Says::InvalidAim);
  }
  if (options.Position && *options.Position == *options.Target) {
    return std::unexpected(Says::InvalidAim);
  }
  return options;
}

[[nodiscard]] Holds<Camera> ResolveCamera(const AssetRenderOptions &options,
                                          const GltfImporter &asset) {
  Camera camera;
  if (options.Camera == CameraMode::Indexed) {
    if (!asset.camera(options.CameraIndex, camera)) {
      return std::unexpected(Says::CameraUnavailable);
    }
  } else if (options.Camera == CameraMode::Default && asset.hasDefaultCamera()) {
    camera = asset.camera();
  } else {
    const auto framed = asset.frameCamera(options.Frame);
    if (!framed) { return std::unexpected(Says::CannotFrame); }
    camera = *framed;
  }
  if (options.Position && options.Target) {
    camera.PositionM = *options.Position;
    camera.LooksAt = true;
    camera.LookAtM = *options.Target;
    camera.NearM = Camera::kNearestM;
    if (!camera.Orthographic) { camera.FarM = 0; }
  }
  if (options.FovDeg) {
    camera.Orthographic = false;
    camera.FovDeg = *options.FovDeg;
  }
  return camera;
}

[[nodiscard]] Result CaptureAsset(const AssetRenderOptions &options) {
  GltfImporter asset;
  auto loaded = asset.load(options.Asset);
  if (!loaded) { return loaded; }
  if (!options.Variant.empty()) {
    auto selected = asset.selectMaterialVariant(options.Variant);
    if (!selected) { return selected; }
  }
  if (options.Animation || asset.animationCount() > 0) {
    const std::array<int, 1> clips{options.Animation.value_or(0)};
    auto selected = asset.selectAnimations(clips);
    if (!selected) { return selected; }
  }
  auto sampled = asset.sampleAnimation(options.TimeS);
  if (!sampled) { return sampled; }
  auto camera = ResolveCamera(options, asset);
  if (!camera) { return std::unexpected(std::move(camera.error())); }
  if (!SDL_Init(SDL_INIT_VIDEO)) { return std::unexpected(std::string(SDL_GetError())); }

  struct VideoSession {
    ~VideoSession() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
  };

  const VideoSession video;

  Engine engine;
  Scenario::Document scene;
  scene.Render.Declared = true;
  scene.Render.Frame = options.Frame;
  scene.Render.Outputs = {"sceneLinear"};
  scene.Render.Exposure = options.Exposure;
  Scenario::View view;
  view.Id = "asset";
  view.Person = "first";
  view.Sees = *camera;
  view.Placement = Scenario::CameraPlacement::Local;
  scene.Views.push_back(view);
  if (options.Lighting == LightingMode::Studio ||
      (options.Lighting == LightingMode::Automatic && asset.geometry().lamps() == 0)) {
    scene.Lit.Declared = true;
    constexpr double kStudioAngleDeg = 45;
    constexpr double kStudioFill = 0.05;
    scene.Lit.Key = {
        .Lux = std::numbers::pi, .ElevationDeg = kStudioAngleDeg, .BearingDeg = kStudioAngleDeg};
    scene.Lit.IndirectLight = {{kStudioFill, kStudioFill, kStudioFill}};
  }
  if (!engine.drawsInto(options.Frame) || !engine.declare(scene) ||
      !engine.setGeometry(asset.geometry()) || !engine.assemble() || !engine.advance()) {
    return std::unexpected(engine.error());
  }
  auto renderer = engine.renderer();
  const int frames = std::max(renderer.settleFrames(), 2);
  for (int frame = 0; frame < frames; ++frame) {
    auto rendered = renderer.render({});
    if (!rendered) { return rendered; }
  }
  return renderer.saveScreenshot(options.Output);
}
}

int RenderAsset(std::span<char *const> arguments) {
  auto options = ParseRenderOptions(arguments);
  if (!options) {
    std::println(stderr, "outshine-client: {}", options.error());
    return 2;
  }
  auto captured = CaptureAsset(*options);
  if (!captured) {
    std::println(stderr, "outshine-client: {}", captured.error());
    return 1;
  }
  std::println("RENDER\t{}\t{}x{}\tt={}\t{}",
               options->Asset,
               options->Frame.WidthPx,
               options->Frame.HeightPx,
               options->TimeS,
               options->Output);
  return 0;
}
}
