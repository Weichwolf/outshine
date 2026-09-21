#include "Lens.h"
#ifndef OUTSHINE_RENDER_SCENERENDERER_H
#define OUTSHINE_RENDER_SCENERENDERER_H

#include "math/Mat4.h"
#include "math/Vec2.h"
#include "math/Vec3.h"
#include "Extent.h"
#include "Heap.h"
#include <array>
#include <span>
#include <cstdint>
#include <memory>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuSubmission.h"
#include "GroundStorage.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "scene/SceneResources.h"
#include "Viewing.h"
#include "Compiled.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/AerialPerspectiveStage.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/MediumMultiScatterStage.h"
#include "stages/DepthPyramidStage.h"
#include "stages/IrradianceStage.h"
#include "stages/MediumRadianceStage.h"
#include "stages/LightVisibilityStage.h"
#include "stages/SubjectCullStage.h"
#include "stages/SkyStage.h"
#include "stages/MediumTransmittanceStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

struct KeptDraws {
  uint32_t Indices = 0;
  uint32_t Batches = 0;
  std::vector<uint32_t> Arguments;
  std::vector<uint32_t> DrawIndex;
  std::vector<uint32_t> Visibility;
};

struct PyramidDepths {
  float Nearest = 0.0f;
  float Farthest = 1.0f;
  float Mean = 0.0f;
};

class SceneRenderer {
public:
  [[nodiscard]] std::expected<void, std::string> Init(Extent frame,
                                                      std::shared_ptr<const Compiled> plan);
  [[nodiscard]] bool BeginsWorldCandidate(std::string &error);
  [[nodiscard]] bool PublishesWorldCandidate(std::string &error);
  void AbandonsWorldCandidate() noexcept;

  [[nodiscard]] const Compiled &Plan() const { return *ActiveState().Plan; }

  [[nodiscard]] bool DeviceUsable() const { return ActiveState().Ready; }

  [[nodiscard]] const std::string &WhyNot() const { return ActiveState().WhyNot; }

  struct Shown {
    int WidthPx = 0;
    int HeightPx = 0;
  };

  [[nodiscard]] std::expected<void, std::string>
  DrawsInto(int widthPx, int heightPx, SDL_Window *presents);
  [[nodiscard]] std::expected<std::optional<Shown>, std::string_view> Presented() const;
  void StopShowing();

  [[nodiscard]] SDL_GPUTextureFormat SurfaceFormat() const;

  void PresentInto(SDL_GPUTexture *surface) { ActiveState().Frame.HostSurface = surface; }

  struct Region {
    double X = 0.0;
    double Y = 0.0;
    double Width = 0.0;
    double Height = 0.0;
    double Aspect = 0.0;
  };

  void SetPictureRegion(Region into) {
    ActiveState().RegionX = into.X;
    ActiveState().RegionY = into.Y;
    ActiveState().RegionW = into.Width;
    ActiveState().RegionH = into.Height;
    ActiveState().RegionAspect = into.Aspect;
  }

  [[nodiscard]] std::expected<void, std::string> RenderFrame();

  [[nodiscard]] bool Drew() const { return ActiveState().Submitted; }

  ~SceneRenderer() {
    WaitForGpu();
    StopShowing();
  }

  explicit SceneRenderer(GpuSubmission submission = {}) : Submission_(submission) {}

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  void WaitForGpu();

  static constexpr int kFramesInFlight = 2;

  [[nodiscard]] int SettleFrames() const {
    return ActiveState().Plan ? ActiveState().Plan->SettleFrames() : 1;
  }

  [[nodiscard]] bool Queued() const { return Presenting_ == SDL_GPU_PRESENTMODE_VSYNC; }

  [[nodiscard]] bool Presents() const { return Showing_ != nullptr; }

