/* EVERY SURFACE THE WORLD IS MADE OF, in one stage over one cut. Terrain, prisms, water and
 * instanced models were four stages with four LOD cuts, and the ladder is one thing
 * (`doc/architecture.md`, "LOD: one ladder, never one per kind of content") — so the cut is made
 * here, once a frame, and each unit reads it.
 *
 * IT ADDS NO PASS AND IT REMOVES NONE. The renderer opens the scene pass and hands it over; the
 * order the units draw in is this stage's, and it is depth order: ground, then what stands on it,
 * then what floats on it, then what grows out of it. */
#ifndef GEOMETRYSTAGE_H
#define GEOMETRYSTAGE_H

#include <memory>

#include "BuildingDraw.h"
#include "ClusterCut.h"
#include "DrawStage.h"
#include "ModelDraw.h"
#include "SubjectDraw.h"
#include "TerrainDraw.h"
#include "WaterDraw.h"

namespace outshine::Render {

class GeometryStage : public DrawStage {
public:
  TerrainDraw &Terrain() { return *Terrain_; }
  const TerrainDraw &Terrain() const { return *Terrain_; }
  BuildingDraw &Buildings() { return *Buildings_; }
  const BuildingDraw &Buildings() const { return *Buildings_; }
  WaterDraw &Water() { return *Water_; }
  const WaterDraw &Water() const { return *Water_; }
  ModelDraw &Models() { return *Models_; }
  const ModelDraw &Models() const { return *Models_; }
  SubjectDraw &Subjects() { return *Subjects_; }
  const SubjectDraw &Subjects() const { return *Subjects_; }

  /* The light every unit binds and the sun every unit shades by — handed to all of them at once,
   * because a second lighting model inside one stage is exactly what this stage exists to prevent. */
  void SetSun(const double sunEcef[3], const double up[3], float nightAmbient);

  void Encode(const FrameContext &ctx, wgpu::RenderPassEncoder &pass) override;

  /* Triangles the LAST Encode submitted, over every unit: the budget instrument. */
  long TriangleCount() const;

private:
  ClusterCut Cut_;
  std::unique_ptr<TerrainDraw> Terrain_ = std::make_unique<TerrainDraw>();
  std::unique_ptr<BuildingDraw> Buildings_ = std::make_unique<BuildingDraw>();
  std::unique_ptr<WaterDraw> Water_ = std::make_unique<WaterDraw>();
  std::unique_ptr<ModelDraw> Models_ = std::make_unique<ModelDraw>();
  std::unique_ptr<SubjectDraw> Subjects_ = std::make_unique<SubjectDraw>();
};

} // namespace outshine::Render
#endif
