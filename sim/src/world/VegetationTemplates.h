/* THE 8-BIT ALBEDO INDEX and the table it indexes (doc/render/visual-target.md §5): the coarse albedo
 * triple is quantised to one byte, and that byte selects a template which declares a species mix,
 * densities, ground cover and clutter — never a texture. Loaded from JSON, so a template gains a field
 * without a rebuild.
 *
 * THE QUANTISER IS A LUT, NOT A BIT CUT, and that is a measured decision rather than a taste. The OSM
 * bake's palette (tiles/src/style.h) puts 51 declared key colours into a narrow, high-lightness band:
 * the best uniform 8-bit RGB cut of it (3-1-4) still merges four buckets across template boundaries,
 * and the natural-looking 3-3-2 merges eight — residential lands on sand, meadow on allotments. A
 * 32x32x32 grid over the same space separates all nine templates with ZERO collisions (min
 * inter-template key distance 0.0126 in gamma-2 space, cell size 0.031, measured: no cell holds two).
 * So the CPU resolves 32768 cells to a template index once, and both the shader and the tile indexer
 * READ that table instead of each running a formula that has to be kept identical. */
#ifndef VEGETATIONTEMPLATES_H
#define VEGETATIONTEMPLATES_H

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "GroundMaterials.h"

namespace outshine::World {

class VegetationTemplates {
public:
  /* One 128-byte row per template, uploaded verbatim as a storage buffer. The field meanings are pinned
   * by the WGSL `VegRow` in TilesStage — change one, change the other. */
  struct Row {
    float Ground[4];    /* rgb LINEAR reflectance of the ground material, w = roughness */
    float Litter[4];    /* rgb LINEAR reflectance of the litter material, w = roughness */
    float GroundSurf[4];/* x = grain (m), y = height amplitude (m), z,w = the two detail octaves (m) */
    float LitterSurf[4];/* the same four for the litter material */
    float Mix[4];       /* x = litter coverage, y = detail contrast, z,w = specular scale ground/litter */
    float Grass[4];     /* rgb linear fresh blade, w = blades/m^2 */
    float Dry[4];       /* rgb linear dry blade, w = blade height (m) */
    float Param[4];     /* x = height jitter, y = blade width (m), z = clutter/m^2, w = dry fraction */
  };

  static constexpr int kLutSide = 32;
  static constexpr int kLutCells = kLutSide * kLutSide * kLutSide;

  /* `mats` must be Ready(): a template that names a class this table does not know is a load error,
   * not a silent default — an unresolved ground class draws the wrong material everywhere it wins. */
  bool Load(const char *path, const GroundMaterials &mats);

  const Row *Rows() const { return Table_.data(); }
  size_t RowBytes() const { return Table_.size() * sizeof(Row); }
  const uint32_t *Lut() const { return Lut_.data(); }
  size_t LutBytes() const { return Lut_.size() * sizeof(uint32_t); }
  bool Ready() const { return !Table_.empty() && !Lut_.empty(); }
  size_t TemplateCount() const { return Table_.size(); }
  const std::string &Name(size_t i) const { return Names_[i]; }
  const std::string &Error() const { return Error_; }

  /* Cells holding key colours of two different templates: 8 bits could not tell them apart, and the
   * loser is silently gone. A number that must stay 0, reported rather than assumed. */
  int LutCollisions() const { return Collisions_; }

  static float SrgbToLinear(float s) {
    return s <= 0.04045f ? s / 12.92f : std::pow((s + 0.055f) / 1.055f, 2.4f);
  }

  /* Gamma-2 (sqrt of linear) is the LUT's address space: perceptually even enough that a uniform grid
   * over it spends its cells where a palette actually varies, and one instruction in WGSL. */
  static int Cell(float lr, float lg, float lb) {
    const auto q = [](float v) {
      const float c = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
      const int i = (int)(std::sqrt(c) * (float)kLutSide);
      return i < 0 ? 0 : (i > kLutSide - 1 ? kLutSide - 1 : i);
    };
    return (q(lb) * kLutSide + q(lg)) * kLutSide + q(lr);
  }

private:
  std::vector<Row> Table_;
  std::vector<uint32_t> Lut_;
  std::vector<std::string> Names_;
  std::string Error_;
  int Collisions_ = 0;
};

} // namespace outshine::World
#endif
