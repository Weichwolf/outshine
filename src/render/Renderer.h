/* THE DEVICE HALF OF A COMPILED PLAN. Renderer owns the device, the resources the plan holds and
 * every render-pass boundary; drawing lives in the stages that record into the pass it opened. A
 * stage never begins or ends a pass.
 *
 * NOTHING HERE DECIDES WHAT IS RENDERED. The plan says which resources exist, which stages are
 * configured, in what order and in how many passes; this file executes that and holds no composition
 * of its own. There is no pass tally to keep against a fixed enumeration, because the pass count is
 * an output of the compiler.
 *
 * THE CATALOGUE IS WHAT A PLAN CAN SAY AND THIS IS WHAT THIS DEVICE LAYER CAN DO, and the two are
 * not the same set. `Init` refuses a plan holding a stage this layer does not execute, naming it: a
 * stage silently encoding nothing is a black picture with nothing to attribute it to, which is the
 * one failure a renderer must never be able to report as success. */
#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "FrameContext.h"
#include "Gpu.h"
#include "GpuOwned.h"
#include "Readback.h"
#include "RenderPlan.h"
#include "stages/Resolve.h"
#include "stages/SubjectDraw.h"
#include "stages/CompositeTransmissionStage.h"
#include "stages/TonemapStage.h"

namespace outshine::Render {

class Renderer {
public:
  /* Bring-up into the render target. The plan is the whole of what will be created and encoded, and
   * it cannot be unvalidated -- it has no public constructor and only RenderPlan::Compile makes one.
   * A refusal leaves `DeviceUsable()` false and says why in the log. */
  void Init(int width, int height, std::shared_ptr<const RenderPlan> plan);
  [[nodiscard]] const RenderPlan &Plan(void) const { return *Plan_; }
  [[nodiscard]] bool DeviceUsable(void) const { return Ready; }

  /* Run the passes and submit. It does NOT wait: the device runs behind the caller, which is what a
   * host that wants to prepare the next frame while this one draws depends on. */
  void RenderFrame(void);

  /* Blocks until every frame submitted so far has finished on the device. It is what makes a frame
   * a MEASURABLE quantity -- a submit returns before any of the work happens, so a clock around
   * `RenderFrame` alone times the encoder and not the frame. Nothing on a shipping frame path calls
   * it; the reads already fence for themselves. */
  void WaitForGpu(void);

  /* HOW MANY FRAMES BEFORE THE PICTURE IS THE PICTURE -- a property the compiled plan states, so no
   * caller carries a settle constant of its own. */
  [[nodiscard]] int SettleFrames(void) const { return Plan_ ? Plan_->SettleFrames() : 1; }

  /* Tightly packed W*H*4 RGBA8, already sRGB-encoded -- ready for a PNG writer. */
  [[nodiscard]] ReadState ReadPixels(std::vector<uint8_t> &rgba);

  /* Reversed-Z scene depth, W*H floats, row-major. Range along the view ray follows as
   * kNearM / depth / cos(angle off boresight). */
  [[nodiscard]] ReadState ReadDepth(std::vector<float> &depth);
  static constexpr float kNearM = 0.05f;   /* MvpCamRel's zn -- the numerator of that division */

