/* THE CLASS MODEL: one template per ground class, keyed by the OSM (layer, kind) pairs that select it
 * and by nothing else. A template declares a species mix, densities, ground cover and clutter — never
 * a texture. Loaded from JSON, so a template gains a field, a tag or a whole OSM layer without a
 * rebuild.
 *
 * THE KEY IS THE TAG, NOT A COLOUR. The key space used to be `keySrgb`, i.e. the palette
 * tiles/src/style.h picked for a map image: the class model was addressed through a paint. That made
 * two classes unreachable by construction (deciduous and coniferous forest share one colour) and it
 * pinned the whole model to a bake that no longer exists.
 *
 * RANK IS DECLARED PER ROW because nothing upstream declares it: the tile server orders its LAYERS
 * (six literal lines) but inside `land` it relies on the provider's emission order — last drawn wins
 * over features that overlap by 2.41 % of the reference tile. An implicit order is not a
 * deterministic one. Higher rank wins. */
#ifndef VEGETATIONTEMPLATES_H
#define VEGETATIONTEMPLATES_H

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "AlpineLimit.h"
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
    float Edge[4];      /* x = edgeReachM, y = 1 built / 0 grown, z = trees per m^2,
                           w = the ground class's slope.plausibleDeg[1] */
  };

  /* What one declared (layer, kind) selects. `WidthM` is 0 for an area feature and the drawn width in
   * METRES for a line one — the ONE place a road's width is decided, because the infrastructure
   * geometry that later lays the same surface must read the number the classifier used. */
  struct Rule {
    int Tpl = 0;
    int Rank = 0;
    float WidthM = 0.0f;
  };

  /* A blade in its two states, LINEAR reflectance in the same space as GroundMaterials::Albedo.
   * Templates reference it by name so one measurement serves every stand that grows the same leaf. */
  struct Blade {
    float Green[3];
    float Dry[3];
  };

  bool Load(const char *path, const GroundMaterials &mats);

  const Row *Rows() const { return Table_.data(); }
  size_t RowBytes() const { return Table_.size() * sizeof(Row); }
  bool Ready() const { return !Table_.empty(); }
  size_t TemplateCount() const { return Table_.size(); }
  const std::string &Name(size_t i) const { return Names_[i]; }
  const std::string &Error() const { return Error_; }

  /* nullptr = this layer/kind is declared nowhere, which is an ERROR at the call site and not a
   * default: a tag the table does not know is exactly the case that hid 81 barrier ways. A layer may
   * declare a `*` kind, which is the statement that the kind does not change the class. */
  const Rule *Find(std::string_view layer, std::string_view kind) const;

  /* The class of a place OSM says nothing about. Declared, never guessed at the call site. */
  int DefaultTemplate() const { return Default_; }
  /* The DEM's own answer where OSM has none (AlpineLimit.h), and the row it names. */
  const AlpineLimit &Limit() const { return Limit_; }
  int BareRockTemplate() const { return RockTpl_; }
  size_t RuleCount() const { return Rules_.size(); }

  /* WHICH OSM LAYERS THIS CLASS MODEL READS, derived from the rows and never written down twice: the
   * layer set is a property of the declaration, so a new layer is a JSON row and not a C++ edit. The
   * tile server's layer set varies with the PLACE — thirteen of the schema's layers are absent from
   * the demo tile and Manhattan carries its water in layers the Weserbergland does not have. */
  const std::vector<std::string> &Layers() const { return Layers_; }
  /* Layers whose rows declare no width: the areas alone, for a coarse tier where a line is sub-pixel. */
  const std::vector<std::string> &AreaLayers() const { return AreaLayers_; }

private:
  std::vector<Row> Table_;
  std::vector<std::string> Names_;
  std::unordered_map<std::string, Rule> Rules_;   /* key "layer/kind", or the layer's wildcard row */
  std::vector<std::string> Layers_, AreaLayers_;
  AlpineLimit Limit_;
  std::string Error_;
  int Default_ = 0, RockTpl_ = 0;
};

} // namespace outshine::World
#endif
