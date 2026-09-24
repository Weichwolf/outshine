#include <expected>
#include <span>
#include <array>
#include <chrono>
#include "math/Units.h"
#include "math/Mat4.h"
#include "RuntimeScene.h"
#include "AzimuthElevation.h"

#include <cstdint>
#include <cstddef>
#include <limits>

#include <algorithm>

#include <memory>
#include <numbers>
#include <cmath>
#include <string>
#include <string_view>
#include <optional>
#include <utility>
#include <ratio>
#include <vector>

#include "Heap.h"
#include "CameraFraming.h"
#include "SubjectProxy.h"
#include "Wgs84.h"

namespace outshine::Core {

namespace Says {
constexpr auto InvalidInitialGeometry = "initial native geometry is not well formed";
constexpr auto NoGeometrySurface = "native geometry requires a declared surface policy";
constexpr auto InvalidFramedCamera = "automatic framing produced an invalid renderer camera";
constexpr auto GeometryBuildAlreadyActive = "native geometry preparation is already active";
constexpr auto NoGeometryBuild = "native geometry preparation has not begun";
constexpr auto InvalidDrivenPartCount = "driven part count exceeds native geometry";
constexpr auto InvalidDrivenGeometry = "driven geometry does not match the composed subject";
constexpr auto InvalidGroundSurface = "generated ground surface is absent";
constexpr auto GeneratedGeometryAppendFailed = "generated geometry could not join driven geometry";
}

constexpr double kExposureCalibration = 1.2;
constexpr double kMeteredMiddleGrey = 2.5;

constexpr double kLuminanceRed = 0.2126;
constexpr double kLuminanceGreen = 0.7152;
constexpr double kLuminanceBlue = 0.0722;

namespace {

struct Listed {
  const std::vector<std::string> &Stages;
  const std::vector<std::string> &Outputs;
};

bool DeclarePlan(std::span<const Render::SubjectMaterial> surfaces,
                 bool sky,
                 bool presents,
                 const Listed &lists,
                 Render::PlanSpec &declaration,
                 std::string &error) {
  declaration.Outputs = {Render::Resource::FrameTex};
  if (presents) { declaration.Outputs.push_back(Render::Resource::Surface); }
  for (const std::string &named : lists.Outputs) {
    const std::optional<Render::Resource> row = Render::Compiled::ResourceByName(named);
    if (!row) {
      error = "the declaration asks the frame to keep '" + named +
              "', and the catalogue holds no picture by that name -- an output list is checked "
              "against the catalogue because a typo that silently drops a buffer leaves a client "
              "reading zeros and calling them a measurement";
      return false;
    }
    bool already = false;
    for (const Render::Resource held : declaration.Outputs) { already = already || held == *row; }
    if (!already) { declaration.Outputs.push_back(*row); }
  }

  if (!lists.Stages.empty()) {

    declaration.Content.clear();
    for (const std::string &named : lists.Stages) {
      const std::optional<Render::Stage> row = Render::Compiled::StageByName(named);
      if (!row) {
        error = "the declaration names render stage '" + named +
                "', and the catalogue holds no row by that name -- a stage list is checked "
                "against the catalogue because a typo that silently drops a pass is a picture "
                "nobody can explain";
        return false;
      }
      declaration.Content.push_back(*row);
    }
    declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
    declaration.Exposure = Render::Declared<float>(1.0f);
    return true;
  }
  declaration.Outputs.push_back(Render::Resource::SceneVelocity);
  declaration.Content = {Render::Stage::Subjects, Render::Stage::Overlay};
  if (sky) {
    declaration.Content.push_back(Render::Stage::Sky);
    declaration.Content.push_back(Render::Stage::AerialPerspective);
  }
  declaration.Content.push_back(Render::Stage::LightVisibility);
  bool carriesGlass = false;
  for (const Render::SubjectMaterial &surface : surfaces) {
    const SurfaceKind kind = surface.State().Kind();
    carriesGlass =
        carriesGlass || kind == SurfaceKind::ThinTransmissive || kind == SurfaceKind::Refractive;
  }
  if (carriesGlass) {
    declaration.Content.push_back(Render::Stage::SubjectsTransmissive);
    declaration.Content.push_back(Render::Stage::CompositeTransmission);
  }

  declaration.Display = Render::Declared<Render::Transfer>(Render::Transfer::Filmic);
  declaration.Exposure = Render::Declared<float>(1.0f);
  return true;
}

}

RuntimeScene::RuntimeScene(Render::SceneRenderer &renderer,
                           Declaration declaration,
                           const Ui::Font *font)
    : Renderer_(&renderer), Declared_(std::move(declaration)) {
  if (Declared_.InitialGeometry != nullptr) {
    DrivenGeometry_ = Declared_.InitialGeometry->clone();
    Held_.SetGeometry(DrivenGeometry_.clone());
    Declared_.InitialGeometry = nullptr;
  }
  std::vector<UiSurface> surfaces = std::move(Declared_.Surfaces);
  Declared_.Surfaces.clear();
  Ui_.Configure(renderer,
                font,
                std::move(surfaces),
                {.WidthPx = static_cast<double>(Declared_.SurfaceWidthPx),
                 .HeightPx = static_cast<double>(Declared_.SurfaceHeightPx)});
}

RuntimeScene::~RuntimeScene() {
  if (Renderer_ == nullptr) { return; }

  std::string ignored;
  (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
  (void)Renderer_->SetOverlay(nullptr, 0, ignored);
  Renderer_->SetPictureRegion({});
}

bool RuntimeScene::Open(Render::SceneRenderer &renderer,
                        Declaration declaration,
                        const Ui::Font *font,
                        std::unique_ptr<RuntimeScene> &out,
                        std::string &error) {
  const bool framesSubject = out && !out->Camera_.Prepared().HasExplicitCamera;
  if (!renderer.BeginsWorldCandidate(error)) { return false; }
  std::unique_ptr<RuntimeScene> candidate;
  if (!Prepare(renderer,
               std::move(declaration),
               font,
               candidate,
               error,
               out ? out->GroundAir_ : GroundAtmosphere{})) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  if (framesSubject) { candidate->FrameItself(); }
  if (!renderer.PublishesWorldCandidate(error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->StopsEditingCandidate();
  HandOffRenderer(out);
  out = std::move(candidate);
  return true;
}

bool RuntimeScene::Prepare(Render::SceneRenderer &renderer,
                           Declaration declaration,
                           const Ui::Font *font,
                           std::unique_ptr<RuntimeScene> &out,
                           std::string &error,
                           const GroundAtmosphere &atmosphere) {
  if (declaration.InitialGeometry != nullptr && !declaration.InitialGeometry->wellFormed()) {
    error = Says::InvalidInitialGeometry;
    return false;
  }
  auto editing = renderer.EditsWorldCandidate();
  std::unique_ptr<RuntimeScene> scene(new RuntimeScene(renderer, std::move(declaration), font));
  scene->GroundAir_ = atmosphere;
  if (!scene->Build(error)) { return false; }
  scene->CandidateEditor_.emplace(std::move(editing));
  out = std::move(scene);
  return true;
}

bool RuntimeScene::ReplacesGeometry(Render::SceneRenderer &renderer,
                                    const RuntimeScene &previous,
                                    Geometry replacement,
                                    const Ui::Font *font,
                                    std::unique_ptr<RuntimeScene> &out,
                                    std::string &error) {
  std::unique_ptr<RuntimeScene> candidate;
  const auto drivenParts = static_cast<size_t>(replacement.parts());
  if (!PreparesGeometryReplacement(
          renderer, previous, std::move(replacement), drivenParts, font, candidate, error)) {
    return false;
  }
  return PublishesPreparedWorld(renderer, out, candidate, error);
}

bool RuntimeScene::PreparesGeometryReplacement(Render::SceneRenderer &renderer,
                                               const RuntimeScene &previous,
                                               Geometry replacement,
                                               size_t drivenParts,
                                               const Ui::Font *font,
                                               std::unique_ptr<RuntimeScene> &candidate,
                                               std::string &error,
                                               Render::SceneResources::PieceSources pieces,
                                               ResourceRestoreMode restore) {
  if (!renderer.BeginsWorldCandidate(error, pieces)) { return false; }
  Declaration declaration = previous.Declared_;
  declaration.Surfaces = previous.Ui_.Surfaces();
  if (!Prepare(renderer, std::move(declaration), font, candidate, error, previous.GroundAir_)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->GroundAlbedo_ = previous.GroundAlbedo_;
  candidate->GroundSurface_ = previous.GroundSurface_;
  candidate->Scratch_.Digests = previous.Scratch_.Digests;
  if (std::cmp_less(drivenParts, replacement.parts())) {
    candidate->DrivenGeometry_ = previous.DrivenGeometry_.clone();
  }
  if (!candidate->SetGeometry(std::move(replacement), drivenParts, error) ||
      (restore == ResourceRestoreMode::Immediate &&
       (!candidate->RestoresPieceResources(error) || !candidate->RestoresGroundResources(error))) ||
      !candidate->Scrolled(previous.Ui_.ScrollState(), error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->Camera_ = previous.Camera_;
  return true;
}

bool RuntimeScene::PreparesWorldReplacement(Render::SceneRenderer &renderer,
                                            const RuntimeScene &previous,
                                            const Ui::Font *font,
                                            std::unique_ptr<RuntimeScene> &candidate,
                                            std::string &error,
                                            Render::SceneResources::PieceSources pieces,
                                            SubjectGeometrySources geometry,
                                            ResourceRestoreMode restore) {
  if (previous.Held_.HasGeometry() && geometry == SubjectGeometrySources::All) {
    return PreparesGeometryReplacement(renderer,
                                       previous,
                                       previous.Held_.Snapshot().clone(),
                                       previous.DrivenParts_,
                                       font,
                                       candidate,
                                       error,
                                       pieces,
                                       restore);
  }
  if (previous.DrivenGeometry_.parts() > 0) {
    return PreparesGeometryReplacement(renderer,
                                       previous,
                                       previous.DrivenGeometry_.clone(),
                                       static_cast<size_t>(previous.DrivenGeometry_.parts()),
                                       font,
                                       candidate,
                                       error,
                                       pieces,
                                       restore);
  }
  if (!renderer.BeginsWorldCandidate(error, pieces)) { return false; }
  Declaration declaration = previous.Declared_;
  declaration.Surfaces = previous.Ui_.Surfaces();
  if (!Prepare(renderer, std::move(declaration), font, candidate, error, previous.GroundAir_)) {
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->GroundAlbedo_ = previous.GroundAlbedo_;
  candidate->GroundSurface_ = previous.GroundSurface_;
  candidate->Scratch_.Digests = previous.Scratch_.Digests;
  if ((restore == ResourceRestoreMode::Immediate &&
       (!candidate->RestoresPieceResources(error) || !candidate->RestoresGroundResources(error))) ||
      !candidate->Scrolled(previous.Ui_.ScrollState(), error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->Camera_ = previous.Camera_;
  return true;
}

bool RuntimeScene::PublishesPreparedWorld(Render::SceneRenderer &renderer,
                                          std::unique_ptr<RuntimeScene> &out,
                                          std::unique_ptr<RuntimeScene> &candidate,
                                          std::string &error) {
  if (!renderer.PublishesWorldCandidate(error)) {
    candidate.reset();
    renderer.AbandonsWorldCandidate();
    return false;
  }
  candidate->StopsEditingCandidate();
  HandOffRenderer(out);
  out = std::move(candidate);
  return true;
}

double RuntimeScene::Framing() const {
  return Declared_.Fill > 0.0 ? Declared_.Fill : kCameraFramingFill;
}

bool RuntimeScene::FitsViewTo(const Box &bounds, Render::Viewpoint &out, std::string &error) const {
  const auto fitted = FrameCamera(
      bounds, {.Fill = Framing(), .Aspect = Renderer_->PictureW() / Renderer_->PictureH()});
  if (!fitted) {
    error = fitted.error();
    return false;
  }
  const auto viewed = Render::ViewpointOf(*fitted);
  if (!viewed) {
    error = Says::InvalidFramedCamera;
    return false;
  }
  out = *viewed;
  return true;
}

bool RuntimeScene::Reshape(std::string &error) {
  if (ShapeCooking_) {
    error = Says::GeometryBuildAlreadyActive;
    return false;
  }
  if (EverShaped_ && ShapedAt_ == Held_.Revision()) { return true; }
  EverShaped_ = false;
  Shaped_ = {};
  const auto shaped = Render::PrepareShape(Held_.Snapshot(), ShapeParts_);
  if (!shaped) {
    error = Describe(shaped.error());
    return false;
  }
  Shaped_ = *shaped;
  ShapedAt_ = Held_.Revision();
  EverShaped_ = true;
  return true;
}

namespace {

double Photopic(const Vec3f &triple) {
  return kLuminanceRed * static_cast<double>(triple[0]) +
         kLuminanceGreen * static_cast<double>(triple[1]) +
         kLuminanceBlue * static_cast<double>(triple[2]);
}

}

Medium RuntimeScene::DeclaredAir() const {
  return Hazed(kEarthAir, Declared_.Haze);
}

double RuntimeScene::MeteredLux() const {
  if (!Declared_.KeyFromClock) { return Declared_.KeyLux; }
  const double cosSun = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
  const GroundLight reach = GroundAir_.Evaluate(DeclaredAir(), cosSun);
  const double straightDown = cosSun > 0.0 ? cosSun : 0.0;
  return kSolarIlluminanceLx *
         (straightDown * Photopic(reach.SunTransmittance) + Photopic(reach.SkyIrradiance));
}

bool RuntimeScene::JoinsSubjects(std::string &error) {
  const bool animate = ImportsAnimation(Declared_.Playback);
  for (const std::string &joining : Declared_.Joins) {
    ScenePlayback arriving;
    if (!arriving.Load({.Path = joining, .Variant = ""},
                       animate,
                       {.Clip = Declared_.Clip, .Fps = Declared_.Fps},
                       error)) {
      return false;
    }
    if (!arriving.Sample(0.0, error)) { return false; }
    AssetReads_ += 1;
    if (!Held_.Append(std::move(arriving), error)) { return false; }
  }
  return true;
}

bool RuntimeScene::StandsSubjects(std::string &error) {
  if (!Held_.IsLoaded()) {
    const bool animate = ImportsAnimation(Declared_.Playback);
    if (!Held_.Load({.Path = Declared_.Stands, .Variant = Declared_.Variant},
                    animate,
                    {.Clip = Declared_.Clip, .Fps = Declared_.Fps},
                    error)) {
      return false;
    }
    AssetReads_ += 1;
  }
  if (!Pose(0.0, error)) { return false; }
  if (!JoinsSubjects(error)) { return false; }
  DrivenGeometry_ = Held_.Snapshot().clone();
  return CarriesBuilt(error);
}

void RuntimeScene::ClearsSubject() {
  Held_.Clear();
  DrivenGeometry_.clear();
  Materials_.Clear();
  ShadowRadiusStoodM_ = 0.0;
  DrivenParts_ = 0;
  PendingDrivenParts_.reset();
  Stoodup_ = false;
  PartBounds_.clear();
  if (Renderer_ != nullptr) {
    std::string ignored;
    (void)Renderer_->SetSubjectMesh(Render::SubjectMesh{}, ignored);
    (void)Renderer_->SetSubjectPlacements(nullptr, 0, ignored);
  }
}

bool RuntimeScene::CarriesBuilt(std::string &error) {
  if (Declared_.Surfacing.empty()) {
    error = "the declaration carries a built subject and no surface -- a body without a "
            "material cannot be resolved, and an empty list is a refusal, not a "
            "dereference";
    return false;
  }
  const auto tookFrom = std::chrono::steady_clock::now();
  CarryMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tookFrom)
                 .count();
  const auto reshapedFrom = std::chrono::steady_clock::now();
  if (!Reshape(error)) { return false; }
  ReshapeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - reshapedFrom)
          .count();
  const auto resolvedFrom = std::chrono::steady_clock::now();
  auto resolved = Materials_.Resolve(Held_.Snapshot(),
                                     Shaped_,
                                     Declared_.Surfacing.front(),
                                     Declared_.Overriding,
                                     GroundSurface_,
                                     Declared_.Stands);
  if (!resolved) {
    error = std::move(resolved.error());
    return false;
  }
  ResolveMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resolvedFrom)
          .count();
  return true;
}

bool RuntimeScene::RestoresPieceResources(std::string &error) {
  return Renderer_->RestorePieces(error);
}

bool RuntimeScene::RestoresGroundResources(std::string &error) {
  return Renderer_->RestoreGroundResources(error);
}

std::expected<bool, std::string> RuntimeScene::AdvancePieceResourceRestore(size_t &nextPiece,
                                                                           size_t piecesMost) {
  return Renderer_->AdvancePieceRestore(nextPiece, piecesMost);
}

std::expected<bool, std::string> RuntimeScene::AdvanceGroundResourceRestore(size_t &nextPage,
                                                                            size_t pagesMost) {
  return Renderer_->AdvanceGroundResourceRestore(nextPage, pagesMost);
}

void RuntimeScene::WearsPieces() {
  if (Renderer_ == nullptr) { return; }
  Renderer_->SetNativePieceSurfaces(
      Materials_.NativeSurfaceSlots(static_cast<size_t>(Held_.Snapshot().surfaces())));
}

void RuntimeScene::StandsShadowRadius() {
  ShadowRadiusStoodM_ = Declared_.ShadowRadiusM;
  if (ShadowRadiusStoodM_ > 0.0 || Shaped_.TriangleCount() == 0) { return; }
  const auto boundedFrom = std::chrono::steady_clock::now();
  const Box bounded = Shaped_.BoundsOf(DrivenParts_);
  BoundsMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
          .count();
  double across = 0.0;
  for (int axis = 0; axis < 3; ++axis) {
    const double span = (bounded.Max[axis] - bounded.Min[axis]) * Declared_.MetresPerUnit;
    across += span * span;
  }
  ShadowRadiusStoodM_ = 0.5 * std::sqrt(across);
}

PunctualLight RuntimeScene::KeyLight() const {
  const Vec3f toSun = TowardTheKey();
  PunctualLight key;
  key.Kind = LightKind::Directional;
  key.Intensity = static_cast<float>(Declared_.KeyLux);
  if (Declared_.KeyFromClock) {
    const GroundLight reach = GroundAir_.Evaluate(DeclaredAir(), static_cast<double>(toSun[1]));
    key.Intensity = static_cast<float>(kSolarIlluminanceLx);
    for (int channel = 0; channel < 3; ++channel) {
      key.Colour[channel] = reach.SunTransmittance[channel];
    }
  }
  for (int axis = 0; axis < 3; ++axis) { key.Direction[axis] = -toSun[axis]; }
  return key;
}

Vec3f RuntimeScene::TowardTheKey() const {
  const Vec3 direction = EastUpSouthDirection(Declared_.KeyBearingDeg * kDeg2Rad,
                                              Declared_.KeyElevationDeg * kDeg2Rad);
  Vec3f into;
  for (int axis = 0; axis < 3; ++axis) { into[axis] = static_cast<float>(direction[axis]); }
  return into;
}

void RuntimeScene::StandsKeyLight() {
  if (Declared_.DrawsSky) { Renderer_->SetMedium(DeclaredAir()); }

  const Vec3f toSun = TowardTheKey();
  const Vec3f up = {{0.0f, 1.0f, 0.0f}};

  Renderer_->SetSky({.ToSun = toSun,
                     .Up = up,
                     .IlluminanceLux = static_cast<float>(
                         Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux),
                     .EyeHeightM = 0.0f});
  if (ShadowRadiusStoodM_ > 0.0) { Renderer_->SetShadowFrame(toSun, up, ShadowRadiusStoodM_); }
}

bool RuntimeScene::StandsPlan(std::string &error) {
  Render::PlanSpec declaration;
  if (!DeclarePlan(Materials_.Slots(),
                   Declared_.DrawsSky,
                   Renderer_ != nullptr && Renderer_->Presents(),
                   {.Stages = Declared_.Stages, .Outputs = Declared_.Outputs},
                   declaration,
                   error)) {
    return false;
  }
  if (!Declared_.Transfer.empty()) {
    const std::optional<Render::Transfer> meant =
        Render::Spells(Render::kTransfers, Declared_.Transfer);
    if (!meant) {
      error = "the declaration transfers the frame as '" + Declared_.Transfer +
              "', and this engine spells " + Render::Spellings(Render::kTransfers);
      return false;
    }
    declaration.Display = Render::Declared<Render::Transfer>(*meant);
  }
  if (!Declared_.Precision.empty()) {
    const std::optional<Render::ScenePrecision> meant =
        Render::Spells(Render::kPrecisions, Declared_.Precision);
    if (!meant) {
      error = "the declaration carries the scene at '" + Declared_.Precision +
              "' precision, and this engine spells " + Render::Spellings(Render::kPrecisions);
      return false;
    }
    declaration.Precision = Render::Declared<Render::ScenePrecision>(*meant);
  }
  if (Declared_.Exposure > 0.0) {
    declaration.Exposure = Render::Declared<float>(static_cast<float>(Declared_.Exposure));
  } else {
    const double metered = MeteredLux();
    if (metered > 0.0) {
      const double ev100 = std::log2(metered / kMeteredMiddleGrey);
      declaration.Exposure = Render::Declared<float>(
          static_cast<float>(1.0 / (kExposureCalibration * std::pow(2.0, ev100))));
    }
  }
  if (Plan_ != nullptr && !(PlanDeclared_ == declaration)) { Plan_ = nullptr; }
  if (Plan_ == nullptr) {
    auto made = Render::Compiled::Compile(declaration);
    if (!made) {
      error = std::move(made).error();
      return false;
    }
    Plan_ = *std::move(made);
    PlanDeclared_ = std::move(declaration);
    PlanInits_ += 1;
    const auto stood = Renderer_->Init(
        {.WidthPx = Declared_.SurfaceWidthPx, .HeightPx = Declared_.SurfaceHeightPx}, Plan_);
    if (!stood) {
      error = std::move(stood).error();
      return false;
    }
  }
  return true;
}

bool RuntimeScene::Build(std::string &error) {
  BuildStage_ = GeometryBuildStage::Plan;
  for (;;) {
    auto advanced = AdvanceBuild(std::numeric_limits<size_t>::max());
    if (!advanced) {
      error = std::move(advanced.error());
      BuildStage_ = GeometryBuildStage::Idle;
      return false;
    }
    if (*advanced) { return true; }
  }
}

std::expected<void, std::string> RuntimeScene::PlanBuild() {
  std::string error;
  if (PendingDrivenParts_) {
    if (!Held_.HasGeometry()) {
      ClearsSubject();
    } else if (!CarriesBuilt(error)) {
      return std::unexpected(std::move(error));
    }
  } else {
    if (!Held_.HasGeometry() && Declared_.Stands.empty()) { ClearsSubject(); }
    if (Held_.HasGeometry() && Declared_.Stands.empty() && !CarriesBuilt(error)) {
      return std::unexpected(std::move(error));
    }
    if (!Declared_.Stands.empty() && !StandsSubjects(error)) {
      return std::unexpected(std::move(error));
    }
  }

  if (!Reshape(error)) { return std::unexpected(std::move(error)); }
  DrivenParts_ = Shaped_.Parts.size();
  if (PendingDrivenParts_) { DrivenParts_ = *PendingDrivenParts_; }
  StandsShadowRadius();

  const auto planFrom = std::chrono::steady_clock::now();
  if (!StandsPlan(error)) { return std::unexpected(std::move(error)); }
  PlanMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - planFrom)
                .count();
  WearsPieces();
  if (DeclaresKeyLight()) { StandsKeyLight(); }
  SubmitMs_ = 0.0;
  InsideMs_ = 0.0;
  return {};
}

std::expected<void, std::string> RuntimeScene::BindBuild() {
  std::string error;
  if (Shaped_.TriangleCount() == 0) {
    if (!Camera_.NeedsBinding()) { Camera_.Unbind(); }
    if (!Renderer_->SetSubjectMaterials(Materials_.Slots(), error)) {
      return std::unexpected(std::move(error));
    }
    Renderer_->SetPictureRegion({});
    return {};
  }
  Renderer_->SetPictureRegion({.X = Declared_.PictureLeftFrac,
                               .Y = Declared_.PictureTopFrac,
                               .Width = Declared_.PictureWidthFrac,
                               .Height = Declared_.PictureHeightFrac});
  Renderer_->CastsBelow(static_cast<uint32_t>(Shaped_.Parts.size()));
  const auto standFrom = std::chrono::steady_clock::now();
  if (!Stand(error)) { return std::unexpected(std::move(error)); }
  StandMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - standFrom)
                 .count();
  const auto surfaceFrom = std::chrono::steady_clock::now();
  if (!Render::Surface(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error)) {
    return std::unexpected(std::move(error));
  }
  SurfaceMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - surfaceFrom)
          .count();
  return {};
}

std::expected<void, std::string> RuntimeScene::PrepareBuild() {
  std::string error;
  const auto phaseAt = std::chrono::steady_clock::now();
  const bool prepared =
      Render::PlanPlacement(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error);
  SubmitMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  if (!prepared) { return std::unexpected(std::move(error)); }
  return {};
}

std::expected<bool, std::string> RuntimeScene::PackBuild(size_t itemsMost) {
  const auto phaseAt = std::chrono::steady_clock::now();
  auto packed = Render::AdvancePackPlacement(Stood_, Scratch_, itemsMost);
  SubmitMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  return packed;
}

std::expected<void, std::string> RuntimeScene::IndexBuild() {
  const auto phaseAt = std::chrono::steady_clock::now();
  auto began = Render::BeginPlacementUpload(*Renderer_, Stood_, Scratch_);
  SubmitMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  if (!began) { return std::unexpected(std::move(began.error())); }
  BuildTicket_ = *began;
  return {};
}

std::expected<void, std::string> RuntimeScene::FinishBuild() {
  std::string error;
  const auto phaseAt = std::chrono::steady_clock::now();
  const bool placed =
      Render::FinishPlacementUpload(*Renderer_, Stood_, Scratch_, BuildTicket_, error);
  SubmitMs_ +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  if (!placed) { return std::unexpected(std::move(error)); }
  BuildTicket_ = {};
  Stoodup_ = true;
  if (Camera_.IsUnbound()) { Camera_.MarkBound(); }
  InsideMs_ = StandMs_ + SurfaceMs_ + SubmitMs_;
  return {};
}

std::expected<void, std::string> RuntimeScene::FinalizeBuild() {
  std::string error;
  if (!Renderer_->RestorePieceMaterials(error)) { return std::unexpected(std::move(error)); }
  const auto composedFrom = std::chrono::steady_clock::now();
  const bool composed = Ui_.Compose(error);
  ComposeMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - composedFrom)
          .count();
  if (!composed) { return std::unexpected(std::move(error)); }
  return {};
}

std::expected<bool, std::string> RuntimeScene::AdvanceBuild(size_t itemsMost) {
  std::expected<void, std::string> advanced;
  switch (BuildStage_) {
    case GeometryBuildStage::Plan:
      advanced = PlanBuild();
      BuildStage_ = GeometryBuildStage::Bind;
      break;
    case GeometryBuildStage::Bind:
      advanced = BindBuild();
      BuildStage_ =
          Shaped_.TriangleCount() == 0 ? GeometryBuildStage::Finalize : GeometryBuildStage::Prepare;
      break;
    case GeometryBuildStage::Prepare:
      advanced = PrepareBuild();
      BuildStage_ = GeometryBuildStage::Pack;
      break;
    case GeometryBuildStage::Pack: {
      auto packed = PackBuild(itemsMost);
      if (!packed) { return std::unexpected(std::move(packed.error())); }
      if (*packed) { BuildStage_ = GeometryBuildStage::Index; }
      return false;
    }
    case GeometryBuildStage::Index:
      advanced = IndexBuild();
      BuildStage_ = GeometryBuildStage::Finish;
      break;
    case GeometryBuildStage::Finish:
      advanced = FinishBuild();
      BuildStage_ = GeometryBuildStage::Finalize;
      break;
    case GeometryBuildStage::Finalize:
      advanced = FinalizeBuild();
      BuildStage_ = GeometryBuildStage::Idle;
      break;
    case GeometryBuildStage::Idle: return std::unexpected(Says::NoGeometryBuild);
  }
  if (!advanced) { return std::unexpected(std::move(advanced.error())); }
  return BuildStage_ == GeometryBuildStage::Idle;
}

void RuntimeScene::RecordGeometrySlice(GeometryBuildStage stage, double ms) noexcept {
  double *record = nullptr;
  switch (stage) {
    case GeometryBuildStage::Plan: record = &GeometrySlices_.PlanMs; break;
    case GeometryBuildStage::Bind: record = &GeometrySlices_.BindMs; break;
    case GeometryBuildStage::Prepare: record = &GeometrySlices_.DrawPlanMs; break;
    case GeometryBuildStage::Pack: record = &GeometrySlices_.PackMs; break;
    case GeometryBuildStage::Index: record = &GeometrySlices_.IndexMs; break;
    case GeometryBuildStage::Finish: record = &GeometrySlices_.FinishMs; break;
    case GeometryBuildStage::Finalize: record = &GeometrySlices_.FinalizeMs; break;
    case GeometryBuildStage::Idle: return;
  }
  if (record != nullptr) { *record = std::max(*record, ms); }
}

bool RuntimeScene::Pose(double seconds, std::string &error) {
  if (!Held_.Sample(seconds, error)) { return false; }
  if (!Reshape(error)) { return false; }
  return true;
}

bool RuntimeScene::Measure(double seconds, std::string &error) {
  if (!Held_.Sample(seconds, error)) { return false; }
  if (!Reshape(error)) { return false; }
  return true;
}

void RuntimeScene::Eye(const Render::Viewpoint &from) noexcept {
  Camera_.Override(from);
}

void RuntimeScene::CapturesRenderedPositions() {
  RenderedPositionsM_.clear();
  RenderedPositionsM_.reserve(Shaped_.VertexCount() * 3u);
  for (const Render::ShapePart &part : Shaped_.Parts) {
    RenderedPositionsM_.insert(
        RenderedPositionsM_.end(), part.PositionsM.begin(), part.PositionsM.end());
  }
}

void RuntimeScene::CoverShapedParts() {
  for (size_t part = 0; part < PartBounds_.size() && part < Shaped_.Parts.size(); ++part) {
    const Render::ShapePart &one = Shaped_.Parts[part];
    Box &held = PartBounds_[part];
    for (size_t vertex = 0; vertex < one.VertexCount && (vertex + 1) * 3 <= one.PositionsM.size();
         ++vertex) {
      const float *const from = one.PositionsM.data() + vertex * 3;
      held.Cover(Vec3{{from[0], from[1], from[2]}});
    }
  }
}

bool RuntimeScene::PartVolumes(std::string &error) {
  if (!PartBounds_.empty()) { return true; }
  const size_t parts = Shaped_.Parts.size();
  if (parts == 0) { return true; }
  PartBounds_.assign(parts, Box{});
  CoverShapedParts();
  for (int sample = 0; sample < Sweeps(); ++sample) {
    if (Seconds(sample) == Held_.TimeS()) { continue; }
    if (!Measure(Seconds(sample), error)) { return false; }
    CoverShapedParts();
  }
  return Held_.FrameCount() <= 1 || Measure(Held_.TimeS(), error);
}

bool RuntimeScene::PlacedBounds(Extents &into, std::string &error) {
  if (!PartVolumes(error)) { return false; }
  const size_t framed =
      DrivenParts_ > 0 && DrivenParts_ < PartBounds_.size() ? DrivenParts_ : PartBounds_.size();
  Box grown;
  for (size_t part = 0; part < framed; ++part) {
    grown.Cover(PartBounds_[part].Through(part < Stood_.Parts() ? Stood_.Placement(part) : Mat4{}));
  }
  const Vec3 leastM = grown.Empty() ? Vec3{} : grown.Min;
  const Vec3 mostM = grown.Empty() ? Vec3{} : grown.Max;
  for (int axis = 0; axis < 3; ++axis) {
    into.LeastM[axis] = leastM[axis];
    into.MostM[axis] = mostM[axis];
  }
  return true;
}

bool RuntimeScene::Look(std::string &error) {
  if (Camera_.HasOverride()) {
    Camera_.Prepared().Eye = Camera_.Override();
    Camera_.Prepared().HasExplicitCamera = true;
    return Render::Aim(*Renderer_, Shaped_, Camera_.Prepared(), Stood_.Anchor(), error);
  }
  Extents placed;
  if (!PlacedBounds(placed, error)) { return false; }
  const Vec3 &least = placed.LeastM;
  const Vec3 &most = placed.MostM;
  Render::Viewpoint framed;
  if (!FitsViewTo({.Min = least, .Max = most}, framed, error)) { return false; }
  const Vec3 centre = {
      {(least[0] + most[0]) * 0.5, (least[1] + most[1]) * 0.5, (least[2] + most[2]) * 0.5}};
  const double turn = Camera_.OrbitDegrees() * kDeg2Rad;
  const double cosine = std::cos(turn);
  const double sine = std::sin(turn);
  const auto spun = [cosine, sine](const Vec3 &from) {
    return Vec3{{from[0] * cosine + from[2] * sine, from[1], -from[0] * sine + from[2] * cosine}};
  };
  framed.EyeM = centre + spun(framed.EyeM - centre);
  framed.Forward = spun(framed.Forward);
  framed.Right = spun(framed.Right);
  framed.Up = spun(framed.Up);
  Camera_.Prepare(framed, false, DrivenParts_);
  return Render::Aim(*Renderer_, Shaped_, Camera_.Prepared(), Stood_.Anchor(), error);
}

void RuntimeScene::StandsEnvironment() {
  Render::SubjectEnvironment environment;
  for (int channel = 0; channel < 3; ++channel) {
    environment.RadianceLinear[channel] = static_cast<float>(Declared_.IndirectLight[channel]);
    environment.GroundLinear[channel] = environment.RadianceLinear[channel];
  }
  if (Declared_.DrawsSky && DeclaresKeyLight()) { LightsFromTheSky(environment); }
  for (int channel = 0; channel < 3; ++channel) {
    AmbientStood_[channel] = environment.RadianceLinear[channel];
    GroundStood_[channel] = environment.GroundLinear[channel];
  }
  Stood_.Around(environment);
}

void RuntimeScene::ReadIrradiance(std::span<const float, Render::kIrradianceFloats> irradiance) {
  const auto &environment = Stood_.IndirectLight();
  const double scale = environment.SkyLux / std::numbers::pi;
  for (size_t channel = 0; channel < 3; ++channel) {
    const double sky = irradiance[channel] * scale;
    const double sun = irradiance[3 + channel] * std::max(environment.CosSunZenith, 0.0) * scale;
    AmbientStood_[channel] = environment.RadianceLinear[channel] + sky;
    GroundStood_[channel] =
        environment.GroundLinear[channel] + environment.GroundAlbedo[channel] * (sky + sun);
  }
}

void RuntimeScene::LightsFromTheSky(Render::SubjectEnvironment &environment) const {
  environment.SkyLux = Declared_.KeyFromClock ? kSolarIlluminanceLx : Declared_.KeyLux;
  environment.CosSunZenith = std::sin(Declared_.KeyElevationDeg * kDeg2Rad);
  for (int channel = 0; channel < 3; ++channel) {
    environment.GroundAlbedo[channel] = GroundAlbedo_[channel];
  }
}

void RuntimeScene::EmitsPerPart() {
  const auto partSlots = Materials_.PartSlots();
  const auto materials = Materials_.Slots();
  for (size_t part = 0; part < partSlots.size(); ++part) {
    const uint32_t slot = partSlots[part];
    if (slot >= materials.size()) { continue; }
    const Material &row = materials[slot].Row;
    const bool emits = row.Emission[0] > 0.0f || row.Emission[1] > 0.0f || row.Emission[2] > 0.0f;
    std::array<float, 3> radiance{};
    for (int channel = 0; channel < 3; ++channel) {
      float value = row.BaseColour[channel];
      if (!row.Unlit) {
        value = emits ? row.Emission[channel]
                      : value * static_cast<float>(Declared_.IndirectLight[channel]);
      }
      radiance[static_cast<size_t>(channel)] = value;
    }
    (void)Stood_.Emits(part, radiance);
  }
}

bool RuntimeScene::ApplyAuthoredCamera(Render::Viewpoint &out) const {
  if (!Held_.AuthoredCamera()) { return false; }
  const auto viewpoint = Render::ViewpointOf(*Held_.AuthoredCamera());
  if (!viewpoint) { return false; }
  out = *viewpoint;
  return true;
}

bool RuntimeScene::Stand(std::string &error) {
  auto standFrom = std::chrono::steady_clock::now();
  const auto sinceStand = [&standFrom] {
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - standFrom)
            .count();
    standFrom = std::chrono::steady_clock::now();
    return ms;
  };
  Stood_ = Render::SubjectProxy{};
  const Vec3 anchorEcefM = {{kWgs84A, 0.0, 0.0}};
  if (!Reshape(error)) { return false; }
  ReshapeAgainMs_ = sinceStand();
  Stood_.Stands(Shaped_, anchorEcefM);
  ProxyStandsMs_ = sinceStand();
  const Mat4 unmoved;
  for (size_t part = 0; part < Stood_.Parts(); ++part) {
    if (!Stood_.Places(part, unmoved)) { return false; }
  }
  Camera_.Prepare(Camera_.HasOverride() ? Camera_.Override() : Render::Viewpoint{},
                  Camera_.HasOverride(),
                  DrivenParts_);
  PlacementUploadHistory_.Reset();
  if (Held_.IsAnimated() && RenderedPositionsM_.size() == Shaped_.VertexCount() * 3u) {
    Stood_.Posed(RenderedPositionsM_);
  }
  PlacesMs_ = sinceStand();
  if (!Stood_.Wears(Materials_.PartSlots(), Materials_.Slots(), error)) { return false; }
  WearsMs_ = sinceStand();
  EmitsPerPart();

  LampsMs_ = sinceStand();
  for (const PunctualLight &placed : Shaped_.Lamps) { Stood_.Lit(placed); }
  if (DeclaresKeyLight()) { Stood_.Lit(KeyLight()); }
  LitMs_ = sinceStand();
  StandsEnvironment();
  MediumMs_ = sinceStand();

  Render::Viewpoint eye = Camera_.Prepared().Eye;
  const bool declared = ApplyAuthoredCamera(eye);
  Camera_.Prepared().Eye = eye;
  if (!Camera_.HasOverride() && (Declared_.Fill > 0.0 || !declared)) {
    const auto boundedFrom = std::chrono::steady_clock::now();
    Box bounded = Shaped_.BoundsOf(DrivenParts_);
    BoundsMs_ =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - boundedFrom)
            .count();
    for (int sample = 1; sample < Sweeps(); ++sample) {
      if (!Measure(Seconds(sample), error)) { return false; }
      bounded.Cover(Shaped_.BoundsOf(DrivenParts_));
    }
    if (Held_.FrameCount() > 1 && !Measure(0.0, error)) { return false; }
    if (!FitsViewTo(bounded, eye, error)) { return false; }
    Camera_.Prepared().Eye = eye;
  }
  FramingMs_ = sinceStand();
  return true;
}

bool RuntimeScene::Submit(std::string &error) {
  if (!Stoodup_) {
    Stoodup_ = Render::Place(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error);
    return Stoodup_;
  }
  return Render::Move(*Renderer_, Stood_, Camera_.Prepared(), Scratch_, error);
}

const std::string &RuntimeScene::ProgrammeOf(size_t surface) const {
  return Ui_.ProgrammeOf(surface);
}

bool RuntimeScene::Redeclare(std::vector<UiSurface> surfaces, std::string &error) {
  const auto published = Renderer_->PublishedWorld();
  return Ui_.Redeclare(std::move(surfaces), error);
}

void RuntimeScene::SkyEye(double aboveGroundM) {
  if (Renderer_ == nullptr) { return; }

  constexpr double kSkyEyeStepM = 2.0;
  const double quantisedM =
      std::floor(std::fmax(0.0, aboveGroundM) / kSkyEyeStepM + 0.5) * kSkyEyeStepM;
  const auto published = Renderer_->PublishedWorld();
  Renderer_->SetSkyEye(static_cast<float>(quantisedM));
}

bool RuntimeScene::ResizeBodyInstances(size_t bodies, std::string &error) {
  const auto published = Renderer_->PublishedWorld();
  if (bodies == 0) {
    error = "runtime scene requires at least one body instance";
    return false;
  }
  const size_t stood = Stood_.Instances();
  if (!Stood_.ResizeInstances(bodies)) {
    error = "subject proxy cannot resize to " + std::to_string(bodies) + " body instances";
    return false;
  }
  if (stood != bodies && Stoodup_) {
    Stoodup_ = false;
    if (!Submit(error)) { return false; }
  }
  if (PlacementUploadHistory_.Bodies() != bodies) { PlacementUploadHistory_.Resize(bodies); }
  return true;
}

bool RuntimeScene::Carry(const Bearing &held, std::string &error) {
  return Carry(0, held, error);
}

Mat4 RuntimeScene::InMetres(const Mat4 &placed) const {
  const double perUnit = Declared_.MetresPerUnit > 0.0 ? Declared_.MetresPerUnit : 1.0;
  Mat4 out = placed;
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 4; ++row) { out[column * 4 + row] *= perUnit; }
  }
  return out;
}

