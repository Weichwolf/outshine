/* ONE airframe as the picture needs it: the glTF LOD stack of the mod's models root flattened into a single
 * vertex range per level, plus the PART table its sidecar declares.
 *
 * THE FLATTENING is the whole idea. A glTF node tree would be one draw per node and one matrix upload
 * per node; here every mesh is baked into the frame of the nearest articulated ancestor at LOAD time and
 * tagged with that part's index, so a whole aeroplane — 133 primitives, eight materials, twenty hinges —
 * is ONE indexed draw and the only per-frame work is up to kMaxUnitParts small matrices.
 *
 * NOTHING HERE TOUCHES WEBGPU: this is the CPU half, so what it produces can be measured without a
 * device. UnitsStage owns the buffers. Vertrag: doc/render/units-visual.md. */
#ifndef UNITMODEL_H
#define UNITMODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace outshine::Render {

/* The published values a part may follow (units/Unit.h UnitArticulation), as an ORDINAL so the
 * drawing side carries no dependency on the simulation's types. World maps one to the other. */
enum class ArtChannel : uint8_t {
  AileronL, AileronR, ElevonL, ElevonR, Rudder, Lef, Speedbrake, Gear, Hook, Canopy, Count,
  None = 255
};

constexpr int kMaxUnitParts = 32;
constexpr int kMaxUnitMaterials = 16;

struct UnitVertex {
  float P[3], N[3], Uv[2];
  uint32_t Tag;   /* part index in the low 16 bits, material index in the high 16 */
};

class UnitModel {
public:
  /* A hinge. `Base` is the 3x4 affine from this part's frame into its PARENT PART's frame, already
   * carrying the node's own translation and orientation; the hinge angle is a rotation about the part
   * frame's local X and is applied after it. Part 0 is the static airframe and never rotates. */
  struct Part {
    std::string Node;
    ArtChannel Ch = ArtChannel::None;
    int Parent = 0;
    float LoDeg = 0.0f, HiDeg = 0.0f;
    /* The 0..1 channel runs the OTHER way for this part: the asset stands on its wheels, so a leg's
     * zero pose is EXTENDED while gear/gear-pos-norm reads 1 there. Doors are built flush and open
     * with the value, hence the distinction rather than one blanket sign. */
    bool Inverted = false;
    float Base[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  };

  struct Lod {
    std::vector<UnitVertex> Verts;
    std::vector<uint32_t> Idx;
    std::vector<Part> Parts;
    float MatColor[kMaxUnitMaterials][4] = {};   /* rgb linear, w = 1 -> sample the base-colour texture */
    int MatCount = 0;
    int TexW = 0;                     /* 0 = no base-colour texture at all */
    std::vector<uint8_t> TexRgba;     /* level 0 RGBA8, square power-of-two (Mips builds the chain) */
    double MaxRangeM = 0.0;           /* 0 = the last stage: no upper bound */
    int Triangles = 0;
    /* WHERE THE EXHAUST LEAVES, measured off the mesh itself: the bbox of every node named `nozzle*`,
     * taken at its aft face. The asset owns the number, the renderer only reads it — which is what
     * keeps a plume attached to the aeroplane it belongs to without a second table of offsets. */
    bool HasNozzle = false;
    float NozzleOff[3] = {0, 0, 0};   /* model space (glTF: +X right, +Y up, +Z aft) */
    float NozzleRadiusM = 0.0f;
  };

  /* `dir` holds `<type>.asset.json` and the `<type>_L*.glb` it names. Missing levels are skipped, so a
   * deployment may ship a subset and the picture degrades by exactly the sidecar's stated XOR area. */
  bool LoadDir(const char *typeName, const char *dir);

  const std::string &TypeName() const { return TypeName_; }
  int LodCount() const { return (int)Lods_.size(); }
  const Lod &GetLod(int i) const { return Lods_[(size_t)i]; }

  /* The finest level whose stated switch range still covers `rangeM`; the last level otherwise. */
  int PickLod(double rangeM) const;

  /* Degrees for `part` given the published channel values, clamped to the sidecar's limits. */
  static float PartAngleDeg(const Part &part, const float art[(int)ArtChannel::Count]);

private:
  bool BuildLod(const char *glbPath, double maxRangeM);

  struct Component {   /* one sidecar `components` row, node wildcards already expanded */
    ArtChannel Ch = ArtChannel::None;
    float LoDeg = 0.0f, HiDeg = 0.0f;
    bool Inverted = false;
  };
  const Component *FindComponent(const std::string &node) const;

  std::string TypeName_;
  std::vector<std::pair<std::string, Component>> Components_;
  std::vector<Lod> Lods_;
};

} // namespace outshine::Render
#endif