  [[nodiscard]] bool Settle(std::string &error);

  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] static bool Executable(Stage stage);

  void CastsBelow(uint32_t slot) { ActiveState().Frame.Shadow.CastsBelow(slot); }

  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = static_cast<float>(outshine::Camera::kNearestM);

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadKeptIndices(KeptDraws &into);

  using PieceRows = SceneResources::PieceRows;

  [[nodiscard]] std::expected<PieceHandle, std::string> PlacePiece(const PieceMesh &piece) {
    return ActiveState().Content.Resources.PlacePiece(ActiveState().Content.Subjects, piece);
  }

  [[nodiscard]] PieceId PlacePiece(const PieceMesh &piece, std::string &error) {
    return ActiveState().Content.Subjects.PlacePiece(piece, error);
  }

  void ReleasePiece(PieceHandle which) {
    ActiveState().Content.Resources.ReleasePiece(ActiveState().Content.Subjects, which);
  }

  void ReleasePiece(PieceId which) { ActiveState().Content.Subjects.ReleasePiece(which); }

  [[nodiscard]] PageId PlaceHeightPage(std::span<const float> nodes, std::string &error) {
    return ActiveState().Content.Subjects.Ground().PlacePage(nodes, error);
  }

  [[nodiscard]] std::expected<HeightPageHandle, std::string>
  PlaceHeightPage(std::span<const float> nodes) {
    return ActiveState().Content.Resources.PlaceHeightPage(ActiveState().Content.Subjects, nodes);
  }

  [[nodiscard]] bool HasHeightPage(HeightPageHandle which) const noexcept {
    return ActiveState().Content.Resources.HasHeightPage(which);
  }

  void ReleaseHeightPage(PageId which) {
    ActiveState().Content.Subjects.Ground().ReleasePage(which);
  }

  void ReleaseHeightPage(HeightPageHandle which) {
    ActiveState().Content.Resources.ReleaseHeightPage(ActiveState().Content.Subjects, which);
  }

  [[nodiscard]] size_t HeightPageSourceBytes() const noexcept {
    return ActiveState().Content.Resources.HeightPageSourceBytes();
  }

  [[nodiscard]] size_t HeightPageSlots() const noexcept {
    return ActiveState().Content.Resources.HeightPageSlots();
  }

  [[nodiscard]] size_t HeightPageSlotBytes() const noexcept {
    return ActiveState().Content.Resources.HeightPageSlotBytes();
  }

  [[nodiscard]] bool SetGroundGrid(std::span<const float> fractions, std::string &error) {
    return ActiveState().Content.Resources.SetGroundGrid(
        ActiveState().Content.Subjects, fractions, error);
  }

  [[nodiscard]] bool SetTerrainTiles(std::span<const TerrainTile> real,
                                     std::span<const TerrainTile> virtual_,
                                     std::string &error) {
    return ActiveState().Content.Resources.SetTerrainTiles(
        ActiveState().Content.Subjects, real, virtual_, error);
  }

  [[nodiscard]] bool SetGroundLattice(std::span<const GroundTile> real,
                                      std::span<const GroundTile> virtual_,
                                      std::string &error) {
    return ActiveState().Content.Subjects.Ground().SetInstances(real, virtual_, error);
  }

  [[nodiscard]] uint32_t GroundLatticeTriangles() const {
    return ActiveState().Content.Subjects.Ground().Triangles();
  }

  [[nodiscard]] bool
  SetPieceInstances(PieceHandle which, std::span<const Mat4> rows, std::string &error) {
    return ActiveState().Content.Resources.SetPieceInstances(
        ActiveState().Content.Subjects, which, rows, error);
  }

  [[nodiscard]] bool
  SetPieceInstances(PieceId which, std::span<const Mat4> rows, std::string &error) {
    return ActiveState().Content.Subjects.SetPieceInstances(which, rows, error);
  }

  [[nodiscard]] bool SetPieceInstances(std::span<const PieceRows> pieces, std::string &error) {
    return ActiveState().Content.Resources.SetPieceInstances(
        ActiveState().Content.Subjects, pieces, error);
  }

  [[nodiscard]] bool RestorePieces(std::string &error) {
    return ActiveState().Content.Resources.RestorePieces(ActiveState().Content.Subjects, error);
  }

  [[nodiscard]] std::expected<uint32_t, std::string> RegisterPieceMaterials(Geometry source) {
    auto &content = ActiveState().Content;
    return content.Resources.RegisterPieceMaterials(
        content.Subjects, content.DrawsGlass ? &content.Glass : nullptr, std::move(source));
  }

  [[nodiscard]] bool RestorePieceMaterials(std::string &error) {
    auto &content = ActiveState().Content;
    return content.Resources.RestorePieceMaterials(
        content.Subjects, content.DrawsGlass ? &content.Glass : nullptr, error);
  }

  [[nodiscard]] size_t PieceSourceBytes() const noexcept {
    return ActiveState().Content.Resources.PieceSourceBytes();
  }

  [[nodiscard]] size_t PieceSlots() const noexcept {
    return ActiveState().Content.Resources.PieceSlots();
  }

  [[nodiscard]] size_t PieceSlotBytes() const noexcept {
    return ActiveState().Content.Resources.PieceSlotBytes();
  }

  void SetNativePieceSurfaces(std::span<const uint32_t> slots) {
    ActiveState().Content.Subjects.SetNativePieceSurfaces(slots);
  }

  [[nodiscard]] uint32_t PiecesStanding() const {
    return ActiveState().Content.Subjects.PiecesStanding();
  }

  [[nodiscard]] uint32_t PieceTriangles() const {
    return ActiveState().Content.Subjects.PieceTriangles();
  }

  [[nodiscard]] size_t TakeUploadAttempts() {
    return ActiveState().Content.Subjects.Owned().TakeUploadAttempts();
  }

  [[nodiscard]] size_t TotalUploadAttempts() const {
    return ActiveState().Content.Subjects.Resident().TotalUploadAttempts();
  }

  [[nodiscard]] size_t RecordedCrossings() const {
    return ActiveState().Content.Subjects.Resident().RecordedCrossings();
  }

  [[nodiscard]] size_t TakeUploadBytes() {
    return ActiveState().Content.Subjects.Owned().TakeUploadBytes();
  }

  [[nodiscard]] size_t TakeBufferAllocationAttempts() {
    return ActiveState().Content.Subjects.Owned().TakeBufferAllocationAttempts();
  }

  [[nodiscard]] size_t TakeStagingAllocationAttempts() {
    return ActiveState().Content.Subjects.Owned().TakeStagingAllocationAttempts();
  }

  [[nodiscard]] uint32_t PieceBytesHeld() const {
    return ActiveState().Content.Subjects.Resident().HeldBytes();
  }

  [[nodiscard]] ReadState ReadSkyIrradiance(std::span<float, kIrradianceFloats> out);

  [[nodiscard]] ReadState ReadPyramid(PyramidDepths &into);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool ReplaceOverlay(std::span<const OverlayQuad> quads,
                                    const OverlayDraw::AtlasPixels *atlas,
                                    std::string &error) {
    return ActiveState().Content.Overlay.Replace(
        ActiveState().Frame.Handles, quads.data(), quads.size(), atlas, error);
  }

  [[nodiscard]] bool SetOverlay(const OverlayQuad *quads, size_t count, std::string &error) {
    return ActiveState().Content.Overlay.SetQuads(ActiveState().Frame.Handles, quads, count, error);
  }

  [[nodiscard]] bool
  SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return ActiveState().Content.Overlay.SetAtlas(
        ActiveState().Frame.Handles, rgba, width, height, error);
  }

  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    const Heap::Tagged relaying("mesh-relay");
    if (ActiveState().Content.DrawsGlass &&
        !ActiveState().Content.Glass.ValidateMesh(mesh, error)) {
      return false;
    }
    return ActiveState().Content.Subjects.SetMesh(mesh, error) &&
           (!ActiveState().Content.DrawsGlass || ActiveState().Content.Glass.SetMesh(mesh, error));
  }

  [[nodiscard]] bool SubjectPlacementRows(size_t rows, std::string &error) {
    return ActiveState().Content.Subjects.PlacementRows(rows, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.PlacementRows(rows, error));
  }

  void MoveSubjectPlacement(size_t slot, const Mat4 &model) {
    ActiveState().Content.Subjects.MovePlacement(slot, model);
    if (ActiveState().Content.DrawsGlass) {
      ActiveState().Content.Glass.MovePlacement(slot, model);
    }
  }

  [[nodiscard]] bool HandSubjectPlacements(std::string &error) {
    return ActiveState().Content.Subjects.HandPlacements(false, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.HandPlacements(false, error));
  }

  [[nodiscard]] size_t SubjectPlacementsMoved() const {
    return ActiveState().Content.Subjects.PlacementsMoved();
  }

  [[nodiscard]] uint32_t SubjectBytesStaged() const {
    return ActiveState().Content.Subjects.StagedBytes();
  }

  void ForgetSubjectStaging() { ActiveState().Content.Subjects.ForgetStagedCount(); }

  [[nodiscard]] const Vec3 &ShadowStoodAtM() const { return ActiveState().Frame.Shadow.StoodAtM(); }

  [[nodiscard]] bool SetSubjectPlacements(const double *models, size_t rows, std::string &error) {
    return ActiveState().Content.Subjects.SetPlacements(models, rows, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetSubjectPose(const SubjectPose &pose, std::string &error) {
    return ActiveState().Content.Subjects.SetPose(pose, error) &&
           (!ActiveState().Content.DrawsGlass || ActiveState().Content.Glass.SetPose(pose, error));
  }

  [[nodiscard]] bool SetSubjectMaterials(std::span<const SubjectMaterial> materials,
                                         std::string &error) {
    return ActiveState().Content.Subjects.SetMaterials(materials, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.SetMaterials(materials, error));
  }

  [[nodiscard]] bool AppendSubjectMaterials(std::span<const SubjectMaterial> materials,
                                            std::string &error) {
    if (!ActiveState().Content.Subjects.ValidateMaterials(materials, error) ||
        (ActiveState().Content.DrawsGlass &&
         !ActiveState().Content.Glass.ValidateMaterials(materials, error))) {
      return false;
    }
    return ActiveState().Content.Subjects.AppendMaterials(materials, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.AppendMaterials(materials, error));
  }

  [[nodiscard]] bool SetSubjectLights(std::span<const SubjectLight> lights, std::string &error) {
    return ActiveState().Content.Subjects.SetLights(lights, error) &&
           (!ActiveState().Content.DrawsGlass ||
            ActiveState().Content.Glass.SetLights(lights, error));
  }

  void SetMedium(const Medium &medium) {
    ActiveState().Medium = medium;
    ActiveState().Frame.MediumTransmittance.Declare(medium);
    ActiveState().Frame.MultiScatter.Declare(medium);
    ActiveState().Frame.Radiance.Declare(
        medium, ActiveState().CosSunZenith, ActiveState().EyeHeightM);
    ActiveState().Frame.SkyIrradianceStage.Declare(medium, ActiveState().CosSunZenith);
  }

  void SetShadowFrame(const Vec3f &toSun, const Vec3f &up, double radiusM) {
    ActiveState().Frame.Shadow.Declare({.ToSun = toSun, .Up = up}, radiusM);
  }

  void SetSky(const Vec3f &toSun, const Vec3f &up, float illuminanceLux, float eyeHeightM) {
    ActiveState().CosSunZenith = toSun[0] * up[0] + toSun[1] * up[1] + toSun[2] * up[2];
    ActiveState().EyeHeightM = eyeHeightM;
    ActiveState().Frame.Radiance.Declare(
        ActiveState().Medium, ActiveState().CosSunZenith, ActiveState().EyeHeightM);
    ActiveState().Frame.SkyIrradianceStage.Declare(ActiveState().Medium,
                                                   ActiveState().CosSunZenith);
    const SkyStanding stands = {
        .SunDir = toSun, .WorldUp = up, .IlluminanceLux = illuminanceLux, .EyeHeightM = eyeHeightM};
    ActiveState().Frame.Sky.Declare(ActiveState().Medium, stands);
    ActiveState().Frame.Aerial.Declare(ActiveState().Medium, stands);
  }

  void SetSkyEye(float eyeHeightM) {
    if (!ActiveState().Frame.Sky.Stands()) { return; }
    ActiveState().EyeHeightM = eyeHeightM;
    ActiveState().Frame.Radiance.Declare(
        ActiveState().Medium, ActiveState().CosSunZenith, ActiveState().EyeHeightM);
    ActiveState().Frame.SkyIrradianceStage.Declare(ActiveState().Medium,
                                                   ActiveState().CosSunZenith);
    ActiveState().Frame.Sky.Eye(ActiveState().Medium, eyeHeightM);
    ActiveState().Frame.Aerial.Eye(ActiveState().Medium, eyeHeightM);
  }

  [[nodiscard]] SDL_GPUTexture *SkyViewTable() const {
    return ActiveState().Frame.SkyViewLut.Get();
  }

  [[nodiscard]] SDL_GPUTexture *MultiScatterTable() const {
    return ActiveState().Frame.MultiScatterLut.Get();
  }

  [[nodiscard]] SDL_GPUTexture *TransmittanceTable() const {
    return ActiveState().Frame.TransmittanceLut.Get();
  }

  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    ActiveState().Content.Subjects.SetEnvironment(environment);
    if (ActiveState().Content.DrawsGlass) {
      ActiveState().Content.Glass.SetEnvironment(environment);
    }
  }

  [[nodiscard]] uint32_t SubjectBatchCount() const {
    return ActiveState().Content.Subjects.BatchCount();
  }

  [[nodiscard]] uint32_t SubjectBatchesTaking(VertexLayout layout) const {
    uint32_t many = 0;
    for (const DrawBatch &batch : ActiveState().Content.Subjects.Drawn()) {
      many += batch.Layout == layout ? 1u : 0u;
    }
    return many;
  }

  [[nodiscard]] size_t ShadowCastCount() const { return ActiveState().Frame.Shadow.CastBatches(); }

  [[nodiscard]] size_t ShadowedFrames() const {
    return ActiveState().Content.Subjects.ShadowedFrames();
  }

  struct Effort {
    double TookMs = 0.0;
    uint32_t DeviceBytes = 0;
    uint32_t Draws = 0;
    uint32_t Triangles = 0;
    uint32_t Surfaces = 0;
    uint32_t Placements = 0;
    uint32_t Textured = 0;
    uint32_t Palettes = 0;
    uint32_t Distinct = 0;
    uint32_t Layouts = 0;
  };

  [[nodiscard]] const Effort &Spent(Stage stage) const {
    return Spent_[static_cast<size_t>(stage)];
  }

  [[nodiscard]] size_t SubjectUniformPushes() const {
    return ActiveState().Content.Subjects.UniformPushes() +
           ActiveState().Content.Glass.UniformPushes();
  }

  [[nodiscard]] float ExposureApplied() const {
    return ActiveState().Plan ? ActiveState().Plan->Exposure() : 0.0f;
  }

  [[nodiscard]] uint32_t SubjectDrawCount() const {
    return ActiveState().Content.Subjects.DrawCount();
  }

  [[nodiscard]] uint32_t SubjectPipelineCount() const {
    return ActiveState().Content.Subjects.PipelineCount();
  }

  void SetCamera(const CameraBasis &basis, const Lens &lens) noexcept;

  [[nodiscard]] bool SetGroundClasses(std::span<const uint32_t> classes,
                                      std::span<const float> palette,
                                      std::string &error);

  [[nodiscard]] bool RestoreGroundResources(std::string &error) {
    auto &content = ActiveState().Content;
    if (!SetGroundClasses(
            content.Resources.GroundClasses(), content.Resources.GroundPalette(), error)) {
      return false;
    }
    return content.Resources.RestoreTerrain(content.Subjects, error);
  }

  [[nodiscard]] float NearMetres() const { return ActiveState().NearM; }

  void BeginTemporalRun();

  [[nodiscard]] int SceneW() const { return ActiveState().Frame.Width; }

  [[nodiscard]] int SceneH() const { return ActiveState().Frame.Height; }

  [[nodiscard]] double PictureW() const;
  [[nodiscard]] double PictureH() const;

  [[nodiscard]] double SceneAspect() const {
    return PictureH() > 0 ? PictureW() / PictureH() : 0.0;
  }

private:
  struct FrameResources;

  [[nodiscard]] std::expected<void, std::string> PrepareFrame();
  GpuSubmission Submission_;
  std::array<Effort, kStageCount> Spent_ = {{}};

  void Create(FrameResources &frame, const Compiled &plan, Resource resource);
  [[nodiscard]] static bool Created(const FrameResources &frame, Resource resource);
  [[nodiscard]] bool Configure(Stage stage,
                               FrameResources &frame,
                               const Compiled &plan,
                               bool drawsGlass,
                               std::string &error);
  [[nodiscard]] bool
  ConfigurePlanStages(FrameResources &frame, const Compiled &plan, bool drawsGlass);
  [[nodiscard]] static AttachmentSet
  ColoursForStage(const FrameResources &frame, const Compiled &plan, Stage wanted);
  void EncodeStage(Stage stage, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (*Configure)(SceneRenderer &renderer,
                      FrameResources &frame,
                      const Compiled &plan,
                      bool drawsGlass,
                      std::string &error);
    void (SceneRenderer::*Encode)(const FrameContext &ctx, const PassRecording &into);
  };

  static constexpr size_t kExecutorCount = 18;
  static const std::array<Executor, kExecutorCount> kExecutors;
  [[nodiscard]] static const Executor *ExecutorOf(Stage stage);
  void Picture(bool picture, const PassRecording &into);
  [[nodiscard]] static bool ConfigureSubjects(SceneRenderer &renderer,
                                              FrameResources &frame,
                                              const Compiled &plan,
                                              bool drawsGlass,
                                              std::string &error);
  [[nodiscard]] static bool ConfigureGlass(SceneRenderer &renderer,
                                           FrameResources &frame,
                                           const Compiled &plan,
                                           bool drawsGlass,
                                           std::string &error);
  [[nodiscard]] static bool ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                           FrameResources &frame,
                                                           const Compiled &plan,
                                                           bool drawsGlass,
                                                           std::string &error);
  [[nodiscard]] static bool ConfigureOverlay(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigurePresent(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureTonemap(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const Compiled &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureMediumTransmittance(SceneRenderer &renderer,
                                                         FrameResources &frame,
                                                         const Compiled &plan,
                                                         bool drawsGlass,
                                                         std::string &error);
  [[nodiscard]] static bool ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                        FrameResources &frame,
                                                        const Compiled &plan,
                                                        bool drawsGlass,
                                                        std::string &error);
  [[nodiscard]] static bool ConfigureMediumRadiance(SceneRenderer &renderer,
                                                    FrameResources &frame,
                                                    const Compiled &plan,
                                                    bool drawsGlass,
                                                    std::string &error);
  [[nodiscard]] static bool ConfigureIrradiance(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const Compiled &plan,
                                                bool drawsGlass,
                                                std::string &error);
  [[nodiscard]] static bool ConfigureDepthPyramid(SceneRenderer &renderer,
                                                  FrameResources &frame,
                                                  const Compiled &plan,
                                                  bool drawsGlass,
                                                  std::string &error);
  [[nodiscard]] static bool ConfigureSky(SceneRenderer &renderer,
                                         FrameResources &frame,
                                         const Compiled &plan,
                                         bool drawsGlass,
                                         std::string &error);
  [[nodiscard]] static bool ConfigureAerialPerspective(SceneRenderer &renderer,
                                                       FrameResources &frame,
                                                       const Compiled &plan,
                                                       bool drawsGlass,
                                                       std::string &error);
  [[nodiscard]] static bool ConfigureLightVisibility(SceneRenderer &renderer,
                                                     FrameResources &frame,
                                                     const Compiled &plan,
                                                     bool drawsGlass,
                                                     std::string &error);
  [[nodiscard]] static bool ConfigureSubjectCull(SceneRenderer &renderer,
                                                 FrameResources &frame,
                                                 const Compiled &plan,
                                                 bool drawsGlass,
                                                 std::string &error);
  void EncodeSubjects(const FrameContext &ctx, const PassRecording &into);
  void EncodeGlass(const FrameContext &ctx, const PassRecording &into);
  void EncodeCompositeTransmission(const FrameContext &ctx, const PassRecording &into);
  void EncodeOverlay(const FrameContext &ctx, const PassRecording &into);
  void EncodePresent(const FrameContext &ctx, const PassRecording &into);
  void EncodeTonemap(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumTransmittance(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumMultiScatter(const FrameContext &ctx, const PassRecording &into);
  void EncodeMediumRadiance(const FrameContext &ctx, const PassRecording &into);

  void EncodeIrradiance(const FrameContext &ctx, const PassRecording &into);
  void EncodeDepthPyramid(const FrameContext &ctx, const PassRecording &into);
  [[nodiscard]] EyeBasis Eye() const;
  void EncodeSky(const FrameContext &ctx, const PassRecording &into);
  void EncodeAerialPerspective(const FrameContext &ctx, const PassRecording &into);
  void EncodeLightVisibility(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectCull(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectScan(const FrameContext &ctx, const PassRecording &into);
  void EncodeSubjectCompact(const FrameContext &ctx, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass, StageSubmission &submission);
  void EncodeComputePass(SDL_GPUCommandBuffer *commands,
                         const Compiled::Pass &declared,
                         StageSubmission &submission);
  void EncodeGraphicsPass(SDL_GPUCommandBuffer *commands,
                          const Compiled::Pass &declared,
                          StageSubmission &submission);
  [[nodiscard]] FrameContext Framing() const;
  void SettleShadow();
  std::array<bool, kResourceCount> Touched_ = {{}};
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] static SDL_GPUTexture *Target(const FrameResources &frame, Resource resource);
  void BindFrameResources();

  [[nodiscard]] SDL_GPUBuffer *BufferFor(Resource resource) const;
  [[nodiscard]] DisplayOptions Display() const;

  [[nodiscard]] SDL_GPUTexture *LinearSource() const;
  [[nodiscard]] SDL_GPUTexture *DisplaySource() const;

  OwnedDevice Device_;

  struct WorldContent {
    GroundStorage Ground;
    SceneResources Resources;
    SubjectDraw Subjects;
    SubjectDraw Glass;
    OverlayDraw Overlay;
    bool DrawsGlass = false;

    WorldContent() = default;
    WorldContent(const WorldContent &) = delete;
    WorldContent &operator=(const WorldContent &) = delete;
    WorldContent(WorldContent &&) noexcept = default;
    WorldContent &operator=(WorldContent &&) noexcept = default;
  };

  struct FrameResources {
    SDL_GPUTexture *HostSurface = nullptr;
    Shown Shown;
    OwnedTexture Offscreen;
    Gpu Handles;
    OwnedTexture HdrTex, VelTex, DepthTex, FrameTex;
    OwnedTexture TransmittanceLut, MultiScatterLut, SkyViewLut;
    OwnedTexture ShadowAtlas;
    OwnedTexture TransmissiveTex, CompositedTex, AerialTex;
    OwnedTexture ShadingNormalTex;
    OwnedTexture SurfaceIdentityTex;
    OwnedSampler Samp, LutSamp;
    OwnedBuffer IrradianceBuffer;
    OwnedBuffer Pyramid;
    Readback PyramidRead;
    int Width = 0, Height = 0;
    std::array<OwnedTexture, 2> LinearTex{};
    int LinearAt = 0;
    bool HistoryHeld = false;
    CompositeTransmissionStage CompositeTransmission;
    AerialPerspectiveStage Aerial;
    TonemapStage Tonemap;
    MediumTransmittanceStage MediumTransmittance;
    MediumMultiScatterStage MultiScatter;
    MediumRadianceStage Radiance;
    IrradianceStage SkyIrradianceStage;
    DepthPyramidStage PyramidStage;
    SkyStage Sky;
    LightVisibilityStage Shadow;
    SubjectCullStage Cull;
    OverlayPipeline OverlayPipe;
    PresentStage Present;
    SubjectPipelineBinding SubjectPipelines;
    SubjectPipelineBinding GlassPipelines;
  };

  struct SceneState {
    FrameResources Frame;
    std::shared_ptr<const Compiled> Plan;
    WorldContent Content;
    Medium Medium = kEarthAir;
    float CosSunZenith = 1.0f;
    float EyeHeightM = 0.0f;
    bool HistoryStarted = false;
    int JitterAt = 0;
    Vec2f Jitter = {{0.0f, 0.0f}};
    Vec2f PrevJitter = {{0.0f, 0.0f}};
    bool CameraFull = false;
    double RegionX = 0.0;
    double RegionY = 0.0;
    double RegionW = 0.0;
    double RegionH = 0.0;
    double RegionAspect = 0.0;
    CameraBasis Camera;
    float FovDeg = 0.0f;
    float OrthoWidthM = 0.0f;
    float OrthoM = 0.0f;
    float NearM = kNearM;
    float FarM = 0.0f;
    bool Submitted = false;
    bool Ready = false;
    std::string WhyNot;
    Vec3 PrevEye = {{0, 0, 0}};
    Mat4f PrevMvp = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

    SceneState() = default;
    SceneState(const SceneState &) = delete;
    SceneState &operator=(const SceneState &) = delete;
    SceneState(SceneState &&) noexcept = default;
    SceneState &operator=(SceneState &&) noexcept = default;
  };

  static_assert(std::is_nothrow_move_constructible_v<FrameResources>);
  static_assert(std::is_nothrow_move_assignable_v<FrameResources>);
  static_assert(std::is_nothrow_move_constructible_v<WorldContent>);
  static_assert(std::is_nothrow_move_assignable_v<WorldContent>);
  static_assert(std::is_nothrow_move_constructible_v<SceneState>);
  static_assert(std::is_nothrow_move_assignable_v<SceneState>);

  bool Stands();
  [[nodiscard]] std::expected<void, std::string>
  InitForTarget(Extent frame, std::shared_ptr<const Compiled> plan, bool presents);
  [[nodiscard]] std::expected<void, std::string>
  StandsOffscreen(FrameResources &frame, const Compiled *plan, bool presents);
  [[nodiscard]] std::expected<OwnedTexture, std::string> MakeOffscreen(const Compiled *plan,
                                                                       Extent frame);
  [[nodiscard]] std::expected<SDL_GPUPresentMode, std::string> ClaimWindow(SDL_Window *window);

  SDL_GPUPresentMode Presenting_ = SDL_GPU_PRESENTMODE_VSYNC;
  SDL_Window *Showing_ = nullptr;
  static constexpr int kJitterPeriod = 8;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };

  [[nodiscard]] Placed PictureRect() const;
  [[nodiscard]] Lens Through() const;

  [[nodiscard]] SceneState &ActiveState() noexcept { return Candidate_ ? *Candidate_ : State_; }

  [[nodiscard]] const SceneState &ActiveState() const noexcept {
    return Candidate_ ? *Candidate_ : State_;
  }

  std::array<SDL_GPUFence *, kFramesInFlight> Landed_ = {};
  int LandedAt_ = 0;
  SceneState State_;
  std::optional<SceneState> Candidate_;
};

}
#endif