bool RuntimeScene::Carry(size_t body, const Bearing &held, std::string &error) {
  const auto published = Renderer_->PublishedWorld();
  const Mat4 bodyM = InMetres(held.WorldFromBodyM);
  if (DrivenParts_ == 0) {
    error = "nothing joined this picture from a file, so there is no body to carry -- every part "
            "stands where the world put it";
    return false;
  }
  const size_t parts = Shaped_.Parts.size();
  if (Stood_.Parts() != parts) {
    error = "the subject proxy stands over " + std::to_string(Stood_.Parts()) +
            " parts and the geometry carries " + std::to_string(parts) +
            ", so nothing standing was held.AsBuilt from what is being carried";
    return false;
  }
  PlacementUploadHistory_.EnsureOne();
  if (body >= PlacementUploadHistory_.Bodies() || body >= Stood_.Instances()) {
    error = "a body numbered " + std::to_string(body) + " was carried into a picture standing " +
            std::to_string(Stood_.Instances()) +
            " deep -- a picture carries the bodies it was told to carry and no others";
    return false;
  }
  const size_t instances = Stood_.Instances();
  const size_t rows = parts * instances;
  const bool bodyMoved = PlacementUploadHistory_.NeedsBodyUpload(body, bodyM);
  const bool builtMoved = PlacementUploadHistory_.NeedsBuiltUpload(held.AsBuilt);
  if (!bodyMoved && !builtMoved) { return true; }

  const size_t joined = DrivenParts_ < parts ? DrivenParts_ : parts;
  if (bodyMoved) {
    for (size_t part = 0; part < joined; ++part) {
      if (!Stood_.Places(part, body, bodyM)) { return false; }
    }
  }
  if (builtMoved) {
    for (size_t part = joined; part < parts; ++part) {
      if (!Stood_.Places(part, body, held.AsBuilt)) { return false; }
    }
  }
  PartBounds_.clear();
  Renderer_->CastsBelow(static_cast<uint32_t>(Shaped_.Parts.size()));

  if ((bodyMoved && joined > 0) &&
      (!Render::Moved(*Renderer_,
                      {.Rows = rows, .Many = instances, .Which = body, .ToPart = joined},
                      bodyM,
                      error))) {
    return false;
  }

  if ((builtMoved && joined < parts) &&
      (!Render::Moved(
          *Renderer_,
          {.Rows = rows, .Many = instances, .Which = body, .FromPart = joined, .ToPart = parts},
          held.AsBuilt,
          error))) {
    return false;
  }

  PlacementUploadHistory_.RecordsUpload(body, bodyM, held.AsBuilt);
  return true;
}

