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

class GroundStack {
public:
  GroundStack() = default;
  ~GroundStack() { Close(); }
  GroundStack(const GroundStack &) = delete;
  GroundStack &operator=(const GroundStack &) = delete;

  [[nodiscard]] bool Open(std::string_view cacheDir, std::string_view assetsDir,
                          std::span<const Provider> providers, double focusLat,
                          double focusLon, Data::Transport &wire, Sink &say,
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
  [[nodiscard]] const WaterField &WaterBodies() const { return WaterBodies_; }
  [[nodiscard]] const StreetField &Ways() const { return Ways_; }
  [[nodiscard]] const VegetationTemplates &Vegetation() const { return Templates_; }
  [[nodiscard]] bool Vegetated() const { return Vegetated_; }

  void Restand(double lat, double lon);
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

}

#endif
