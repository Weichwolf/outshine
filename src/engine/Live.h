#ifndef OUTSHINE_ENGINE_LIVE_H
#define OUTSHINE_ENGINE_LIVE_H

#include <algorithm>
#include <expected>
#include <span>
#include <optional>
#include <array>
#include "math/Box.h"
#include "math/Mat4.h"
#include "Shape.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Outshine.h>
#include <scenario/Scenario.h>

#include "Asset.h"
#include "SubjectProxy.h"
#include "Overlay.h"
#include "Layout.h"
#include "Markup.h"
#include "Paint.h"
#include "Pointer.h"
#include "SceneRenderer.h"
#include "Style.h"
#include "scene/Material.h"
#include "Atmosphere.h"
#include "Surfacing.h"

namespace outshine::Core {

constexpr Vec3 kGroundAlbedoUnsaid = {{0.10, 0.13, 0.07}};

struct Declaration {
  double Haze = 1.0;
  Scenario::AssetAnimation Animation = Scenario::AssetAnimation::Play;
  int Clip = 0;

  int SurfaceWidthPx = 0, SurfaceHeightPx = 0;

  std::string Stands;
  std::vector<std::string> Joins;

  std::vector<std::string> Stages;

  std::vector<std::string> Outputs;

  std::string Transfer;
  std::string Precision;

  const Geometry *InitialGeometry = nullptr;
  std::vector<Material> Surfacing{Material{}};

  std::vector<Scenario::SurfaceOverride> Overriding;

  std::string Variant;

  double MetresPerUnit = 1.0;

  double Fps = Scenario::kFpsUnsaid;

  double Fill = 0.0;

  double OrbitDegPerFrame = 0.0;

  double PictureLeftFrac = 0.0, PictureTopFrac = 0.0;
  double PictureWidthFrac = 0.0, PictureHeightFrac = 0.0;

  std::array<double, 3> IndirectLight = {0.0, 0.0, 0.0};
  double KeyLux = 0.0;

  double Exposure = 0.0;

  bool DrawsSky = false;
  double ShadowRadiusM = 0.0;
  double KeyElevationDeg = 0.0, KeyBearingDeg = 0.0;
  bool KeyFromClock = false;

  std::vector<Shows> Surfaces;
};

struct Extents {
  Vec3 LeastM = {{0.0, 0.0, 0.0}};
  Vec3 MostM = {{0.0, 0.0, 0.0}};
};

class Live {
public:
  ~Live();
  Live(const Live &) = delete;
  Live &operator=(const Live &) = delete;

  [[nodiscard]] static bool Open(Render::SceneRenderer &renderer,
                                 Declaration declaration,
                                 const Ui::Font *font,
                                 std::unique_ptr<Live> &out,
                                 std::string &error);
  [[nodiscard]] static bool Prepare(Render::SceneRenderer &renderer,
                                    Declaration declaration,
                                    const Ui::Font *font,
                                    std::unique_ptr<Live> &out,
                                    std::string &error);
  [[nodiscard]] static bool ReplacesGeometry(Render::SceneRenderer &renderer,
                                             const Live &previous,
                                             Geometry replacement,
                                             const Ui::Font *font,
                                             std::unique_ptr<Live> &out,
                                             std::string &error);
  [[nodiscard]] static bool PreparesGeometryReplacement(Render::SceneRenderer &renderer,
                                                        const Live &previous,
                                                        Geometry replacement,
                                                        const Ui::Font *font,
                                                        std::unique_ptr<Live> &candidate,
                                                        std::string &error);
  [[nodiscard]] static bool PreparesWorldReplacement(Render::SceneRenderer &renderer,
                                                     const Live &previous,
                                                     const Ui::Font *font,
                                                     std::unique_ptr<Live> &candidate,
                                                     std::string &error);
  [[nodiscard]] static bool PublishesPreparedWorld(Render::SceneRenderer &renderer,
                                                   std::unique_ptr<Live> &out,
                                                   std::unique_ptr<Live> &candidate,
                                                   std::string &error);

  [[nodiscard]] bool Carries(size_t bodies, std::string &error);
  [[nodiscard]] bool Redeclare(std::vector<Shows> surfaces, std::string &error);
  [[nodiscard]] const std::string &ProgrammeOf(size_t surface) const;

  void GroundIs(int surfaceIndex) { GroundSurface_ = surfaceIndex; }

  void Digests(bool yes) { Scratch_.Digests = yes; }