  /* THE SCENE-REFERRED LINEAR TAP: W*H RGBA floats, row-major -- resolved linear radiance BEFORE the
   * display transfer. It is the zero point a radiance comparison needs, and a plan that holds no
   * such resource refuses.
   *
   * ONE CURRENCY OUT, AND THE PLAN SAYS WHICH STORAGE PRODUCED IT. Under `ScenePrecision::Half` the
   * values are widened from binary16, which is exact; under `Float` they are the target's own bits.
   * A caller whose verdict is the value reads `Plan::Format(Resource::SceneLinear)` to know which
   * floor it is measuring against. */
  [[nodiscard]] ReadState ReadSceneLinear(std::vector<float> &rgba);
  /* THE NORMAL THE BRDF RECEIVED, one xyz per pixel, world-space in the subject's own frame
   * (board:1122). Empty unless the plan holds the target, which it does only where something asked
   * for it -- a readback of an attachment nobody requested would be reading a texture that was
   * never allocated. */
  [[nodiscard]] ReadState ReadShadingNormal(std::vector<float> &xyz);
  /* WHICH SURFACE SLOT THE FRAGMENT WORE, one value per pixel in `x`, ONE HIGHER THAN THE SLOT so
   * that 0 is "no subject fragment here" (board:1138). Empty unless the plan holds the target. A
   * slot is not a material: which material a slot carries is the consumer's own table, and this
   * layer has no spelling for one. */
  [[nodiscard]] ReadState ReadSurfaceIdentity(std::vector<float> &slot);
  /* THE SCREEN-SPACE MOTION OF WHAT WROTE THE DEPTH, two floats per pixel in NDC units per frame
   * (board:1169). Empty unless the plan holds the target. A pixel no subject fragment reached
   * carries the pass's clear value, `kVelocityStatic` in both channels, which is unreachable for a
   * real motion -- NDC displacement is bounded by 2 per axis -- so "static" and "did not move" are
   * two answers here and not one. */
  [[nodiscard]] ReadState ReadSceneVelocity(std::vector<float> &xy);

  /* THE DECLARED SUBJECT OF A STUDIO (stages/SubjectDraw.h): one indexed mesh and the DRAW LIST over
   * it -- many primitives, each with its own surface slot and vertex layout. */
  [[nodiscard]] bool SetSubjectMesh(const SubjectMesh &mesh, std::string &error) {
    return Subjects_.SetMesh(mesh, error);
  }
  /* THE SUBJECT'S SURFACES, one per slot a draw key can name: the file says which image and how it
   * is addressed, the consumer decodes it, and this holds it. */
  [[nodiscard]] bool SetSubjectMaterials(const std::vector<SubjectMaterial> &materials,
                                         std::string &error) {
    return Subjects_.SetMaterials(materials, error);
  }
  /* THE LIGHTS THE SUBJECT IS LIT BY, as a list. */
  [[nodiscard]] bool SetSubjectLights(const std::vector<SubjectLight> &lights, std::string &error) {
    return Subjects_.SetLights(lights, error);
  }
  /* THE ENVIRONMENT THE SUBJECT SITS IN (board:1206), declared and zero where none was declared. */
  void SetSubjectEnvironment(const SubjectEnvironment &environment) {
    Subjects_.SetEnvironment(environment);
  }
  [[nodiscard]] uint32_t SubjectBatchCount(void) const { return Subjects_.BatchCount(); }
  [[nodiscard]] uint32_t SubjectDrawCount(void) const { return Subjects_.DrawCount(); }
  /* HOW MANY SUBJECT PIPELINES `Init` BUILT (board:1187). It is readable because a frame instrument
   * asked whether a pipeline count cost anything must read the count in the same run as the
   * duration; a number quoted from a source diff beside a measurement is the pairing this tree has
   * already been wrong about once. */
  [[nodiscard]] uint32_t SubjectPipelineCount(void) const { return Subjects_.PipelineCount(); }
  /* WHERE A SHADOW RAY STARTS, in the subject's own metres (`stages/ShadowRay.h`). It is readable
   * because it is the ONLY quantity in the visibility estimator a comparison can be displaced by:
   * I.26.15 bounds a shadow-edge disagreement in screen pixels against this bias projected, and a
   * bound over a number nobody can read would be a bound nobody can recompute. */
  [[nodiscard]] float ShadowRayNearM(void) const { return Subjects_.ShadowNearM(); }

  /* Carries ROLL, so the horizon tilts at bank. */
  void SetCameraBasis(const double eye[3], const double fwd[3], const double right[3],
                      const double up[3]);

