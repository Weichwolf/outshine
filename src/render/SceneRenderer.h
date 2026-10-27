#include "SceneLighting.h"
#include "MeshInput.h"
#include "Lens.h"
#ifndef OUTSHINE_RENDER_SCENERENDERER_H
#define OUTSHINE_RENDER_SCENERENDERER_H

#include "Mat4.h"
#include "GeometryHierarchy.h"
#include "Vec2.h"
#include "Vec3.h"
#include "Extent.h"
#include <RenderConfiguration.h>
#include "Heap.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <cstdint>
#include <memory>
#include <expected>
#include <utility>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct SDL_Window;

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuSubmission.h"
#include "FrameTimeline.h"
#include "GpuFrameSample.h"
#include "ViewHistory.h"
#include "FrameTargets.h"
#include "atmosphere/AtmospherePasses.h"
#include "TextureFormat.h"
#include "UploadQueue.h"
#include "TerrainMaterialBuffers.h"

#include "Readback.h"
#include "scene/SceneResources.h"
#include "scene/SceneRenderState.h"
#include "scene/SceneCandidateEdit.h"
#include "Viewing.h"
#include "ExecutionPlan.h"
#include "stages/OverlayDraw.h"
#include "stages/PresentStage.h"

#include "stages/Resolve.h"
#include "stages/GeometryRenderer.h"
#include "stages/GeometryResidency.h"
#include "stages/AerialPerspectiveStage.h"
#include "stages/CompositeTransmissionStage.h"
#include "visibility/DepthPyramidPass.h"
#include "shadow/SunShadowPass.h"
#include "stages/GeometryCullStage.h"
#include "stages/SkyStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

struct GroundClassUploadMetrics {
  TerrainMaterialBuffersUploadMetrics Storage;
  double SourcePreparationMs = 0.0;
  double RestoreSourceMs = 0.0;
};

enum class RenderFramePhase : size_t {
  Prepare,
  Acquire,
  Upload,
  Swapchain,
  Cull,
  Encode,
  FenceWait,
  Submit,
  Finish,
  Count
};

struct RenderFrameTiming {
  std::array<double, static_cast<size_t>(RenderFramePhase::Count)> PhaseMs{};
  double TotalMs = 0.0;
};

enum class GeometryReadback { Counters, Indices };

struct KeptDraws {
  uint32_t Indices = 0;
  uint32_t Batches = 0;
  GeometryQueueState RootWork;
  GeometryQueueState ClusterWork;
  uint64_t ReadbackBytes = 0;
  std::vector<uint32_t> Arguments;
  std::vector<uint32_t> DrawIndex;
  std::vector<uint32_t> Visibility;
};

struct PyramidDepths {
  float Nearest = 0.0f;
  float Farthest = 1.0f;
  float Mean = 0.0f;
};

struct SkyDeclaration {
  Vec3f ToSun;
  Vec3f Up;
  float IlluminanceLux = 0.0f;
  float EyeHeightM = 0.0f;
};

class SceneRenderer {
public:
  enum class GroundSources : uint8_t { Default, Published };
  enum class OverlaySource : uint8_t { Candidate, Published };

  [[nodiscard]] std::expected<void, std::string> Init(Extent frame,
                                                      std::shared_ptr<const ExecutionPlan> plan);

  [[nodiscard]] UploadQueue::WaitScope OfflineUploads(std::chrono::nanoseconds budget) {
    return {UploadBudget_, budget};
  }

  [[nodiscard]] bool
  BeginsWorldCandidate(std::string &error,
                       SceneResources::PieceSources pieces = SceneResources::PieceSources::Copy,
                       GroundSources ground = GroundSources::Default);
  [[nodiscard]] bool PublishesWorldCandidate(std::string &error,
                                             OverlaySource overlay = OverlaySource::Candidate);
  void AbandonsWorldCandidate() noexcept;

  [[nodiscard]] bool HasWorldCandidate() const noexcept { return Candidate_.has_value(); }

  [[nodiscard]] const ExecutionPlan &Plan() const { return *ActiveState().Plan; }

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

  void SetContentSelection(const ContentSelection &selection);

  [[nodiscard]] Native::Format SurfaceFormat() const;

  void PresentInto(Native::Texture *surface) { ActiveFrame().HostSurface = surface; }

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

  [[nodiscard]] std::expected<Native::Completion, std::string> PrepareWorldResources();

  [[nodiscard]] bool WorldResourcesComplete(const Native::Completion &completion);

  [[nodiscard]] std::expected<void, std::string> RenderFrame();

  [[nodiscard]] bool Drew() const { return ActiveState().History.HasSubmittedFrame(); }

  ~SceneRenderer() {
    WaitForGpu();
    StopShowing();
  }

  explicit SceneRenderer(GpuSubmission submission = {}) : Submission_(submission) {}

  SceneRenderer(const SceneRenderer &) = delete;
  SceneRenderer &operator=(const SceneRenderer &) = delete;

  void WaitForGpu();

  static constexpr int kFramesInFlight = static_cast<int>(FrameTimeline::Capacity);

  [[nodiscard]] int SettleFrames() const {
    return ActiveState().Plan ? ActiveState().Plan->SettleFrames() : 1;
  }

  [[nodiscard]] bool Queued() const { return Presenting_; }

  [[nodiscard]] bool Presents() const { return Showing_ != nullptr; }

  [[nodiscard]] bool Settle(std::string &error);

  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  [[nodiscard]] static bool Executable(Stage stage);

  void CastsBelow(uint32_t slot) { ActiveFrame().Shadow.CastsBelow(slot); }

