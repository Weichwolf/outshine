/* WHAT THE TWO UV FLOATS OF A BUILDING VERTEX MEAN. The generator writes them and the fragment stage
 * reads them; neither may include the other, so the encoding is stated HERE, once, and the shader
 * gets its constants from this file rather than from a second copy that can drift.
 *
 *   uv.x >= 0   a WALL. uv.x = kBayCeil * (style + kStyleCount * standing) + bay coordinate. Whole
 *               bays fall on the piers, so fract() is the position across one bay and a wall of zero
 *               bays never reaches the middle of one — which is how a party wall stays blind.
 *   uv.y        on a wall, kStoreyCeil * identity + 1 + STOREYS over the part's own foot. The +1
 *               keeps a wall sunk below its foot out of the identity below it, and it is an integer
 *               so fract(uv.y) is still the position up one storey. One storey is 1.0 for every
 *               building in the town, so the opening rhythm is one function with nothing per
 *               building to pass and the storey height that varies by class varies in the GEOMETRY.
 *   uv.x < 0    NOT a wall: uv.x = -(kind + kFacadeStride * identity). uv.y is then metres over the
 *               structure's base.
 *
 * WHY THE IDENTITY RIDES IN uv.y AND THE STANDING IN uv.x. A bay coordinate is read through fract()
 * and its absolute size is what limits it: at 2^18 an f32 resolves a bay to 1.5 cm, and the window
 * march inside an opening would band. uv.y is a storey count that never passes 40, so 64 identities
 * multiplied onto it stay under 2^12 and resolve to a tenth of a millimetre. The standing is two
 * bits and rides where it costs 3 mm.
 *
 * A style is a MATERIAL LANGUAGE, not a use: it says which facade grammar a wall is shaded in, the
 * way a colour row does. Nothing in the engine's physics reads it. */
#ifndef FACADEUV_H
#define FACADEUV_H

#include <string>

namespace outshine {

enum class Facade : int {
  Wall = 0, RoofPitch = 1, RoofFlat = 2, Soffit = 3, Ledge = 4, Trim = 5, Metal = 6, Parapet = 7,
  Plinth = 8, Kerb = 9, Pavement = 10
};

enum class FacadeStyle : int {
  Outbuilding = 0, House = 1, Terrace = 2, Block = 3, Hall = 4, Tower = 5, Spire = 6
};

/* WHERE A WALL STANDS IN THE PLOT, which is the one thing a footprint alone cannot say and every
 * grammar of a facade depends on. The entrance is a single bay of the front, split out of it by the
 * mesher, so a door is one bay wide wherever the face is long. */
enum class Standing : int { Back = 0, Front = 1, Entrance = 2 };

/* Wide enough that no wall reaches it: the longest OSM outline in a German town is under 400 m and
 * a bay is never under 2 m, so 256 bays on one face is out of reach. */
constexpr float kBayCeil = 256.0f;
constexpr int kStyleCount = 8;
/* Higher than any storey a wall reaches, so the identity above it and the storey below it cannot
 * meet. A hall's clear height is one storey and a tower's is forty. */
constexpr float kStoreyCeil = 64.0f;
constexpr int kFacadeStride = 16;
constexpr int kIdentCount = 64;

inline float FacadeUvX(FacadeStyle style, Standing standing, float bay) {
  return kBayCeil * (float)((int)style + kStyleCount * (int)standing) + bay;
}

inline float FacadeUvY(int ident, float storeysOverFoot) {
  return kStoreyCeil * (float)ident + 1.0f + storeysOverFoot;
}

inline float FaceUvX(Facade kind, int ident) {
  return -(float)((int)kind + kFacadeStride * ident);
}

inline std::string FacadeUvWGSL() {
  const auto k = [](int v) { return std::to_string(v); };
  return "const kBayCeil : f32 = " + std::to_string((int)kBayCeil) + ".0;\n"
         "const kStyleCount : i32 = " + k(kStyleCount) + ";\n"
         "const kStoreyCeil : f32 = " + std::to_string((int)kStoreyCeil) + ".0;\n"
         "const kFacadeStride : i32 = " + k(kFacadeStride) + ";\n"
         "const kOutbuilding : i32 = " + k((int)FacadeStyle::Outbuilding) + ";\n"
         "const kHouse : i32 = " + k((int)FacadeStyle::House) + ";\n"
         "const kTerrace : i32 = " + k((int)FacadeStyle::Terrace) + ";\n"
         "const kBlock : i32 = " + k((int)FacadeStyle::Block) + ";\n"
         "const kHall : i32 = " + k((int)FacadeStyle::Hall) + ";\n"
         "const kTower : i32 = " + k((int)FacadeStyle::Tower) + ";\n"
         "const kSpire : i32 = " + k((int)FacadeStyle::Spire) + ";\n"
         "const kFront : i32 = " + k((int)Standing::Front) + ";\n"
         "const kEntrance : i32 = " + k((int)Standing::Entrance) + ";\n"
         "const kRoofPitch : i32 = " + k((int)Facade::RoofPitch) + ";\n"
         "const kRoofFlat : i32 = " + k((int)Facade::RoofFlat) + ";\n"
         "const kSoffit : i32 = " + k((int)Facade::Soffit) + ";\n"
         "const kLedge : i32 = " + k((int)Facade::Ledge) + ";\n"
         "const kTrim : i32 = " + k((int)Facade::Trim) + ";\n"
         "const kMetal : i32 = " + k((int)Facade::Metal) + ";\n"
         "const kParapet : i32 = " + k((int)Facade::Parapet) + ";\n"
         "const kPlinth : i32 = " + k((int)Facade::Plinth) + ";\n"
         "const kKerb : i32 = " + k((int)Facade::Kerb) + ";\n"
         "const kPavement : i32 = " + k((int)Facade::Pavement) + ";\n";
}

}  // namespace outshine
#endif
