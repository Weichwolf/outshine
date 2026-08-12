#include "GeometryStage.h"

namespace outshine::Render {

void GeometryStage::SetSun(const double sunEcef[3], const double up[3], float nightAmbient) {
  Buildings_->SetSun(sunEcef, up, nightAmbient);
  Water_->SetSun(sunEcef, up, nightAmbient);
  Models_->SetSun(sunEcef, nightAmbient);
}

void GeometryStage::Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) {
  Cut_.Open(ctx);
  Terrain_->Encode(ctx, Cut_, pass);
  Buildings_->Encode(ctx, Cut_, pass);
  /* After the ground and the prisms: a water surface is opaque here and depth sorts it. */
  Water_->Encode(ctx, Cut_, pass);
  Models_->Encode(ctx, Cut_, pass);
  /* Last, and it belongs to no ladder: a studio subject is the only thing in the scene. */
  Subjects_->Encode(ctx, Cut_, pass);
}

long GeometryStage::TriangleCount() const {
  return Terrain_->TriangleCount() + (long)Buildings_->VertexCount() / 3 +
         Models_->TriangleCount() + Subjects_->TriangleCount();
}

} // namespace outshine::Render