  [[nodiscard]] ReadState ReadShadowAtlas(std::vector<float> &depth);
  static constexpr float kNearM = static_cast<float>(outshine::Camera::kNearestM);

  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);

  // Explicit readback of the last accepted selection; counters omit index/visibility payloads.
  [[nodiscard]] ReadState ReadGeometrySelection(KeptDraws &into, GeometryReadback detail);

  [[nodiscard]] Native::Result<TerrainRenderer::DrawArguments> ReadGroundDrawArguments() const;

  using PieceRows = SceneResources::PieceRows;

  [[nodiscard]] std::expected<PieceHandle, std::string> PlacePiece(const PieceMesh &piece) {
    return ActiveContent().Scene().Resources().PlacePiece(ActiveContent().Geometry(), piece);
  }

  [[nodiscard]] std::expected<PieceHandle, std::string>
  PlaceStructurePiece(const PieceMesh &piece) {
    return ActiveContent().Scene().Resources().PlacePiece(
        ActiveContent().Geometry(), piece, SceneResources::PieceOwner::StructureTile);
  }

  [[nodiscard]] bool HasPieceSource(PieceHandle which) const noexcept {
    return ActiveContent().Scene().Resources().HasPiece(which);
  }

  [[nodiscard]] std::expected<size_t, std::string>
  ReconcileStructurePieces(std::span<const PieceHandle> held) {
    return ActiveContent().Scene().Resources().ReconcileStructurePieces(ActiveContent().Geometry(),
                                                                        held);
  }

  [[nodiscard]] PieceId PlacePiece(const PieceMesh &piece, std::string &error) {
    auto placed = ActiveContent().Geometry().PlacePiece(piece);
    if (!placed) {
      error = std::move(placed.error().Message);
      return kNoPiece;
    }
    return *placed;
  }

  void ReleasePiece(PieceHandle which) {
    ActiveContent().Scene().Resources().ReleasePiece(ActiveContent().Geometry(), which);
  }

  void ReleasePiece(PieceId which) { ActiveContent().Geometry().ReleasePiece(which); }

  [[nodiscard]] PageId PlaceHeightPage(std::span<const float> nodes, std::string &error) {
    auto placed = ActiveContent().Geometry().Ground().PlacePage(nodes);
    if (!placed) {
      error = std::move(placed.error().Message);
      return kNoPage;
    }
    return *placed;
  }

  [[nodiscard]] Native::Result<HeightPageHandle> PlaceHeightPage(std::span<const float> nodes) {
    return ActiveContent().Scene().Resources().PlaceHeightPage(ActiveContent().Geometry(), nodes);
  }

  [[nodiscard]] Native::Result<std::vector<HeightPageHandle>>
  PlaceHeightPages(std::span<const HeightPageView> nodes) {
    return ActiveContent().Scene().Resources().PlaceHeightPages(ActiveContent().Geometry(), nodes);
  }

  [[nodiscard]] bool HasHeightPage(HeightPageHandle which) const noexcept {
    return ActiveContent().Scene().Resources().HasHeightPage(which);
  }

  void ReleaseHeightPage(PageId which) { ActiveContent().Geometry().Ground().ReleasePage(which); }

  void ReleaseHeightPage(HeightPageHandle which) {
    ActiveContent().Scene().Resources().ReleaseHeightPage(ActiveContent().Geometry(), which);
  }

  [[nodiscard]] size_t HeightPageSourceBytes() const noexcept {
    return ActiveContent().Scene().Resources().HeightPageSourceBytes();
  }

  [[nodiscard]] size_t HeightPageSlots() const noexcept {
    return ActiveContent().Scene().Resources().HeightPageSlots();
  }

  [[nodiscard]] size_t HeightPageSlotBytes() const noexcept {
    return ActiveContent().Scene().Resources().HeightPageSlotBytes();
  }

  [[nodiscard]] bool SetGroundGrid(std::span<const float> fractions, std::string &error) {
    return Accepted(SetGroundGrid(fractions), error);
  }

  [[nodiscard]] Native::Result<void> SetGroundGrid(std::span<const float> fractions) {
    return ActiveContent().Scene().Resources().SetGroundGrid(ActiveContent().Geometry(), fractions);
  }

  [[nodiscard]] bool SetTerrainTiles(std::span<const TerrainTile> real,
                                     std::span<const TerrainTile> virtual_,
                                     std::string &error) {
    return Accepted(SetTerrainTiles(real, virtual_), error);
  }

  [[nodiscard]] Native::Result<void> SetTerrainTiles(std::span<const TerrainTile> real,
                                                     std::span<const TerrainTile> virtual_) {
    return ActiveContent().Scene().Resources().SetTerrainTiles(
        ActiveContent().Geometry(), real, virtual_);
  }

  [[nodiscard]] bool SetTerrainRenderer(std::span<const GroundTile> real,
                                        std::span<const GroundTile> virtual_,
                                        std::string &error) {
    return Accepted(ActiveContent().Geometry().Ground().SetInstances(real, virtual_), error);
  }

  [[nodiscard]] uint32_t TerrainRendererTriangles() const {
    return ActiveContent().Geometry().Ground().Triangles();
  }

  [[nodiscard]] bool
  SetPieceInstances(PieceHandle which, std::span<const Mat4> rows, std::string &error) {
    return ActiveContent().Scene().Resources().SetPieceInstances(
        ActiveContent().Geometry(), which, rows, error);
  }

  [[nodiscard]] bool
  SetPieceInstances(PieceId which, std::span<const Mat4> rows, std::string &error) {
    return ActiveContent().Geometry().SetPieceInstances(which, rows, error);
  }

  [[nodiscard]] bool SetPieceInstances(std::span<const PieceRows> pieces, std::string &error) {
    return ActiveContent().Scene().Resources().SetPieceInstances(
        ActiveContent().Geometry(), pieces, error);
  }

  [[nodiscard]] Native::Result<PrismGridHandle>
  PlacePrismGrid(std::shared_ptr<const PrismGrid> source,
                 std::span<const PrismInstance> instances,
                 uint32_t material) {
    auto &content = ActiveContent();
    return content.Scene().Resources().PlacePrismGrid(
        content.Geometry(), std::move(source), instances, material);
  }

  [[nodiscard]] Native::Result<void> ReplacePrismGrid(PrismGridHandle which,
                                                      std::shared_ptr<const PrismGrid> source,
                                                      std::span<const PrismInstance> instances,
                                                      uint32_t material) {
    auto &content = ActiveContent();
    return content.Scene().Resources().ReplacePrismGrid(
        content.Geometry(), which, std::move(source), instances, material);
  }

  [[nodiscard]] Native::Result<void>
  SetPrismGridInstances(PrismGridHandle which, std::span<const PrismInstance> instances) {
    auto &content = ActiveContent();
    return content.Scene().Resources().SetPrismGridInstances(content.Geometry(), which, instances);
  }

  void ReleasePrismGrid(PrismGridHandle which) {
    auto &content = ActiveContent();
    content.Scene().Resources().ReleasePrismGrid(content.Geometry(), which);
  }

  [[nodiscard]] bool HasPrismGrid(PrismGridHandle which) const noexcept {
    return ActiveContent().Scene().Resources().HasPrismGrid(which);
  }

  [[nodiscard]] bool RestorePieces(std::string &error) {
    return ActiveContent().Scene().Resources().RestorePieces(ActiveContent().Geometry(), error);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvancePieceRestore(size_t &nextPiece,
                                                                     size_t piecesMost) {
    auto &content = ActiveContent();
    return content.Scene().Resources().AdvancePieceRestore(
        content.Geometry(), nextPiece, piecesMost);
  }

  [[nodiscard]] std::expected<uint32_t, std::string> RegisterPieceMaterials(Geometry source) {
    auto &content = ActiveContent();
    return content.Scene().Resources().RegisterPieceMaterials(
        content.Geometry(),
        content.DrawsGlass ? &content.Transmission() : nullptr,
        std::move(source));
  }

  [[nodiscard]] bool RestorePieceMaterials(std::string &error) {
    auto &content = ActiveContent();
    return content.Scene().Resources().RestorePieceMaterials(
        content.Geometry(), content.DrawsGlass ? &content.Transmission() : nullptr, error);
  }

  [[nodiscard]] size_t PieceSourceBytes() const noexcept {
    return ActiveContent().Scene().Resources().PieceSourceBytes();
  }

  [[nodiscard]] size_t PieceSlots() const noexcept {
    return ActiveContent().Scene().Resources().PieceSlots();
  }

  [[nodiscard]] size_t PieceSlotBytes() const noexcept {
    return ActiveContent().Scene().Resources().PieceSlotBytes();
  }

  void SetNativePieceSurfaces(std::span<const uint32_t> slots) {
    ActiveContent().Geometry().SetNativePieceSurfaces(slots);
  }

  [[nodiscard]] uint32_t PiecesStanding() const {
    return ActiveContent().Geometry().PiecesStanding();
  }

  [[nodiscard]] size_t PieceSourceCount() const {
    return ActiveContent().Scene().Resources().PieceSourceCount();
  }

  [[nodiscard]] uint32_t PieceTriangles() const {
    return ActiveContent().Geometry().PieceTriangles();
  }

  [[nodiscard]] size_t TakeUploadAttempts() {
    return ActiveContent().Geometry().Residency().TakeUploadAttempts();
  }

  [[nodiscard]] size_t TotalUploadAttempts() const {
    return ActiveContent().Geometry().Resident().TotalUploadAttempts();
  }

  [[nodiscard]] size_t RecordedCrossings() const {
    return ActiveContent().Geometry().Resident().RecordedCrossings();
  }

  [[nodiscard]] size_t TakeUploadBytes() {
    return ActiveContent().Geometry().Residency().TakeUploadBytes();
  }

  [[nodiscard]] size_t TakeBufferAllocationAttempts() {
    return ActiveContent().Geometry().Residency().TakeBufferAllocationAttempts();
  }

  [[nodiscard]] size_t TakeStagingAllocationAttempts() {
    return ActiveContent().Geometry().Residency().TakeStagingAllocationAttempts();
  }

  [[nodiscard]] GeometryResidency::AllocationStats PieceAllocations() const {
    return ActiveContent().Geometry().Resident().Allocations();
  }

  [[nodiscard]] uint64_t PieceBytesHeld() const {
    return ActiveContent().Geometry().Resident().HeldBytes();
  }

  [[nodiscard]] ReadState ReadEnvironmentSpecular(std::vector<float> &rgba);

  [[nodiscard]] ReadState ReadSkyIrradiance(std::span<float, kIrradianceFloats> out);

  struct FrameGraphAllocations {
    size_t Textures = 0;
    size_t Buffers = 0;
    size_t Samplers = 0;
  };

  [[nodiscard]] FrameGraphAllocations FrameGraphAllocationCounts() const noexcept;

  [[nodiscard]] ReadState ReadPyramid(PyramidDepths &into);

  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);

  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);
  [[nodiscard]] ReadState ReadSurfaceBase(std::vector<float> &rgba);
  [[nodiscard]] ReadState ReadSurfaceMetalRough(std::vector<float> &rg);

  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  [[nodiscard]] bool ReplaceOverlay(std::span<const Ui::PaintQuad> quads,
                                    const Ui::PaintAtlasView *atlas,
                                    std::string &error) {
    return Accepted(
        ActiveState().Overlay.Replace(ActiveFrame().Handles, quads.data(), quads.size(), atlas),
        error);
  }

  [[nodiscard]] bool SetOverlay(const Ui::PaintQuad *quads, size_t count, std::string &error) {
    return Accepted(ActiveState().Overlay.SetQuads(ActiveFrame().Handles, quads, count), error);
  }

  [[nodiscard]] bool
  SetOverlayAtlas(const uint8_t *rgba, int width, int height, std::string &error) {
    return Accepted(ActiveState().Overlay.SetAtlas(ActiveFrame().Handles, rgba, width, height),
                    error);
  }

  [[nodiscard]] bool SetGeometryMesh(const MeshInput &mesh, std::string &error) {
    const Heap::Tagged relaying("mesh-relay");
    if (ActiveContent().DrawsGlass && !ActiveContent().Transmission().ValidateMesh(mesh, error)) {
      return false;
    }
    return ActiveContent().Geometry().SetMesh(mesh, error) &&
           (!ActiveContent().DrawsGlass || ActiveContent().Transmission().SetMesh(mesh, error));
  }

  [[nodiscard]] std::expected<GeometryRenderer::MeshTicket, std::string>
  BeginGeometryMesh(const MeshInput &mesh) {
    std::string error;
    if (ActiveContent().DrawsGlass && !ActiveContent().Transmission().ValidateMesh(mesh, error)) {
      return std::unexpected(std::move(error));
    }
    return ActiveContent().Geometry().BeginMesh(mesh);
  }

  [[nodiscard]] bool FinishGeometryMesh(GeometryRenderer::MeshTicket ticket,
                                        const MeshInput &mesh,
                                        std::string &error) {
    return ActiveContent().Geometry().FinishMesh(ticket, mesh, error) &&
           (!ActiveContent().DrawsGlass || ActiveContent().Transmission().SetMesh(mesh, error));
  }

  [[nodiscard]] bool GeometryMeshPending() const noexcept {
    return ActiveContent().Geometry().MeshPending();
  }

  [[nodiscard]] GeometryUploadMetrics LastGeometryMeshUpload() const noexcept {
    return ActiveContent().Geometry().LastMeshUpload();
  }

  [[nodiscard]] bool GeometryPlacementRows(size_t rows, std::string &error) {
    return ActiveContent().Geometry().PlacementRows(rows, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().PlacementRows(rows, error));
  }

  void MoveGeometryPlacement(size_t slot, const Mat4 &model) {
    ActiveContent().Geometry().MovePlacement(slot, model);
    if (ActiveContent().DrawsGlass) { ActiveContent().Transmission().MovePlacement(slot, model); }
  }

  [[nodiscard]] bool HandGeometryPlacements(std::string &error) {
    return ActiveContent().Geometry().HandPlacements(false, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().HandPlacements(false, error));
  }

  [[nodiscard]] size_t GeometryPlacementsMoved() const {
    return ActiveContent().Geometry().PlacementsMoved();
  }

  [[nodiscard]] uint32_t GeometryBytesStaged() const {
    return ActiveContent().Geometry().StagedBytes();
  }

  void ForgetGeometryStaging() { ActiveContent().Geometry().ForgetStagedCount(); }

  [[nodiscard]] const Vec3 &ShadowCentreM() const { return ActiveFrame().Shadow.ShadowCentreM(); }

  [[nodiscard]] bool SetGeometryPlacements(const double *models, size_t rows, std::string &error) {
    return ActiveContent().Geometry().SetPlacements(models, rows, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().SetPlacements(models, rows, error));
  }

  [[nodiscard]] bool SetGeometryVertexInput(const VertexInput &pose, std::string &error) {
    return ActiveContent().Geometry().SetVertexInput(pose, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().SetVertexInput(pose, error));
  }

  [[nodiscard]] bool SetGeometryMaterials(std::span<const MaterialBinding> materials,
                                          std::string &error) {
    return ActiveContent().Geometry().SetMaterials(materials, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().SetMaterials(materials, error));
  }

  [[nodiscard]] bool AppendGeometryMaterials(std::span<const MaterialBinding> materials,
                                             std::string &error) {
    if (!ActiveContent().Geometry().ValidateMaterials(materials, error) ||
        (ActiveContent().DrawsGlass &&
         !ActiveContent().Transmission().ValidateMaterials(materials, error))) {
      return false;
    }
    return ActiveContent().Geometry().AppendMaterials(materials, error) &&
           (!ActiveContent().DrawsGlass ||
            ActiveContent().Transmission().AppendMaterials(materials, error));
  }

  [[nodiscard]] bool SetGeometryLights(std::span<const SceneLight> lights, std::string &error) {
    return ActiveContent().Geometry().SetLights(lights, error) &&
           (!ActiveContent().DrawsGlass || ActiveContent().Transmission().SetLights(lights, error));
  }

  void SetMedium(const Medium &medium) {
    ActiveState().Medium = medium;
    ActiveState().MediumDeclared = true;
    if (!Candidate_) { ApplyWorldDeclarations(); }
  }

  void SetShadowFrame(
      const Vec3f &toSun, const Vec3f &up, double radiusM, bool cameraCentred, size_t lightIndex) {
    ActiveState().ShadowToSun = toSun;
    ActiveState().ShadowUp = up;
    ActiveState().ShadowRadiusM = radiusM;
    ActiveState().ShadowCameraCentred = cameraCentred;
    ActiveState().ShadowLightIndex = lightIndex;
    ActiveState().ShadowDeclared = true;
    if (!Candidate_) { ApplyWorldDeclarations(); }
  }

  void SetSky(const SkyDeclaration &sky) {
    ActiveState().CosSunZenith =
        sky.ToSun[0] * sky.Up[0] + sky.ToSun[1] * sky.Up[1] + sky.ToSun[2] * sky.Up[2];
    ActiveState().EyeHeightM = sky.EyeHeightM;
    ActiveState().SkyToSun = sky.ToSun;
    ActiveState().SkyUp = sky.Up;
    ActiveState().SkyIlluminanceLux = sky.IlluminanceLux;
    ActiveState().SkyDeclared = true;
    if (!Candidate_) { ApplyWorldDeclarations(); }
  }

  void SetSkyEye(float eyeHeightM) {
    if (!ActiveState().SkyDeclared) { return; }
    ActiveState().EyeHeightM = eyeHeightM;
    if (!Candidate_) { ApplyWorldDeclarations(); }
  }

  [[nodiscard]] Native::Texture *SkyViewTable() const {
    return ActiveFrame().Atmosphere.Texture(Resource::SkyViewLut);
  }

  [[nodiscard]] Native::Texture *MultiScatterTable() const {
    return ActiveFrame().Atmosphere.Texture(Resource::MultiScatterLut);
  }

  [[nodiscard]] Native::Texture *TransmittanceTable() const {
    return ActiveFrame().Atmosphere.Texture(Resource::TransmittanceLut);
  }

  void SetGeometryEnvironment(const EnvironmentLighting &environment) {
    ActiveContent().Geometry().SetEnvironment(environment);
    if (ActiveContent().DrawsGlass) { ActiveContent().Transmission().SetEnvironment(environment); }
    if (!Candidate_) { ApplyWorldDeclarations(); }
  }

  [[nodiscard]] uint32_t GeometryBatchCount() const {
    return ActiveContent().Geometry().BatchCount();
  }

  [[nodiscard]] uint32_t GeometryBatchesTaking(VertexLayout layout) const {
    uint32_t many = 0;
    for (const DrawBatch &batch : ActiveContent().Geometry().Drawn()) {
      many += batch.Layout == layout ? 1u : 0u;
    }
    return many;
  }

  [[nodiscard]] size_t ShadowCastCount() const { return ActiveFrame().Shadow.CastBatches(); }

  [[nodiscard]] const std::array<ShadowRegionUniform, kSunShadowRegions> &ShadowRegions() const {
    return ActiveFrame().Shadow.Regions();
  }

  [[nodiscard]] size_t ShadowedFrames() const {
    return ActiveContent().Geometry().ShadowedFrames();
  }

  struct Effort {
    double TookMs = 0.0;
    uint64_t DeviceBytes = 0;
    uint32_t Draws = 0;
    uint64_t DirectTriangles = 0;
    uint64_t IndirectInputTriangles = 0;
    uint32_t IndirectRecords = 0;
    uint32_t Surfaces = 0;
    uint32_t Placements = 0;
    uint32_t Textured = 0;
    uint32_t Palettes = 0;
    uint32_t Layouts = 0;
  };

  [[nodiscard]] const Effort &Spent(Stage stage) const {
    return Spent_[static_cast<size_t>(stage)];
  }

  struct CostProfile {
    struct StageCost {
      double TotalMs = 0.0, MostMs = 0.0;
      uint64_t Frames = 0;
    };

    std::array<StageCost, kStageCount> Stages{};
    RenderFrameTiming HostTotals;
    StageCost Gpu;
    uint64_t UnavailableGpuFrames = 0;
    uint64_t DroppedGpuSamples = 0;
    double GpuQueryMs = 0.0;
    uint64_t Frames = 0;
  };

  [[nodiscard]] const CostProfile &Costs() const noexcept { return Costs_; }

  [[nodiscard]] std::string_view GpuName() const noexcept;

  void ResetMeasurements() noexcept;
  void KeepGpuSamples(size_t maximum);

  [[nodiscard]] std::span<const GpuFrameSample> GpuSamples() const noexcept { return GpuSamples_; }

  struct SubmittedCameraFrame {
    uint64_t Serial = 0;
    CameraBasis Basis;
    Vec3 GeometryAnchorM;
  };

  [[nodiscard]] const SubmittedCameraFrame &LastSubmittedCamera() const noexcept {
    return LastSubmittedCamera_;
  }

  [[nodiscard]] const RenderFrameTiming &LastRenderFrameTiming() const noexcept {
    return LastRenderFrameTiming_;
  }

  [[nodiscard]] const RenderFrameTiming &SlowestRenderFrameTiming() const noexcept {
    return SlowestRenderFrameTiming_;
  }

  [[nodiscard]] size_t GeometryUniformPushes() const {
    return ActiveContent().Geometry().UniformPushes() +
           ActiveContent().Transmission().UniformPushes();
  }

  [[nodiscard]] float ExposureApplied() const {
    return ActiveState().Plan ? ActiveState().Plan->Exposure() : 0.0f;
  }

  [[nodiscard]] uint32_t GeometryDrawCount() const {
    return ActiveContent().Geometry().DrawCount();
  }

  [[nodiscard]] uint32_t GeometryDrawCallCount() const noexcept {
    const auto &content = ActiveContent();
    return content.Geometry().EncodedDrawCalls() +
           (content.DrawsGlass ? content.Transmission().EncodedDrawCalls() : 0u);
  }

  [[nodiscard]] uint32_t GeometryPipelineCount() const {
    return ActiveContent().Geometry().PipelineCount();
  }

  void SetCamera(const CameraBasis &basis, const Lens &lens) noexcept;

  [[nodiscard]] bool SetGroundClasses(std::span<const uint32_t> classes,
                                      std::span<const float> palette,
                                      std::string &error,
                                      GroundClassUploadMetrics *metrics = nullptr);
  [[nodiscard]] bool SetGroundClasses(GroundClassificationSource source,
                                      std::string &error,
                                      GroundClassUploadMetrics *metrics = nullptr);
  [[nodiscard]] bool BeginGroundClasses(GroundClassificationSource source,
                                        std::string &error,
                                        GroundClassUploadMetrics *metrics = nullptr);
  [[nodiscard]] std::expected<bool, std::string>
  AdvanceGroundClasses(size_t bytesMost, GroundClassUploadMetrics *metrics = nullptr);

  [[nodiscard]] bool RestoreGroundResources(std::string &error) {
    auto &content = ActiveContent();
    if (!UploadGroundClasses(content.Scene().Resources().GroundClasses(),
                             content.Scene().Resources().GroundPalette(),
                             error)) {
      return false;
    }
    return content.Scene().Resources().RestoreTerrain(content.Geometry(), error);
  }

  [[nodiscard]] std::expected<bool, std::string> AdvanceGroundResourceRestore(size_t &nextPage,
                                                                              size_t pagesMost) {
    auto &content = ActiveContent();
    return content.Scene().Resources().AdvanceTerrainRestore(
        content.Geometry(), nextPage, pagesMost);
  }

  [[nodiscard]] float NearMetres() const { return ActiveState().NearM; }

  void BeginTemporalRun();

  [[nodiscard]] int SceneW() const { return ActiveFrame().Width; }

  [[nodiscard]] int SceneH() const { return ActiveFrame().Height; }

  [[nodiscard]] double PictureW() const;
  [[nodiscard]] double PictureH() const;

  [[nodiscard]] double SceneAspect() const {
    return PictureH() > 0 ? PictureW() / PictureH() : 0.0;
  }

