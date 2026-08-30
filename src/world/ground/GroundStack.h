#ifndef OUTSHINE_WORLD_GROUND_GROUNDSTACK_H
#define OUTSHINE_WORLD_GROUND_GROUNDSTACK_H

#include <memory>
#include <span>
#include <string_view>

#include "ContentStore.h"
#include "DeclaredSources.h"
#include "TerrainLoader.h"
#include "SourceDecl.h"
#include "BuildingField.h"
#include "ClassField.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "StreetField.h"
#include "TilePool.h"
#include "VegetationTemplates.h"
#include "WaterField.h"

namespace outshine {
class Sink;
}

namespace outshine::Ground {

// WHAT THE FRAME MAY SPEND ON THE STREAM, and it is a DURATION rather than a count of tiles. A
// count is bound to how often somebody calls, which is not a quantity a frame budget is made of: at
// 0.3 ms a tile it leaves the frame idle and at 40 ms it has already overrun. [SET] at 2.0 ms --
// 12 per cent of the 16.7 ms a 60 Hz frame owns -- pending a measurement of the decode's own cost
// on this target. `preload` passes 0.0, which means unbounded, because it IS the wait.
constexpr double kStreamBudgetMs = 2.0;

class GroundStack {
public:
  GroundStack() = default;

  ~GroundStack() { Close(); }

  GroundStack(const GroundStack &) = delete;
  GroundStack &operator=(const GroundStack &) = delete;

  [[nodiscard]] bool Open(std::string_view cacheDir,
                          std::string_view assetsDir,
                          std::span<const Provider> providers,
                          double focusLat,
                          double focusLon,
                          Data::Transport &wire,
                          Sink &say,
                          double patienceS = 0.0);
  void Close();

  [[nodiscard]] bool Opened() const { return Opened_; }

  [[nodiscard]] TilePool &Pool() const { return *Pool_; }

  [[nodiscard]] GroundStream &Ground() const { return *Ground_; }

  [[nodiscard]] const ClassField &Classes() const { return Cls_; }

  // THE ALBEDO EACH LAND CLASS DECLARES. Twenty of them are loaded from
  // `world/ground-materials.json` at every start and nothing outside this tier could read them, so
  // a desert and a meadow came out the same green.
  [[nodiscard]] const GroundMaterials &Materials() const { return Materials_; }

  void SetVegetation(const VegetationTemplates *veg) { Cls_.SetVegetation(veg); }

  [[nodiscard]] const OsmField *Vectors() const { return Vectors_.get(); }

  [[nodiscard]] const BuildingField &Footprints() const { return Footprints_; }

  // THE MESHER COMES FROM ABOVE. `StructureMesher` is declared in this tier and implemented in
  // `src/generators/`, which this tier may not see, so whoever sits above both installs it. Nothing
  // did: `BuildingField::Shapes` had no caller and `BuildingField::Verts` had no reader, so every
  // footprint the world read was meshed by nobody.
  void ShapesFootprintsWith(const StructureMesher *mesher) { Footprints_.Shapes(mesher); }

  // THE FOCAL LENGTH THE FOOTPRINTS ARE MESHED FOR. A level of detail is a number of PIXELS, so the
  // generator cannot choose one without knowing how many pixels a metre is worth. It is declared
  // once with the frame and the lens rather than sampled per frame, because a level that changed
  // with the camera would remesh the world every time the view turned.
  void SeeFootprintsWith(double focalPx) { Footprints_.SeenWith(focalPx); }

  [[nodiscard]] const WaterField &WaterBodies() const { return WaterBodies_; }

  [[nodiscard]] const StreetField &Ways() const { return Ways_; }

  [[nodiscard]] const VegetationTemplates &Vegetation() const { return Templates_; }

  [[nodiscard]] bool Vegetated() const { return Vegetated_; }

  void Restand(double lat, double lon, double budgetMs);
  [[nodiscard]] bool Drained() const;
  [[nodiscard]] bool Ingested() const;
  [[nodiscard]] int FinestZoomOf(Data::DataKind kind) const;

private:
  std::unique_ptr<Data::ContentStore> Store_;
  std::unique_ptr<Data::SourceSet> Sources_;
  std::unique_ptr<TilePool> Pool_;
  std::unique_ptr<GroundStream> Ground_;
  ClassField Cls_;
  GroundMaterials Materials_;
  VegetationTemplates Templates_;
  std::unique_ptr<OsmField> Vectors_;
  BuildingField Footprints_;
  WaterField WaterBodies_;
  StreetField Ways_;
  int SurfaceZoom_ = 0;
  bool Vegetated_ = false;
  bool Opened_ = false;
};

} // namespace outshine::Ground

#endif
