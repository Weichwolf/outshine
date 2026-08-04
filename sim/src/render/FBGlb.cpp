#include "FBGlb.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include "FBJson.h"
#include "FBLog.h"

/* stb_image's implementation lives in world/terrain/terrain.cpp — declared, never re-implemented:
 * two implementations in one link would collide (the same statement world/FBTerrainLoader.cpp makes). */
extern "C" {
unsigned char *stbi_load_from_memory(const unsigned char *buffer, int len, int *x, int *y,
                                     int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);
}

namespace FlightBox::Render {

namespace {

constexpr uint32_t kMagic = 0x46546C67;   /* 'glTF' */
constexpr uint32_t kChunkJson = 0x4E4F534A;
constexpr uint32_t kChunkBin = 0x004E4942;

uint32_t Rd32(const uint8_t *p) {
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}

struct Accessor {
  const uint8_t *Data = nullptr;
  uint32_t Count = 0, Comps = 0, CompType = 0;
};

}   // namespace

bool FBGlb::Fail(const char *why) {
  Err_ = why;
  FBLog::Error("render", "glb_reject", {{"why", why}});
  return false;
}

bool FBGlb::LoadFile(const char *path) {
  std::FILE *f = std::fopen(path, "rb");
  if (!f) return Fail("open");
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n <= 12) { std::fclose(f); return Fail("truncated"); }
  std::vector<uint8_t> buf((size_t)n);
  const size_t rd = std::fread(buf.data(), 1, (size_t)n, f);
  std::fclose(f);
  if (rd != (size_t)n) return Fail("read");
  return LoadMemory(buf.data(), buf.size());
}