  [[nodiscard]] double BuildMs() const { return BuildMs_; }

  [[nodiscard]] double ReshapeMs() const { return ReshapeMs_; }

  [[nodiscard]] double ComposeMs() const { return ComposeMs_; }

  [[nodiscard]] double ReshapeAgainMs() const { return ReshapeAgainMs_; }

  [[nodiscard]] double ProxyStandsMs() const { return ProxyStandsMs_; }

  [[nodiscard]] double PlacesMs() const { return PlacesMs_; }

  [[nodiscard]] double WearsMs() const { return WearsMs_; }

  [[nodiscard]] double LampsMs() const { return LampsMs_; }

  [[nodiscard]] double LitMs() const { return LitMs_; }

  [[nodiscard]] double MediumMs() const { return MediumMs_; }

  [[nodiscard]] Render::SubjectTransferMetrics TransferMetrics() const { return Scratch_.Metrics; }

  [[nodiscard]] double FramingMs() const { return FramingMs_; }

  [[nodiscard]] size_t SkyIntegrations() const { return GroundAir_.Integrations(); }

  [[nodiscard]] double MeteredLux() const;

  void ReadIrradiance(std::span<const float, Render::kIrradianceFloats> irradiance);

  [[nodiscard]] std::span<const double, 3> AmbientStood() const { return AmbientStood_; }

  [[nodiscard]] std::span<const double, 3> GroundStood() const { return GroundStood_; }

  [[nodiscard]] double CarryMs() const { return CarryMs_; }

  [[nodiscard]] double ResolveMs() const { return ResolveMs_; }

  [[nodiscard]] double BoundsMs() const { return BoundsMs_; }

  [[nodiscard]] double InsideMs() const { return InsideMs_; }

  [[nodiscard]] double SurfaceMs() const { return SurfaceMs_; }

  [[nodiscard]] double StandMs() const { return StandMs_; }

  [[nodiscard]] double SubmitMs() const { return SubmitMs_; }

  [[nodiscard]] bool SetGeometry(outshine::Geometry &&built, size_t carried, std::string &error);
  [[nodiscard]] bool Reshape(std::string &error);

  [[nodiscard]] const Render::ClusterBuildMetrics &Clustering() const noexcept {
    return ShapeParts_.Clustering;
  }

  [[nodiscard]] bool SetGeometry(outshine::Geometry &&built,
                                 size_t carried,
                                 const Material &wearing,
                                 std::string &error);

  void ScaledBy(double metresPerUnit) { Declared_.MetresPerUnit = metresPerUnit; }

  [[nodiscard]] double ShadowRadiusStanding() const { return ShadowRadiusStoodM_; }

  [[nodiscard]] Render::ReadState Pyramid(Render::PyramidDepths &into) const {
    return Renderer_->ReadPyramid(into);
  }

  [[nodiscard]] const Vec3 &ShadowCentreStanding() const { return Renderer_->ShadowStoodAtM(); }

  struct Bearing {
    Mat4 WorldFromBodyM;
    Mat4 AsBuilt;
  };

  [[nodiscard]] bool Carry(size_t body, const Bearing &held, std::string &error);

  [[nodiscard]] bool Carry(const Bearing &held, std::string &error);

  [[nodiscard]] bool PlacedBounds(Extents &into, std::string &error);

  void SkyEye(double aboveGroundM);

  [[nodiscard]] bool Advance(std::string &error);
  [[nodiscard]] bool Draw(std::string &error);

  void Eye(const Render::Viewpoint &from) noexcept;

  [[nodiscard]] const Render::Viewpoint &Aimed() const { return Looking_.Eye; }

  [[nodiscard]] const Render::Viewpoint &Watching() const { return Eye_; }

  [[nodiscard]] const Declaration &Standing() const { return Declared_; }

  void Grounding(const Vec3 &albedo) {
    for (int channel = 0; channel < 3; ++channel) { GroundAlbedo_[channel] = albedo[channel]; }
  }

  [[nodiscard]] const Render::SubjectEnvironment &AmbientStanding() const {
    return Stood_.IndirectLight();
  }

  [[nodiscard]] size_t PartsStanding() const { return Stood_.Parts(); }

  [[nodiscard]] size_t InstancesStanding() const { return Stood_.Instances(); }

  [[nodiscard]] double NearStanding() const { return static_cast<double>(Renderer_->NearMetres()); }

  [[nodiscard]] static double NearestStandable() {
    return static_cast<double>(Render::SceneRenderer::kNearM);
  }

