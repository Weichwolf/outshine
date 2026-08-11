/* WHAT THE TWO UV FLOATS OF A BUILDING VERTEX MEAN. The generator writes them and the fragment stage
 * reads them; neither may include the other, so the encoding is stated HERE, once, and the shader
 * gets its constants from this file rather than from a second copy that can drift.
 *
 *   uv.x >= 0   a WALL. uv.x = kUseStride * style + bay coordinate. Whole bays fall on the piers, so
 *               fract() is the position across one bay and a wall of zero bays never reaches the
 *               middle of one — which is how a party wall stays blind. The style rides in the
 *               integer part because fract() cannot see it and the cluster DAG's seam test only
 *               looks at the sign.
 *   uv.y        on a wall, STOREYS over the base — not metres. One storey is 1.0 for every building
 *               in the town, so the opening rhythm is one function with nothing per building to pass
 *               and the storey height that varies by class varies in the GEOMETRY.
 *   uv.x < 0    NOT a wall: the value is -Facade. uv.y is then metres over the base.
 *
 * A style is a MATERIAL LANGUAGE, not a use: it says which facade grammar a wall is shaded in, the
 * way a colour row does. Nothing in the engine's physics reads it. */
#ifndef FACADEUV_H
#define FACADEUV_H

#include <string>

namespace outshine {

enum class Facade : int {
  Wall = 0, RoofPitch = 1, RoofFlat = 2, Soffit = 3, Ledge = 4, Trim = 5, Metal = 6, Parapet = 7
};

enum class FacadeStyle : int {
  Outbuilding = 0, House = 1, Terrace = 2, Block = 3, Hall = 4, Tower = 5, Spire = 6
};

/* Wide enough that no wall reaches it: the longest OSM outline in a German town is under 400 m and
 * a bay is never under 2 m, so 256 bays on one face is out of reach; small enough that the sum
 * keeps a bay's fraction to a thousandth in f32. */
constexpr float kUseStride = 256.0f;

inline float FacadeUvX(FacadeStyle style, float bay) {
  return kUseStride * (float)(int)style + bay;
}

inline std::string FacadeUvWGSL() {
  const auto k = [](int v) { return std::to_string(v); };
  return "const kUseStride : f32 = " + std::to_string((int)kUseStride) + ".0;\n"
         "const kOutbuilding : i32 = " + k((int)FacadeStyle::Outbuilding) + ";\n"
         "const kHouse : i32 = " + k((int)FacadeStyle::House) + ";\n"
         "const kTerrace : i32 = " + k((int)FacadeStyle::Terrace) + ";\n"
         "const kBlock : i32 = " + k((int)FacadeStyle::Block) + ";\n"
         "const kHall : i32 = " + k((int)FacadeStyle::Hall) + ";\n"
         "const kTower : i32 = " + k((int)FacadeStyle::Tower) + ";\n"
         "const kSpire : i32 = " + k((int)FacadeStyle::Spire) + ";\n"
         "const kRoofPitch : i32 = " + k((int)Facade::RoofPitch) + ";\n"
         "const kRoofFlat : i32 = " + k((int)Facade::RoofFlat) + ";\n"
         "const kSoffit : i32 = " + k((int)Facade::Soffit) + ";\n"
         "const kLedge : i32 = " + k((int)Facade::Ledge) + ";\n"
         "const kTrim : i32 = " + k((int)Facade::Trim) + ";\n"
         "const kMetal : i32 = " + k((int)Facade::Metal) + ";\n"
         "const kParapet : i32 = " + k((int)Facade::Parapet) + ";\n";
}

}  // namespace outshine
#endif
