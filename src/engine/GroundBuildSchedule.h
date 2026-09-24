#ifndef OUTSHINE_ENGINE_GROUNDBUILDSCHEDULE_H
#define OUTSHINE_ENGINE_GROUNDBUILDSCHEDULE_H

#include <cstdint>
#include <string_view>

namespace outshine::Core {
class GroundBuildSchedule {
public:
  enum class SheetPhase : uint8_t { NeedsFields, NeedsRefinement, NeedsHalos, NeedsMesh, Ready };
  enum class Stage : uint8_t {
    NeedsClasses,
    NeedsGroundSurface,
    NeedsModels,
    NeedsNetwork,
    NeedsRoadAlignments,
    NeedsCorridors,
    NeedsBakes,
    NeedsEarthworks,
    NeedsTerrainMesh,
    NeedsWater,
    NeedsGeometry,
    NeedsPublication
  };

  [[nodiscard]] bool Prepared() const noexcept { return Prepared_; }

  [[nodiscard]] bool MarkPrepared() noexcept {
    if (Prepared_) { return false; }
    Prepared_ = true;
    return true;
  }

  [[nodiscard]] SheetPhase CurrentSheetPhase() const noexcept { return CurrentSheetPhase_; }

  [[nodiscard]] bool AdvanceSheetPhase() noexcept {
    if (!Prepared_) { return false; }
    switch (CurrentSheetPhase_) {
      case SheetPhase::NeedsFields: CurrentSheetPhase_ = SheetPhase::NeedsRefinement; return true;
      case SheetPhase::NeedsRefinement: CurrentSheetPhase_ = SheetPhase::NeedsHalos; return true;
      case SheetPhase::NeedsHalos: CurrentSheetPhase_ = SheetPhase::NeedsMesh; return true;
      case SheetPhase::NeedsMesh: CurrentSheetPhase_ = SheetPhase::Ready; return true;
      case SheetPhase::Ready: return false;
    }
    return false;
  }

  [[nodiscard]] Stage CurrentStage() const noexcept { return CurrentStage_; }

  [[nodiscard]] bool AdvanceStage() noexcept {
    if (!Prepared_ || CurrentSheetPhase_ != SheetPhase::Ready) { return false; }
    switch (CurrentStage_) {
      case Stage::NeedsClasses: CurrentStage_ = Stage::NeedsGroundSurface; return true;
      case Stage::NeedsGroundSurface: CurrentStage_ = Stage::NeedsModels; return true;
      case Stage::NeedsModels: CurrentStage_ = Stage::NeedsNetwork; return true;
      case Stage::NeedsNetwork: CurrentStage_ = Stage::NeedsRoadAlignments; return true;
      case Stage::NeedsRoadAlignments: CurrentStage_ = Stage::NeedsCorridors; return true;
      case Stage::NeedsCorridors: CurrentStage_ = Stage::NeedsBakes; return true;
      case Stage::NeedsBakes: CurrentStage_ = Stage::NeedsEarthworks; return true;
      case Stage::NeedsEarthworks: CurrentStage_ = Stage::NeedsTerrainMesh; return true;
      case Stage::NeedsTerrainMesh: CurrentStage_ = Stage::NeedsWater; return true;
      case Stage::NeedsWater: CurrentStage_ = Stage::NeedsGeometry; return true;
      case Stage::NeedsGeometry: CurrentStage_ = Stage::NeedsPublication; return true;
      case Stage::NeedsPublication: return false;
    }
    return false;
  }

  [[nodiscard]] std::string_view Status() const noexcept {
    switch (CurrentSheetPhase_) {
      case SheetPhase::NeedsFields: return "sheet-fields";
      case SheetPhase::NeedsRefinement: return "sheet-refinement";
      case SheetPhase::NeedsHalos: return "sheet-halos";
      case SheetPhase::NeedsMesh: return "sheet-mesh";
      case SheetPhase::Ready: break;
    }
    switch (CurrentStage_) {
      case Stage::NeedsClasses: return "classes";
      case Stage::NeedsGroundSurface: return "ground-surface";
      case Stage::NeedsModels: return "models";
      case Stage::NeedsNetwork: return "network";
      case Stage::NeedsRoadAlignments: return "road-alignments";
      case Stage::NeedsCorridors: return "corridors";
      case Stage::NeedsBakes: return "structure-bakes";
      case Stage::NeedsEarthworks: return "earthworks";
      case Stage::NeedsTerrainMesh: return "terrain-mesh";
      case Stage::NeedsWater: return "water";
      case Stage::NeedsGeometry: return "geometry";
      case Stage::NeedsPublication: return "publication";
    }
    return "unknown";
  }

private:
  SheetPhase CurrentSheetPhase_ = SheetPhase::NeedsFields;
  Stage CurrentStage_ = Stage::NeedsClasses;
  bool Prepared_ = false;
};
}

#endif