  [[nodiscard]] const double *PlacementStanding(size_t part) const {
    return Stood_.Placement(part).data();
  }

  [[nodiscard]] bool Watched() const { return HaveEye_; }

  void FrameItself() {
    HaveEye_ = false;
    Aim_ = AimState::Dirty;
  }

  [[nodiscard]] Ui::Touched Under(double xPx, double yPx, size_t &surface) const {
    return Over_.Under(xPx, yPx, surface);
  }

  [[nodiscard]] Holds<bool> Wheeled(double xPx, double yPx, double byPx, std::string &error) {
    auto previous = Over_.Scrolled();
    bool again = false;
    Over_.Wheeled(xPx, yPx, byPx, again);
    if (again && !Compose(error)) {
      Over_.Scrolled(std::move(previous));
      return std::unexpected(error);
    }
    return again;
  }

  [[nodiscard]] const std::vector<std::vector<Ui::Layout::Scrolled>> &Scrolled() const {
    return Over_.Scrolled();
  }

  [[nodiscard]] bool Scrolled(std::vector<std::vector<Ui::Layout::Scrolled>> kept,
                              std::string &error) {
    auto previous = Over_.Scrolled();
    Over_.Scrolled(std::move(kept));
    if (Compose(error)) { return true; }
    Over_.Scrolled(std::move(previous));
    return false;
  }

  [[nodiscard]] static size_t TookPosing() { return TookPosing_; }

  [[nodiscard]] static size_t TookSubmitting() { return TookSubmitting_; }

  [[nodiscard]] static size_t TookAiming() { return TookAiming_; }

  [[nodiscard]] static size_t TookDrawing() { return TookDrawing_; }

  [[nodiscard]] static size_t AssetReads() { return AssetReads_; }

  [[nodiscard]] static size_t PlanInits() { return PlanInits_; }

  [[nodiscard]] size_t PlanStages() const { return Plan_ ? Plan_->Order().size() : 0u; }

  [[nodiscard]] size_t PlanPasses() const { return Plan_ ? Plan_->Passes().size() : 0u; }

  [[nodiscard]] const Render::Shape &Shown() const { return Shaped_; }

  [[nodiscard]] size_t CarriedParts() const { return Joined_; }

  [[nodiscard]] bool Stands() const { return Stoodup_; }

  static constexpr int kSweepSamples = 16;

  [[nodiscard]] double AtS() const { return Held_.AtS(); }

  [[nodiscard]] double DurationS() const { return Held_.DurationS(); }

  [[nodiscard]] bool Moves() const { return Held_.Moves(); }

  [[nodiscard]] double LocalsDigest() const { return Held_.LocalsDigest(); }

  [[nodiscard]] double AssembledDigest() const { return Held_.AssembledDigest(); }

  [[nodiscard]] int Frames() const { return Held_.Frames(); }

private:
  friend class ::outshine::Engine;

  static void HandOffRenderer(std::unique_ptr<Live> &owner) noexcept {
    if (owner != nullptr) { owner->Renderer_ = nullptr; }
  }

  static size_t TookPosing_, TookSubmitting_, TookAiming_, TookDrawing_;
  static size_t AssetReads_;
  static size_t PlanInits_;
  Render::PlanSpec PlanDeclared_;

  Live(Render::SceneRenderer &renderer, Declaration declaration, const Ui::Font *font);

  struct Wearing {
    size_t Part = 0;
    uint32_t Slot = 0;
  };

  void
  PaintsPart(Wearing what, const Scenario::SurfaceOverride &said, std::vector<uint32_t> &wearers);
  [[nodiscard]] size_t WornByNodeOrPart();
  [[nodiscard]] size_t WornByNativeSurfaceAndPart(const Geometry &native, size_t firstPart);
  [[nodiscard]] size_t WornByNativeSurface(const Geometry &native);
  [[nodiscard]] size_t WornByNativeParts(const Geometry &native, size_t firstPart);
  [[nodiscard]] bool WearsOverrides(std::string &error);
  [[nodiscard]] bool RejectsUnwornOverrides(std::string &error) const;
  [[nodiscard]] Mat4 InMetres(const Mat4 &placed) const;
  void StandsEnvironment();
  void LightsFromTheSky(Render::SubjectEnvironment &environment) const;
  void EmitsPerPart();
  [[nodiscard]] bool StandsPlan(std::string &error);

  [[nodiscard]] bool DeclaresKeyLight() const {
    return Declared_.KeyLux > 0.0 || Declared_.KeyFromClock;
  }

