#ifndef OUTSHINE_ENGINE_GROUNDBUILDSCHEDULE_H
#define OUTSHINE_ENGINE_GROUNDBUILDSCHEDULE_H

#include <cstdint>
#include <string_view>

namespace outshine::Core {
class GroundBuildSchedule {
public:
  enum class SheetPhase : uint8_t { NeedsRefinement, NeedsHalos, NeedsMesh, Ready };
  enum class Stage : uint8_t {
    NeedsClasses,
    NeedsGroundSurface,
    NeedsModels,
    NeedsBakes,
    NeedsCorridors,
    NeedsEarthworks,
    NeedsTerrainMesh,
    NeedsWater,
    NeedsGeometry,
    NeedsPublication
  };

  [[nodiscard]] bool Prepared() const noexcept { return Prepared_; }

  void MarksPrepared() noexcept { Prepared_ = true; }

  [[nodiscard]] SheetPhase SheetBuilding() const noexcept { return SheetBuilding_; }

  void CompletesSheetPhase() noexcept {
    switch (SheetBuilding_) {
      case SheetPhase::NeedsRefinement: SheetBuilding_ = SheetPhase::NeedsHalos; return;
      case SheetPhase::NeedsHalos: SheetBuilding_ = SheetPhase::NeedsMesh; return;
      case SheetPhase::NeedsMesh: SheetBuilding_ = SheetPhase::Ready; return;
      case SheetPhase::Ready: return;
    }
  }

  [[nodiscard]] Stage NextStage() const noexcept { return NextStage_; }

  void CompletesStage() noexcept {
    switch (NextStage_) {
      case Stage::NeedsClasses: NextStage_ = Stage::NeedsGroundSurface; return;
      case Stage::NeedsGroundSurface: NextStage_ = Stage::NeedsModels; return;
      case Stage::NeedsModels: NextStage_ = Stage::NeedsBakes; return;
      case Stage::NeedsBakes: NextStage_ = Stage::NeedsCorridors; return;
      case Stage::NeedsCorridors: NextStage_ = Stage::NeedsEarthworks; return;
      case Stage::NeedsEarthworks: NextStage_ = Stage::NeedsTerrainMesh; return;
      case Stage::NeedsTerrainMesh: NextStage_ = Stage::NeedsWater; return;
      case Stage::NeedsWater: NextStage_ = Stage::NeedsGeometry; return;
      case Stage::NeedsGeometry: NextStage_ = Stage::NeedsPublication; return;
      case Stage::NeedsPublication: return;
    }
  }

  [[nodiscard]] std::string_view Status() const noexcept {
    switch (SheetBuilding_) {
      case SheetPhase::NeedsRefinement: return "sheet-refinement";
      case SheetPhase::NeedsHalos: return "sheet-halos";
      case SheetPhase::NeedsMesh: return "sheet-mesh";
      case SheetPhase::Ready: break;
    }
    switch (NextStage_) {
      case Stage::NeedsClasses: return "classes";
      case Stage::NeedsGroundSurface: return "ground-surface";
      case Stage::NeedsModels: return "models";
      case Stage::NeedsBakes: return "structure-bakes";
      case Stage::NeedsCorridors: return "corridors";
      case Stage::NeedsEarthworks: return "earthworks";
      case Stage::NeedsTerrainMesh: return "terrain-mesh";
      case Stage::NeedsWater: return "water";
      case Stage::NeedsGeometry: return "geometry";
      case Stage::NeedsPublication: return "publication";
    }
    return "unknown";
  }

private:
  SheetPhase SheetBuilding_ = SheetPhase::NeedsRefinement;
  Stage NextStage_ = Stage::NeedsClasses;
  bool Prepared_ = false;
};
}

#endif