  /* THE SCENE'S vertical field of view over the full frame height, as the scene file declares it.
   * It enters the projection and nothing else, so there is never a second copy to drift from. */
  void SetFovDeg(double deg) { FovDeg = deg > 0.0 ? (float)deg : FovDeg; }
  void SetOrthoM(double m) { OrthoM = (float)m; }

  [[nodiscard]] int SceneW(void) const { return Width; }
  [[nodiscard]] int SceneH(void) const { return Height; }
  /* The frame's shape, which a parallel projection needs and a perspective one does not: the
   * vertical field of view fixes the horizontal only once the aspect is known. */
  [[nodiscard]] double SceneAspect(void) const {
    return Height > 0 ? (double)Width / (double)Height : 0.0;
  }

private:
  /* THE THREE EXHAUSTIVE SWITCHES OVER THE CATALOGUE, and there are no others: whether this layer
   * can execute a stage at all, what a resource IS, and what configures a stage. Each has no
   * `default:`, so a new catalogue row is a compile error until all three answer for it. */
  [[nodiscard]] static bool Executable(Stage stage);
  void Create(Resource resource);
  [[nodiscard]] bool Configure(Stage stage, std::string &error);
  void EncodeStage(Stage stage, const PassRecording &into);
  void EncodePass(SDL_GPUCommandBuffer *commands, size_t pass);
  [[nodiscard]] SDL_GPUTexture *Target(Resource resource) const;
  [[nodiscard]] DisplayOptions Display(void) const;
  /* The texture the linear tap copies, which is whatever the plan bound in `sceneLinear`'s place. */
  [[nodiscard]] SDL_GPUTexture *LinearSource(void) const;

  /* FIRST, SO IT IS DESTROYED LAST (`C.13`): every handle below is taken from it. */
  OwnedDevice Device_;
  std::shared_ptr<const RenderPlan> Plan_;
  Gpu Handles;
  OwnedTexture HdrTex, VelTex, DepthTex, FrameTex, OffscreenTex;
  /* `KHR_materials_transmission` (board:1386): where a transmissive draw puts its radiance, and the
   * two put together. Kept apart from `HdrTex` because the pass that reads what stands behind it
   * must not also be writing that -- the plan's own topological invariant refuses the shape where it
   * would be, and that refusal is what chose this layout. Both stay unbound on a plan that declares
   * no glass, where `SceneComposited` aliases straight to `SceneHdr`. */
  OwnedTexture TransmissiveTex, CompositedTex;
  /* The normal the BRDF received, allocated only where a plan reads it (board:1122). */
  OwnedTexture ShadingNormalTex;
  /* Which surface slot the fragment wore, allocated only where a plan reads it (board:1138). */
  OwnedTexture SurfaceIdentityTex;
  OwnedSampler Samp;
  SubjectDraw Subjects_;
  CompositeTransmissionStage CompositeTransmission_;
  TonemapStage Tonemap_;

  bool Ready = false;
  int Width = 0, Height = 0;
  bool CameraFull = false;                /* SetCameraBasis used */
  double Eye[3] = {0, 0, 0};
  double Fwd[3] = {0, 0, 0}, Right[3] = {0, 0, 0}, Up[3] = {0, 0, 0};
  float FovDeg = 60.0f;                   /* [SET] until a scene declares one */
  float OrthoM = 0.0f;                    /* > 0: parallel projection covering this many metres */
  /* WHERE THE EYE WAS WHEN THE LAST FRAME WAS SUBMITTED, which is the other half of a screen-space
   * motion vector (board:1169). It is written at the END of RenderFrame, so every stage of one
   * frame sees the same previous camera whatever order the passes were compiled into. Before the
   * first submit it is this frame's, which makes the first frame's motion zero rather than the
   * displacement from an undefined pose. */
  bool Submitted = false;
  double PrevEye[3] = {0, 0, 0};
  float PrevMvp16[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

} // namespace outshine::Render
#endif