  [[nodiscard]] Vec3f TowardTheKey() const;
  [[nodiscard]] PunctualLight KeyLight() const;

  void StandsKeyLight();
  void StandsShadowRadius();
  void ClearsSubject();
  [[nodiscard]] bool CarriesBuilt(std::string &error);
  [[nodiscard]] bool JoinsSubjects(std::string &error);
  [[nodiscard]] bool StandsSubjects(std::string &error);
  [[nodiscard]] bool AppendNativeSurfaceTable(const Geometry &native, std::string &error);
  [[nodiscard]] bool Build(std::string &error);
  [[nodiscard]] std::expected<void, std::string> BindSubject();
  [[nodiscard]] double Framing() const;
  [[nodiscard]] bool Pose(double seconds, std::string &error);
  [[nodiscard]] bool Measure(double seconds, std::string &error);

  [[nodiscard]] int Sweeps() const {
    const int frames = Held_.Frames();
    return std::clamp(frames, 1, kSweepSamples);
  }

  [[nodiscard]] double Seconds(int sample) const {
    const int over = Sweeps();
    return over > 1
               ? static_cast<double>(sample) * Held_.DurationS() / static_cast<double>(over - 1)
               : 0.0;
  }

  [[nodiscard]] bool Look(std::string &error);
  [[nodiscard]] bool Stand(std::string &error);
  [[nodiscard]] bool Submit(std::string &error);

  [[nodiscard]] bool Compose(std::string &error) { return Compose(Declared_.Surfaces, error); }

  [[nodiscard]] bool Compose(std::span<const Shows> surfaces, std::string &error) {
    return Over_.Compose(*Renderer_,
                         surfaces,
                         {.WidthPx = static_cast<double>(Declared_.SurfaceWidthPx),
                          .HeightPx = static_cast<double>(Declared_.SurfaceHeightPx)},
                         error);
  }

  Render::SceneRenderer *Renderer_ = nullptr;
  Overlay Over_;
  Declaration Declared_;
  Vec3 GroundAlbedo_ = kGroundAlbedoUnsaid;
  double ShadowRadiusStoodM_ = 0.0;
  std::shared_ptr<const Render::Compiled> Plan_;
  Render::Viewpoint Eye_;
  bool HaveEye_ = false;
  enum class AimState { Unbound, Bound, Dirty };
  AimState Aim_ = AimState::Unbound;
  std::vector<Mat4> SentBody_;
  Mat4 SentBuilt_{};

  std::vector<Box> PartBounds_;
  void CapturesRenderedPositions();
  void CoverShapedParts();
  [[nodiscard]] bool PartVolumes(std::string &error);
  Render::SurfaceTable Table_;
  size_t OverridesWorn_ = 0;

  [[nodiscard]] bool RestoresPieceResources(std::string &error);

  [[nodiscard]] bool RestoresGroundResources(std::string &error);
  Posed Held_;
  Render::SubjectProxy Stood_;
  Render::Eye Looking_;
  Render::SubjectScratch Scratch_;

  Render::ShapeStore ShapeParts_;
  Render::Shape Shaped_;
  std::vector<float> RenderedPositionsM_;
  int GroundSurface_ = -1;
  void WearsPieces();
  uint64_t ShapedAt_ = 0;
  bool EverShaped_ = false;
  double BuildMs_ = 0.0, StandMs_ = 0.0, SubmitMs_ = 0.0;
  double ReshapeMs_ = 0.0, ComposeMs_ = 0.0;
  double ReshapeAgainMs_ = 0.0, ProxyStandsMs_ = 0.0, PlacesMs_ = 0.0, WearsMs_ = 0.0;
  double LampsMs_ = 0.0, LitMs_ = 0.0, MediumMs_ = 0.0, FramingMs_ = 0.0;

  std::array<double, 3> AmbientStood_ = {0.0, 0.0, 0.0};
  std::array<double, 3> GroundStood_ = {0.0, 0.0, 0.0};

  [[nodiscard]] Medium DeclaredAir() const;

  GroundAtmosphere GroundAir_;
  double CarryMs_ = 0.0, ResolveMs_ = 0.0, BoundsMs_ = 0.0, InsideMs_ = 0.0, SurfaceMs_ = 0.0;

  bool Stoodup_ = false;
  size_t Joined_ = 0;
  size_t Carrying_ = 0;

  double Around_ = 0.0;
};

}
#endif