bool RuntimeScene::SetGeometry(outshine::Geometry &&built, size_t drivenParts, std::string &error) {
  if (Declared_.Surfacing.empty()) {
    error = Says::NoGeometrySurface;
    return false;
  }
  return SetGeometry(std::move(built), drivenParts, Declared_.Surfacing.front(), error);
}

bool RuntimeScene::SetGeometry(outshine::Geometry &&built,
                               size_t drivenParts,
                               const Material &wearing,
                               std::string &error) {
  auto began = BeginGeometryBuild(std::move(built), drivenParts, Material(wearing));
  if (!began) {
    error = std::move(began.error());
    return false;
  }
  for (;;) {
    auto advanced = AdvanceGeometryBuild(std::numeric_limits<size_t>::max());
    if (!advanced) {
      error = std::move(advanced.error());
      return false;
    }
    if (*advanced) { return true; }
  }
}

std::expected<void, std::string>
RuntimeScene::BeginGeometryBuild(outshine::Geometry &&built, size_t drivenParts, Material wearing) {
  if (GeometryBuildActive()) { return std::unexpected(Says::GeometryBuildAlreadyActive); }
  if (std::cmp_greater(drivenParts, built.parts())) {
    return std::unexpected(Says::InvalidDrivenPartCount);
  }
  if (std::cmp_equal(drivenParts, built.parts())) {
    DrivenGeometry_ = built.clone();
  } else if (!std::cmp_equal(drivenParts, DrivenGeometry_.parts())) {
    return std::unexpected(Says::InvalidDrivenGeometry);
  }
  Camera_.Invalidate();
  GeometryBuildSurfaces_ = std::move(Declared_.Surfacing);
  Declared_.Surfacing.clear();
  Declared_.Surfacing.push_back(wearing);
  Held_.SetGeometry(std::move(built));
  Stoodup_ = false;
  PendingDrivenParts_ = drivenParts;
  GeometrySlices_ = {};
  const auto phaseAt = std::chrono::steady_clock::now();
  Shaped_ = {};
  EverShaped_ = false;
  ShapeParts_.Clear();
  Render::AppendGeometry(Held_.Snapshot(), ShapeParts_);
  if (auto bound = Render::BindShapeStorage(ShapeParts_); !bound) {
    RestoreGeometryBuildState();
    return std::unexpected(std::string(Describe(bound.error())));
  }
  ShapeCooking_.emplace(ShapeParts_, ShapeParts_.Surfaces);
  BuildMs_ =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  return {};
}