private:
  [[nodiscard]] static bool Accepted(Native::Result<void> result, std::string &error);

  [[nodiscard]] bool UploadGroundClasses(std::span<const uint32_t> classes,
                                         std::span<const float> palette,
                                         std::string &error,
                                         GroundClassUploadMetrics *metrics = nullptr);

  struct FrameResources;

  [[nodiscard]] Native::Result<void> FlushCrossings(Native::Commands &commands);
  [[nodiscard]] Native::Result<void> WaitForFrameSlot();
  [[nodiscard]] std::expected<void, std::string> PrepareFrame();
  [[nodiscard]] std::expected<void, std::string> RenderPublishedFrame();
  GpuSubmission Submission_;
  std::array<Effort, kStageCount> Spent_ = {{}};
  SubmittedCameraFrame LastSubmittedCamera_;
  void CommitCostProfile(const RenderFrameTiming &timing);
  void CollectGpuTime(Native::Completion completion);
  void CollectCompletedFrameTimes();
  CostProfile Costs_;
  Native::Completion MeasurementBegin_{};
  std::vector<GpuFrameSample> GpuSamples_;
  size_t GpuSampleLimit_ = 0;
  std::array<Effort, kStageCount> PendingSpent_{};
  RenderFrameTiming LastRenderFrameTiming_;
  RenderFrameTiming SlowestRenderFrameTiming_;

  [[nodiscard]] Native::Result<void> CreateSampler(FrameResources &frame);
  [[nodiscard]] bool PrepareGroundMaterialBuffers(FrameResources &frame, std::string &error);
  void CommitSubmittedFrame(StageSubmission &stageSubmission, const CameraBasis &camera);
  [[nodiscard]] Native::Result<void>
  Create(FrameResources &frame, const ExecutionPlan &plan, Resource resource);
  [[nodiscard]] static bool Created(const FrameResources &frame, Resource resource);
  [[nodiscard]] bool Configure(Stage stage,
                               FrameResources &frame,
                               const ExecutionPlan &plan,
                               bool drawsGlass,
                               std::string &error);
  [[nodiscard]] bool
  ConfigurePlanStages(FrameResources &frame, const ExecutionPlan &plan, bool drawsGlass);
  [[nodiscard]] static AttachmentSet
  ColoursForStage(const FrameResources &frame, const ExecutionPlan &plan, Stage wanted);
  [[nodiscard]] Native::Result<void>
  EncodeStage(Stage stage, const FrameContext &ctx, const PassRecording &into);

  struct Executor {
    Stage Named;
    bool (*Configure)(SceneRenderer &renderer,
                      FrameResources &frame,
                      const ExecutionPlan &plan,
                      bool drawsGlass,
                      std::string &error);
    Native::Result<void> (SceneRenderer::*Encode)(const FrameContext &ctx,
                                                  const PassRecording &into);
  };

  static constexpr size_t kExecutorCount = 23;
  static const std::array<Executor, kExecutorCount> kExecutors;
  [[nodiscard]] static const Executor *ExecutorOf(Stage stage);
  [[nodiscard]] PassRecording Picture(bool picture, const PassRecording &into) const;
  [[nodiscard]] static bool ConfigureGroundPyramid(SceneRenderer &renderer,
                                                   FrameResources &frame,
                                                   const ExecutionPlan &plan,
                                                   bool drawsGlass,
                                                   std::string &error);
  [[nodiscard]] static bool ConfigureGroundSelection(SceneRenderer &renderer,
                                                     FrameResources &frame,
                                                     const ExecutionPlan &plan,
                                                     bool drawsGlass,
                                                     std::string &error);
  [[nodiscard]] Native::Result<void> EncodeGroundPyramid(const FrameContext &ctx,
                                                         const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGroundSelection(const FrameContext &ctx,
                                                           const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGround(const FrameContext &ctx,
                                                  const PassRecording &into);
  [[nodiscard]] static bool ConfigureGeometry(SceneRenderer &renderer,
                                              FrameResources &frame,
                                              const ExecutionPlan &plan,
                                              bool drawsGlass,
                                              std::string &error);
  [[nodiscard]] static bool ConfigureGlass(SceneRenderer &renderer,
                                           FrameResources &frame,
                                           const ExecutionPlan &plan,
                                           bool drawsGlass,
                                           std::string &error);
  [[nodiscard]] static bool ConfigureCompositeTransmission(SceneRenderer &renderer,
                                                           FrameResources &frame,
                                                           const ExecutionPlan &plan,
                                                           bool drawsGlass,
                                                           std::string &error);
  [[nodiscard]] static bool ConfigureOverlay(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const ExecutionPlan &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigurePresent(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const ExecutionPlan &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureTonemap(SceneRenderer &renderer,
                                             FrameResources &frame,
                                             const ExecutionPlan &plan,
                                             bool drawsGlass,
                                             std::string &error);
  [[nodiscard]] static bool ConfigureMediumTransmittance(SceneRenderer &renderer,
                                                         FrameResources &frame,
                                                         const ExecutionPlan &plan,
                                                         bool drawsGlass,
                                                         std::string &error);
  [[nodiscard]] static bool ConfigureMediumMultiScatter(SceneRenderer &renderer,
                                                        FrameResources &frame,
                                                        const ExecutionPlan &plan,
                                                        bool drawsGlass,
                                                        std::string &error);
  [[nodiscard]] static bool ConfigureMediumRadiance(SceneRenderer &renderer,
                                                    FrameResources &frame,
                                                    const ExecutionPlan &plan,
                                                    bool drawsGlass,
                                                    std::string &error);
  [[nodiscard]] static bool ConfigureEnvironmentSpecular(SceneRenderer &renderer,
                                                         FrameResources &frame,
                                                         const ExecutionPlan &plan,
                                                         bool drawsGlass,
                                                         std::string &error);
  [[nodiscard]] static bool ConfigureIrradiance(SceneRenderer &renderer,
                                                FrameResources &frame,
                                                const ExecutionPlan &plan,
                                                bool drawsGlass,
                                                std::string &error);
  [[nodiscard]] static bool ConfigureDepthPyramid(SceneRenderer &renderer,
                                                  FrameResources &frame,
                                                  const ExecutionPlan &plan,
                                                  bool drawsGlass,
                                                  std::string &error);
  [[nodiscard]] static bool ConfigureSky(SceneRenderer &renderer,
                                         FrameResources &frame,
                                         const ExecutionPlan &plan,
                                         bool drawsGlass,
                                         std::string &error);
  [[nodiscard]] static bool ConfigureAerialPerspective(SceneRenderer &renderer,
                                                       FrameResources &frame,
                                                       const ExecutionPlan &plan,
                                                       bool drawsGlass,
                                                       std::string &error);
  [[nodiscard]] static bool ConfigureLightVisibility(SceneRenderer &renderer,
                                                     FrameResources &frame,
                                                     const ExecutionPlan &plan,
                                                     bool drawsGlass,
                                                     std::string &error);
  [[nodiscard]] static bool ConfigureGeometryCull(SceneRenderer &renderer,
                                                  FrameResources &frame,
                                                  const ExecutionPlan &plan,
                                                  bool drawsGlass,
                                                  std::string &error);
  [[nodiscard]] Native::Result<void> EncodeGeometry(const FrameContext &ctx,
                                                    const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGlass(const FrameContext &ctx,
                                                 const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeCompositeTransmission(const FrameContext &ctx,
                                                                 const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeOverlay(const FrameContext &ctx,
                                                   const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodePresent(const FrameContext &ctx,
                                                   const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeTonemap(const FrameContext &ctx,
                                                   const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeMediumTransmittance(const FrameContext &ctx,
                                                               const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeMediumMultiScatter(const FrameContext &ctx,
                                                              const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeMediumRadiance(const FrameContext &ctx,
                                                          const PassRecording &into);

  [[nodiscard]] Native::Result<void> EncodeEnvironmentSpecular(const FrameContext &ctx,
                                                               const PassRecording &into);

  [[nodiscard]] Native::Result<void> EncodeIrradiance(const FrameContext &ctx,
                                                      const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeDepthPyramid(const FrameContext &ctx,
                                                        const PassRecording &into);
  [[nodiscard]] EyeBasis Eye() const;
  [[nodiscard]] Native::Result<void> EncodeSky(const FrameContext &ctx, const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeAerialPerspective(const FrameContext &ctx,
                                                             const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeLightVisibility(const FrameContext &ctx,
                                                           const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGeometryRoots(const FrameContext &ctx,
                                                         const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGeometryCull(const FrameContext &ctx,
                                                        const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGeometryScan(const FrameContext &ctx,
                                                        const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodeGeometryCompact(const FrameContext &ctx,
                                                           const PassRecording &into);
  [[nodiscard]] Native::Result<void> EncodePass(Native::Commands *commands,
                                                size_t pass,
                                                const FrameContext &ctx,
                                                StageSubmission &submission);
  [[nodiscard]] Native::Result<void> EncodeComputePass(Native::Commands *commands,
                                                       const ExecutionPlan::Pass &declared,
                                                       const FrameContext &ctx,
                                                       StageSubmission &submission);
  [[nodiscard]] Native::Result<void> EncodeGraphicsPass(Native::Commands *commands,
                                                        const ExecutionPlan::Pass &declared,
                                                        const FrameContext &ctx,
                                                        StageSubmission &submission);
  [[nodiscard]] FrameContext Framing() const;
  void SettleShadow();
  std::array<bool, kResourceCount> Touched_ = {{}};
  [[nodiscard]] Native::Texture *Target(Resource resource) const;
  [[nodiscard]] static Native::Texture *
  Target(const FrameResources &frame, Resource resource, size_t linearIndex);
  void BindFrameResources();
  void ApplyWorldDeclarations();

  [[nodiscard]] DisplayOptions Display() const;

  [[nodiscard]] Native::Texture *LinearSource() const;
  [[nodiscard]] Native::Texture *DisplaySource() const;

  std::unique_ptr<Native::Device> Device_;
  std::chrono::nanoseconds UploadBudget_{};
  std::unique_ptr<UploadQueue> Uploads_;

  struct FrameResources {
    const Native::Texture *HostSurface = nullptr;
    Shown Shown;
    std::unique_ptr<Native::Texture> Offscreen;
    Gpu Handles;
    FrameTargets Targets;
    AtmospherePasses Atmosphere;
    std::unique_ptr<Native::Sampler> Samp;
    Readback PyramidRead;
    int Width = 0, Height = 0;
    CompositeTransmissionStage CompositeTransmission;
    AerialPerspectiveStage Aerial;
    TonemapStage Tonemap;
    DepthPyramidPass DepthPyramid;
    DepthPyramidPass GroundPyramid;
    SkyStage Sky;
    SunShadowPass Shadow;
    GeometryCullStage Cull;
    OverlayPipeline OverlayPipe;
    PresentStage Present;
    GeometryPipelineBinding GeometryPipelines;
    GeometryPipelineBinding GlassPipelines;
  };

  struct SceneStateCore {
    std::shared_ptr<const ExecutionPlan> Plan;
    std::unique_ptr<SceneRenderState> Content = std::make_unique<SceneRenderState>();
    OverlayDraw Overlay;
    Medium Medium = kEarthAir;
    float CosSunZenith = 1.0f;
    float EyeHeightM = 0.0f;
    ViewHistory History;
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
    bool Ready = false;
    std::string WhyNot;

    bool MediumDeclared = false;
    bool SkyDeclared = false;
    bool ShadowDeclared = false;
    Vec3f SkyToSun = {{0.0f, 1.0f, 0.0f}};
    Vec3f SkyUp = {{0.0f, 1.0f, 0.0f}};
    float SkyIlluminanceLux = 0.0f;
    Vec3f ShadowToSun = {{0.0f, 1.0f, 0.0f}};
    Vec3f ShadowUp = {{0.0f, 1.0f, 0.0f}};
    double ShadowRadiusM = 0.0;
    bool ShadowCameraCentred = false;
    size_t ShadowLightIndex = 0;

    SceneStateCore() = default;
    SceneStateCore(const SceneStateCore &) = delete;
    SceneStateCore &operator=(const SceneStateCore &) = delete;
    SceneStateCore(SceneStateCore &&) noexcept = default;
    SceneStateCore &operator=(SceneStateCore &&) noexcept = default;
  };

  struct SceneState : SceneStateCore {
    FrameResources Frame;
  };

  struct WorldCandidate : SceneStateCore {
    std::optional<FrameResources> Frame;
    GroundSources GroundInputs = GroundSources::Default;
  };

  static_assert(std::is_nothrow_move_constructible_v<FrameResources>);
  static_assert(std::is_nothrow_move_assignable_v<FrameResources>);
  static_assert(std::is_nothrow_move_constructible_v<SceneStateCore>);
  static_assert(std::is_nothrow_move_assignable_v<SceneStateCore>);
  static_assert(std::is_nothrow_move_constructible_v<SceneState>);
  static_assert(std::is_nothrow_move_assignable_v<SceneState>);
  static_assert(std::is_nothrow_move_constructible_v<WorldCandidate>);
  static_assert(std::is_nothrow_move_assignable_v<WorldCandidate>);

  bool Stands();
  [[nodiscard]] std::expected<void, std::string>
  InitForTarget(Extent frame, std::shared_ptr<const ExecutionPlan> plan, bool presents);
  [[nodiscard]] std::expected<void, std::string> ValidateExecutablePlan(const ExecutionPlan &plan);
  [[nodiscard]] std::expected<bool, std::string> ReuseFrameResources(Extent frame,
                                                                     const ExecutionPlan &plan);
  [[nodiscard]] std::expected<FrameResources, std::string>
  BuildFrameResources(Extent frame, const ExecutionPlan &plan, bool presents);
  void PublishFrameResources(FrameResources frame, std::shared_ptr<const ExecutionPlan> plan);
  void LogReadyPlan(Native::Device *device) const;
  [[nodiscard]] std::expected<void, std::string>
  StandsOffscreen(FrameResources &frame, const ExecutionPlan *plan, bool presents);
  [[nodiscard]] std::expected<std::unique_ptr<Native::Texture>, std::string>
  MakeOffscreen(const ExecutionPlan *plan, Extent frame);
  [[nodiscard]] std::expected<bool, std::string> ClaimWindow(SDL_Window *window);

  bool Presenting_ = true;
  SDL_Window *Showing_ = nullptr;

  struct Placed {
    double LeftPx = 0, TopPx = 0, WidthPx = 0, HeightPx = 0;
  };

  [[nodiscard]] Placed PictureRect() const;
  [[nodiscard]] Lens Through() const;

public:
  class PublishedWorldScope {
  public:
    explicit PublishedWorldScope(SceneRenderer &renderer) noexcept : Renderer_(renderer) {
      ++Renderer_.PublishedScopeDepth_;
    }

    PublishedWorldScope(const PublishedWorldScope &) = delete;
    PublishedWorldScope &operator=(const PublishedWorldScope &) = delete;
    PublishedWorldScope(PublishedWorldScope &&) = delete;
    PublishedWorldScope &operator=(PublishedWorldScope &&) = delete;

    ~PublishedWorldScope() {
      assert(Renderer_.PublishedScopeDepth_ > 0);
      --Renderer_.PublishedScopeDepth_;
    }

  private:
    SceneRenderer &Renderer_;
  };

  [[nodiscard]] PublishedWorldScope PublishedWorld() noexcept { return PublishedWorldScope(*this); }

  [[nodiscard]] SceneCandidateEdit EditWorldCandidate() noexcept {
    assert(Candidate_);
    return SceneCandidateEdit(CandidateEditorDepth_);
  }

private:
  [[nodiscard]] SceneStateCore &ActiveState() noexcept {
    return Candidate_ && CandidateEditorDepth_ > 0 && PublishedScopeDepth_ == 0
               ? static_cast<SceneStateCore &>(*Candidate_)
               : static_cast<SceneStateCore &>(State_);
  }

  [[nodiscard]] const SceneStateCore &ActiveState() const noexcept {
    return Candidate_ && CandidateEditorDepth_ > 0 && PublishedScopeDepth_ == 0
               ? static_cast<const SceneStateCore &>(*Candidate_)
               : static_cast<const SceneStateCore &>(State_);
  }

  [[nodiscard]] SceneRenderState &ActiveContent() noexcept { return *ActiveState().Content; }

  [[nodiscard]] const SceneRenderState &ActiveContent() const noexcept {
    return *ActiveState().Content;
  }

  [[nodiscard]] FrameResources &ActiveFrame() noexcept {
    return Candidate_ && CandidateEditorDepth_ > 0 && PublishedScopeDepth_ == 0 && Candidate_->Frame
               ? *Candidate_->Frame
               : State_.Frame;
  }

  [[nodiscard]] const FrameResources &ActiveFrame() const noexcept {
    return Candidate_ && CandidateEditorDepth_ > 0 && PublishedScopeDepth_ == 0 && Candidate_->Frame
               ? *Candidate_->Frame
               : State_.Frame;
  }

  FrameTimeline Frames_;
  SceneState State_;
  std::optional<WorldCandidate> Candidate_;
  size_t PublishedScopeDepth_ = 0;
  size_t CandidateEditorDepth_ = 0;
};

}
#endif
