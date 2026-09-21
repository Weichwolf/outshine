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

  [[nodiscard]] bool MarksPrepared() noexcept {
    if (Prepared_) { return false; }
    Prepared_ = true;
    return true;
  }

  [[nodiscard]] SheetPhase SheetBuilding() const noexcept { return SheetBuilding_; }

  [[nodiscard]] bool CompletesSheetPhase() noexcept {
    if (!Prepared_) { return false; }
    switch (SheetBuilding_) {
      case SheetPhase::NeedsRefinement: SheetBuilding_ = SheetPhase::NeedsHalos; return true;
      case SheetPhase::NeedsHalos: SheetBuilding_ = SheetPhase::NeedsMesh; return true;
      case SheetPhase::NeedsMesh: SheetBuilding_ = SheetPhase::Ready; return true;
      case SheetPhase::Ready: return false;
    }
    return false;
  }

  [[nodiscard]] Stage NextStage() const noexcept { return NextStage_; }

  [[nodiscard]] bool CompletesStage() noexcept {
    if (!Prepared_ || SheetBuilding_ != SheetPhase::Ready) { return false; }
    switch (NextStage_) {
      case Stage::NeedsClasses: NextStage_ = Stage::NeedsGroundSurface; return true;
      case Stage::NeedsGroundSurface: NextStage_ = Stage::NeedsModels; return true;
      case Stage::NeedsModels: NextStage_ = Stage::NeedsBakes; return true;
      case Stage::NeedsBakes: NextStage_ = Stage::NeedsCorridors; return true;
      case Stage::NeedsCorridors: NextStage_ = Stage::NeedsEarthworks; return true;
      case Stage::NeedsEarthworks: NextStage_ = Stage::NeedsTerrainMesh; return true;
      case Stage::NeedsTerrainMesh: NextStage_ = Stage::NeedsWater; return true;
      case Stage::NeedsWater: NextStage_ = Stage::NeedsGeometry; return true;
      case Stage::NeedsGeometry: NextStage_ = Stage::NeedsPublication; return true;
      case Stage::NeedsPublication: return false;
    }
    return false;
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