std::expected<void, std::string> RuntimeScene::BeginGeneratedGeometryBuild(
    outshine::Geometry &&generated, MaterialInstance groundSurface, Material wearing) {
  if (!groundSurface.bound() || groundSurface.index() >= generated.surfaces()) {
    return std::unexpected(Says::InvalidGroundSurface);
  }
  if (DrivenParts_ == 0) {
    GroundSurface_ = groundSurface.index();
    return BeginGeometryBuild(std::move(generated), 0, wearing);
  }
  Geometry driven = DrivenGeometry_.clone();
  const int surfaceBase = driven.surfaces();
  if (!driven.append(generated)) { return std::unexpected(Says::GeneratedGeometryAppendFailed); }
  GroundSurface_ = surfaceBase + groundSurface.index();
  return BeginGeometryBuild(std::move(driven), DrivenParts_, wearing);
}

std::expected<bool, std::string> RuntimeScene::AdvanceGeometryBuild(size_t itemsMost) {
  if (!GeometryBuildActive()) { return std::unexpected(Says::NoGeometryBuild); }
  const auto phaseAt = std::chrono::steady_clock::now();
  if (ShapeCooking_) {
    auto cooked = ShapeCooking_->Advance(itemsMost);
    const double sliceMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt)
            .count();
    BuildMs_ += sliceMs;
    GeometrySlices_.CookMs = std::max(GeometrySlices_.CookMs, sliceMs);
    if (!cooked) {
      const std::string error(Describe(cooked.error()));
      RestoreGeometryBuildState();
      return std::unexpected(error);
    }
    if (!*cooked) { return false; }
    Shaped_ = Render::ViewShape(ShapeParts_);
    ShapedAt_ = Held_.Revision();
    EverShaped_ = true;
    ShapeCooking_.reset();
    BuildStage_ = GeometryBuildStage::Plan;
    return false;
  }
  const GeometryBuildStage stage = BuildStage_;
  auto advanced = AdvanceBuild(itemsMost);
  const double sliceMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseAt).count();
  BuildMs_ += sliceMs;
  RecordGeometrySlice(stage, sliceMs);
  if (!advanced) {
    std::string error = std::move(advanced.error());
    RestoreGeometryBuildState();
    return std::unexpected(std::move(error));
  }
  if (*advanced) { RestoreGeometryBuildState(); }
  return *advanced;
}