bool FBGlb::LoadMemory(const uint8_t *data, size_t len) {
  Pos.clear(); Nrm.clear(); Uv.clear(); Idx.clear();
  Prims.clear(); MeshFirstPrim.clear(); MeshPrimCount.clear();
  Nodes.clear(); Children.clear(); Roots.clear(); Materials.clear(); Images.clear();

  if (!data || len < 12 || Rd32(data) != kMagic) return Fail("magic");
  if (Rd32(data + 4) != 2) return Fail("version");

  const uint8_t *json = nullptr, *bin = nullptr;
  size_t jsonLen = 0, binLen = 0;
  for (size_t off = 12; off + 8 <= len;) {
    const uint32_t cl = Rd32(data + off), ct = Rd32(data + off + 4);
    if (off + 8 + cl > len) return Fail("chunk_overrun");
    if (ct == kChunkJson) { json = data + off + 8; jsonLen = cl; }
    else if (ct == kChunkBin) { bin = data + off + 8; binLen = cl; }
    off += 8 + cl;
  }
  if (!json) return Fail("no_json");

  FBJson doc;
  if (!doc.Parse((const char *)json, jsonLen)) return Fail("json_parse");
  const FBJson::Ref root = doc.Root();

  /* ---- buffer views ---- */
  const FBJson::Ref jviews = root["bufferViews"];
  std::vector<const uint8_t *> viewPtr(jviews.Size(), nullptr);
  std::vector<size_t> viewLen(jviews.Size(), 0);
  for (size_t i = 0; i < jviews.Size(); i++) {
    const FBJson::Ref v = jviews[i];
    if (v["byteStride"].Valid()) return Fail("strided_view");
    const size_t off = (size_t)v["byteOffset"].Num(0.0), bl = (size_t)v["byteLength"].Num(0.0);
    if (!bin || off + bl > binLen) return Fail("view_range");
    viewPtr[i] = bin + off;
    viewLen[i] = bl;
  }

  /* ---- accessors ---- */
  const FBJson::Ref jacc = root["accessors"];
  std::vector<Accessor> acc(jacc.Size());
  for (size_t i = 0; i < jacc.Size(); i++) {
    const FBJson::Ref a = jacc[i];
    if (a["sparse"].Valid()) return Fail("sparse_accessor");
    const int bv = a["bufferView"].Int(-1);
    if (bv < 0 || (size_t)bv >= viewPtr.size()) return Fail("accessor_view");
    const std::string type = a["type"].Str();
    const uint32_t comps = type == "SCALAR" ? 1u : type == "VEC2" ? 2u : type == "VEC3" ? 3u
                         : type == "VEC4" ? 4u : 0u;
    if (!comps) return Fail("accessor_type");
    acc[i].Data = viewPtr[(size_t)bv] + (size_t)a["byteOffset"].Num(0.0);
    acc[i].Count = (uint32_t)a["count"].Num(0.0);
    acc[i].Comps = comps;
    acc[i].CompType = (uint32_t)a["componentType"].Num(0.0);
  }

  /* ---- materials ---- */
  const FBJson::Ref jmats = root["materials"];
  const FBJson::Ref jtex = root["textures"];
  Materials.resize(jmats.Size());
  std::vector<int> imageWanted;
  for (size_t i = 0; i < jmats.Size(); i++) {
    const FBJson::Ref m = jmats[i];
    FBGlbMaterial &mat = Materials[i];
    mat.Name = m["name"].Str();
    mat.Blend = m["alphaMode"].StrEquals("BLEND");
    const FBJson::Ref pbr = m["pbrMetallicRoughness"];
    const FBJson::Ref bcf = pbr["baseColorFactor"];
    for (size_t k = 0; k < 4 && k < bcf.Size(); k++) mat.BaseColor[k] = (float)bcf[k].Num(1.0);
    const FBJson::Ref bct = pbr["baseColorTexture"];
    if (bct.Valid()) {
      const int t = bct["index"].Int(-1);
      const int src = t >= 0 && (size_t)t < jtex.Size() ? jtex[(size_t)t]["source"].Int(-1) : -1;
      if (src >= 0) {
        mat.BaseColorTex = src;
        if (std::find(imageWanted.begin(), imageWanted.end(), src) == imageWanted.end())
          imageWanted.push_back(src);
      }
    }
  }

  /* ---- images: ONLY those a base colour actually points at. The normal and ORM maps of the F-16
   * asset are 2048^2 PNGs each and this reader has no consumer for them. ---- */
  const FBJson::Ref jimg = root["images"];
  Images.resize(jimg.Size());
  for (int src : imageWanted) {
    if ((size_t)src >= jimg.Size()) return Fail("image_index");
    const FBJson::Ref im = jimg[(size_t)src];
    const int bv = im["bufferView"].Int(-1);
    if (bv < 0 || (size_t)bv >= viewPtr.size()) return Fail("image_view");
    int w = 0, h = 0, comp = 0;
    uint8_t *px = stbi_load_from_memory(viewPtr[(size_t)bv], (int)viewLen[(size_t)bv], &w, &h, &comp, 4);
    if (!px) return Fail("image_decode");
    Images[(size_t)src].W = w;
    Images[(size_t)src].H = h;
    Images[(size_t)src].Rgba.assign(px, px + (size_t)w * (size_t)h * 4);
    stbi_image_free(px);
  }

  /* ---- meshes: every primitive appended to the ONE vertex/index range ---- */
  const FBJson::Ref jmesh = root["meshes"];
  MeshFirstPrim.resize(jmesh.Size());
  MeshPrimCount.resize(jmesh.Size());
  for (size_t i = 0; i < jmesh.Size(); i++) {
    const FBJson::Ref prims = jmesh[i]["primitives"];
    MeshFirstPrim[i] = (uint32_t)Prims.size();
    MeshPrimCount[i] = (uint32_t)prims.Size();
    for (size_t k = 0; k < prims.Size(); k++) {
      const FBJson::Ref p = prims[k];
      if ((int)p["mode"].Num(4.0) != 4) return Fail("non_triangles");
      const FBJson::Ref at = p["attributes"];
      if (at["JOINTS_0"].Valid() || p["targets"].Valid()) return Fail("skinned_or_morphed");
      const int ap = at["POSITION"].Int(-1), an = at["NORMAL"].Int(-1), au = at["TEXCOORD_0"].Int(-1);
      const int ai = p["indices"].Int(-1);
      if (ap < 0 || ai < 0) return Fail("missing_attribute");
      const Accessor &P = acc[(size_t)ap];
      if (P.CompType != 5126 || P.Comps != 3) return Fail("position_format");

      const uint32_t base = (uint32_t)VertexCount();
      const float *pf = (const float *)P.Data;
      Pos.insert(Pos.end(), pf, pf + (size_t)P.Count * 3);
      if (an >= 0 && acc[(size_t)an].CompType == 5126 && acc[(size_t)an].Comps == 3) {
        const float *nf = (const float *)acc[(size_t)an].Data;
        Nrm.insert(Nrm.end(), nf, nf + (size_t)P.Count * 3);
      } else {
        Nrm.resize(Nrm.size() + (size_t)P.Count * 3, 0.0f);
      }
      if (au >= 0 && acc[(size_t)au].CompType == 5126 && acc[(size_t)au].Comps == 2) {
        const float *uf = (const float *)acc[(size_t)au].Data;
        Uv.insert(Uv.end(), uf, uf + (size_t)P.Count * 2);
      } else {
        Uv.resize(Uv.size() + (size_t)P.Count * 2, 0.0f);
      }

      const Accessor &I = acc[(size_t)ai];
      FBGlbPrimitive gp;
      gp.First = (uint32_t)Idx.size();
      gp.Count = I.Count;
      gp.VertexFirst = base;
      gp.VertexCount = P.Count;
      gp.Material = p["material"].Int(-1);
      Idx.resize(Idx.size() + I.Count);
      uint32_t *dst = Idx.data() + gp.First;
      if (I.CompType == 5123) {
        const uint16_t *s = (const uint16_t *)I.Data;
        for (uint32_t v = 0; v < I.Count; v++) dst[v] = base + s[v];
      } else if (I.CompType == 5125) {
        const uint32_t *s = (const uint32_t *)I.Data;
        for (uint32_t v = 0; v < I.Count; v++) dst[v] = base + s[v];
      } else if (I.CompType == 5121) {
        const uint8_t *s = I.Data;
        for (uint32_t v = 0; v < I.Count; v++) dst[v] = base + s[v];
      } else {
        return Fail("index_format");
      }
      Prims.push_back(gp);
    }
  }

  /* ---- nodes ---- */
  const FBJson::Ref jnodes = root["nodes"];
  Nodes.resize(jnodes.Size());
  for (size_t i = 0; i < jnodes.Size(); i++) {
    const FBJson::Ref n = jnodes[i];
    FBGlbNode &nd = Nodes[i];
    nd.Name = n["name"].Str();
    nd.Mesh = n["mesh"].Int(-1);
    if (n["matrix"].Valid()) return Fail("node_matrix");   /* build_f16.py emits TRS; a matrix would be a new asset */
    const FBJson::Ref t = n["translation"], r = n["rotation"], s = n["scale"];
    for (size_t k = 0; k < 3 && k < t.Size(); k++) nd.T[k] = (float)t[k].Num(0.0);
    for (size_t k = 0; k < 4 && k < r.Size(); k++) nd.R[k] = (float)r[k].Num(k == 3 ? 1.0 : 0.0);
    for (size_t k = 0; k < 3 && k < s.Size(); k++) nd.S[k] = (float)s[k].Num(1.0);
    const FBJson::Ref ch = n["children"];
    nd.FirstChild = (uint32_t)Children.size();
    nd.ChildCount = (uint32_t)ch.Size();
    for (size_t k = 0; k < ch.Size(); k++) Children.push_back(ch[k].Int(-1));
  }

  const FBJson::Ref scenes = root["scenes"];
  const FBJson::Ref scene = scenes[(size_t)root["scene"].Int(0)];
  const FBJson::Ref sroots = scene["nodes"];
  for (size_t i = 0; i < sroots.Size(); i++) Roots.push_back(sroots[i].Int(-1));
  if (Roots.empty()) return Fail("no_scene_root");

  return true;
}

} // namespace FlightBox::Render
