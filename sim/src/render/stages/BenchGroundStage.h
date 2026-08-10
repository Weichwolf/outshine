/* THE FLOOR AND THE REFERENCE the subject bench stands a plant on. Two surfaces, and the split is
 * the whole point of the stage: the PLANE wears the
 * subject's own declared substrate, because the contrast question the bench exists for is soil against
 * blade and a neutral floor answers it with a photometer; the CARD is 18 % neutral and stands upright
 * at the frame's edge, because a reference that lies BEHIND the subject is a background and not a
 * reference. Neither is the scene's classified ground — both are declared constants, so the ground
 * shader's own defects stay off the plant's account.
 *
 * What it is NOT allowed to be is a second lighting model: it splices the same SurfaceLight.h,
 * every lit surface in the frame splices, and binds the same
 * SceneLight bundle. Self-gates on "no plane declared", so the scene pass is untouched. */
#ifndef BENCHGROUNDSTAGE_H
#define BENCHGROUNDSTAGE_H

#include "DrawStage.h"

namespace outshine::Render {

/* Where the neutral card stands, in ground metres east/north of the CAMERA — the same frame the
 * plane's own texture coordinate uses. `LatE/LatN` is the card's width axis (the camera's right in
 * the ground plane), so the card faces the camera without the stage knowing the azimuth.
 * `HalfWidthM` <= 0 retires it. */
struct BenchCard {
  double EastM = 0.0, NorthM = 0.0;
  double LatE = 1.0, LatN = 0.0;
  double HalfWidthM = 0.0, HeightM = 0.0;
};

class BenchGroundStage : public DrawStage {
public:
  void Configure(const Gpu &gpu, const SceneLight &light);
  bool Ready(void) const { return Pipe != nullptr; }

  /* `eyeAglM` is how far the camera stands above the plane, `radiusM` how far the plane reaches, and
   * `gridM` the spacing of the ruled lines on it. Passing radiusM <= 0 retires the whole stage. */
  void SetPlane(double eyeAglM, double radiusM, double gridM);
  void SetSubstrate(const float linearRgb[3]);
  void SetCard(const BenchCard &card) { Card = card; }
  void SetSun(const double sunEcef[3], float nightAmbient);

  static constexpr float kCardAlbedo = 0.18f;   /* the photographic neutral, and the ONLY constant here */

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

private:
  static constexpr int kUniFloats = 48;   /* mat4 + seven vec4f = 44, rounded up — the WGSL `B` verbatim */

  wgpu::Device Device;
  wgpu::Queue Queue;
  wgpu::RenderPipeline Pipe;
  wgpu::BindGroup Bind;
  wgpu::Buffer Uni;
  SceneLight Light;

  double SunDir[3] = {0, 0, 1};
  double EyeAglM = 0.0, RadiusM = 0.0, GridM = 0.0;
  float Substrate[3] = {kCardAlbedo, kCardAlbedo, kCardAlbedo};
  BenchCard Card;
  float NightAmbient = 0.0f;
};

} // namespace outshine::Render
#endif
