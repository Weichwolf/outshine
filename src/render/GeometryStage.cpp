#include "GeometryStage.h"

namespace outshine::Render {

void GeometryStage::SetSun(const double sunEcef[3], const double up[3], float nightAmbient) {
  Buildings_->SetSun(sunEcef, up, nightAmbient);
  Water_->SetSun(sunEcef, up, nightAmbient);
  Models_->SetSun(sunEcef, nightAmbient);
}

void GeometryStage::EncodeUnit(Stage unit, const FrameContext &ctx,
                               wgpu::RenderPassEncoder &pass) {
  /* The draw ORDER is the plan's, and the plan's is the catalogue's: ground, then what stands on it,
   * then what floats on it, then what grows out of it, then a studio subject that belongs to no
   * ladder. This answers for one unit and never decides which of them run. */
  if (unit == Stage::Terrain) Terrain_->Encode(ctx, Cut_, pass);
  if (unit == Stage::Buildings) Buildings_->Encode(ctx, Cut_, pass);
  if (unit == Stage::Water) Water_->Encode(ctx, Cut_, pass);
  if (unit == Stage::Models) Models_->Encode(ctx, Cut_, pass);
  if (unit == Stage::Subjects) Subjects_->Encode(ctx, Cut_, pass);
}

long GeometryStage::TriangleCount() const {
  return Terrain_->TriangleCount() + (long)Buildings_->VertexCount() / 3 +
         Models_->TriangleCount() + Subjects_->TriangleCount();
}

} // namespace outshine::Render
