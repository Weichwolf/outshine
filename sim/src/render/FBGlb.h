/* A minimal glTF-binary reader: a 12-byte header plus a JSON chunk and a BIN chunk. It reads exactly
 * what a rigid airframe needs — positions, normals, UVs, indices, the node hierarchy with its TRS, and
 * each material's base colour (factor or texture) — and refuses anything it cannot represent instead of
 * approximating it (skins, morph targets, sparse accessors, non-triangle modes, strided views).
 *
 * Refusing is the point: the `.glb` files under the mod's models root are OUR files, built by
 * build_f16.py, and a reader that silently dropped an attribute would show a wrong aeroplane rather
 * than fail. What the F-16 asset actually contains is asserted in doc/render/units-visual.md.
 *
 * Indices are rewritten GLOBAL at parse time (primitive base vertex folded in), so the whole file is
 * one vertex range and one index range and a draw is a (first, count) pair. */
#ifndef FBGLB_H
#define FBGLB_H

#include <cstdint>
#include <string>
#include <vector>

namespace FlightBox::Render {

struct FBGlbMaterial {
  std::string Name;
  float BaseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};   /* linear, glTF pbrMetallicRoughness.baseColorFactor */
  int BaseColorTex = -1;                            /* index into FBGlb::Images, -1 = factor only */
  bool Blend = false;                               /* alphaMode BLEND */
};

struct FBGlbPrimitive {
  uint32_t First = 0, Count = 0;               /* into FBGlb::Idx */
  uint32_t VertexFirst = 0, VertexCount = 0;   /* into FBGlb::Pos/Nrm/Uv */
  int Material = -1;
};

struct FBGlbNode {
  std::string Name;
  int Mesh = -1;
  float T[3] = {0.0f, 0.0f, 0.0f};
  float R[4] = {0.0f, 0.0f, 0.0f, 1.0f};   /* quaternion xyzw, glTF order */
  float S[3] = {1.0f, 1.0f, 1.0f};
  uint32_t FirstChild = 0, ChildCount = 0;   /* into FBGlb::Children */
};

/* Decoded RGBA8, level 0 only; the caller builds mips (FBMips.h). Square power-of-two by contract. */
struct FBGlbImage {
  int W = 0, H = 0;
  std::vector<uint8_t> Rgba;
};

class FBGlb {
public:
  bool LoadMemory(const uint8_t *data, size_t len);
  bool LoadFile(const char *path);

  const char *Error() const { return Err_.c_str(); }

  size_t VertexCount() const { return Pos.size() / 3; }

  std::vector<float> Pos, Nrm, Uv;
  std::vector<uint32_t> Idx;
  std::vector<FBGlbPrimitive> Prims;
  std::vector<uint32_t> MeshFirstPrim, MeshPrimCount;
  std::vector<FBGlbNode> Nodes;
  std::vector<int> Children;
  std::vector<int> Roots;
  std::vector<FBGlbMaterial> Materials;
  std::vector<FBGlbImage> Images;

private:
  bool Fail(const char *why);

  std::string Err_;
};

} // namespace FlightBox::Render
#endif