void RuntimeScene::RestoreGeometryBuildState() noexcept {
  ShapeCooking_.reset();
  BuildStage_ = GeometryBuildStage::Idle;
  BuildTicket_ = {};
  PendingDrivenParts_.reset();
  Declared_.Surfacing = std::move(GeometryBuildSurfaces_);
}

size_t RuntimeScene::TookPosing_ = 0, RuntimeScene::TookSubmitting_ = 0,
       RuntimeScene::TookAiming_ = 0, RuntimeScene::TookDrawing_ = 0;
size_t RuntimeScene::AssetReads_ = 0;
size_t RuntimeScene::PlanInits_ = 0;

bool RuntimeScene::Advance(std::string &error) {
  const auto published = Renderer_->PublishedWorld();
  static const Heap::Tag kAdvancingTag("runtime-scene-advance");
  const Heap::Tagged advancing(kAdvancingTag);
  const auto took = [](const char *tag, size_t before) { return Heap::TakenUnder(tag) - before; };

  if (Held_.IsAnimated() && Held_.DurationS() > 0.0) {
    Held_.Advance(Declared_.Fps > 0.0 ? 1.0 / Declared_.Fps : 0.0,
                  Declared_.Playback == PlaybackPolicy::Loop);
    const size_t beforePose = Heap::TakenUnder("runtime-scene-pose");
    {
      static const Heap::Tag kPosingTag("runtime-scene-pose");
      const Heap::Tagged posing(kPosingTag);
      if (!Pose(Held_.TimeS(), error)) { return false; }
    }
    TookPosing_ = took("runtime-scene-pose", beforePose);
    const size_t beforeSubmit = Heap::TakenUnder("runtime-scene-submit");
    {
      static const Heap::Tag kSubmittingTag("runtime-scene-submit");
      const Heap::Tagged submitting(kSubmittingTag);
      if (!Submit(error)) { return false; }
    }
    TookSubmitting_ = took("runtime-scene-submit", beforeSubmit);
  }

  const bool orbits = Declared_.OrbitDegPerFrame != 0.0 && Shaped_.TriangleCount() > 0;
  if (orbits) { Camera_.AdvanceOrbit(Declared_.OrbitDegPerFrame); }
  if (orbits || Camera_.NeedsBinding()) {
    const size_t beforeAim = Heap::TakenUnder("runtime-scene-aim");
    {
      static const Heap::Tag kAimingTag("runtime-scene-aim");
      const Heap::Tagged aiming(kAimingTag);
      if (!Look(error)) { return false; }
    }
    Camera_.MarkBound();
    TookAiming_ = took("runtime-scene-aim", beforeAim);
  }
  return true;
}

bool RuntimeScene::Draw(std::string &error) {
  if (Renderer_ == nullptr) {
    error = "no device stands, so there is nothing to draw with";
    return false;
  }
  const auto published = Renderer_->PublishedWorld();
  if (Camera_.NeedsBinding()) {
    if (!Look(error)) { return false; }
    Camera_.MarkBound();
  }
  const size_t beforeDraw = Heap::TakenUnder("render-frame");
  {
    static const Heap::Tag kDrawingTag("render-frame");
    const Heap::Tagged drawing(kDrawingTag);
    auto rendered = Renderer_->RenderFrame();
    if (!rendered) {
      error = std::move(rendered.error());
      return false;
    }
    if (Held_.IsAnimated()) {
      CapturesRenderedPositions();
      Stood_.Posed(RenderedPositionsM_);
    }
  }
  TookDrawing_ = Heap::TakenUnder("render-frame") - beforeDraw;
  return true;
}

}
