#include "FBUnitModel.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "FBGlb.h"
#include "FBJson.h"
#include "FBLog.h"

namespace FlightBox::Render {

namespace {

constexpr float kDeg = 57.29577951308232f;

/* 3x4 affine, row-major; the implicit fourth row is (0,0,0,1). */
void Mul34(const float a[12], const float b[12], float o[12]) {
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++)
      o[r * 4 + c] = a[r * 4 + 0] * b[0 * 4 + c] + a[r * 4 + 1] * b[1 * 4 + c] + a[r * 4 + 2] * b[2 * 4 + c];
    o[r * 4 + 3] = a[r * 4 + 0] * b[3] + a[r * 4 + 1] * b[7] + a[r * 4 + 2] * b[11] + a[r * 4 + 3];
  }
}

void TrsToMat(const float t[3], const float q[4], const float s[3], float o[12]) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  const float m[9] = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
                      2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                      2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) o[r * 4 + c] = m[r * 3 + c] * s[c];
    o[r * 4 + 3] = t[r];
  }
}

/* One sidecar `node` cell may name several nodes; the two spellings build_f16.py uses are a numeric
 * range (`ctl.speedbrake.0..3`) and a shared stem (`gear.door.main.l/.r`). Expanding here keeps the node
 * names in the sidecar, where the asset that owns them can change them. */
std::vector<std::string> ExpandNodes(const std::string &cell) {
  std::vector<std::string> out;
  const size_t slash = cell.find('/');
  if (slash != std::string::npos) {
    const std::string head = cell.substr(0, slash);
    out.push_back(head);
    const size_t stem = head.rfind('.');
    size_t p = slash;
    while (p < cell.size()) {
      const size_t next = cell.find('/', p + 1);
      const std::string tail = cell.substr(p + 1, next == std::string::npos ? std::string::npos : next - p - 1);
      if (!tail.empty() && stem != std::string::npos) out.push_back(head.substr(0, stem) + tail);
      if (next == std::string::npos) break;
      p = next;
    }
    return out;
  }
  const size_t dots = cell.find("..");
  if (dots != std::string::npos && dots > 0) {
    size_t lo = dots;
    while (lo > 0 && cell[lo - 1] >= '0' && cell[lo - 1] <= '9') lo--;
    if (lo < dots) {
      const std::string prefix = cell.substr(0, lo);
      const int a = std::atoi(cell.c_str() + lo), b = std::atoi(cell.c_str() + dots + 2);
      for (int i = a; i <= b; i++) out.push_back(prefix + std::to_string(i));
      return out;
    }
  }
  out.push_back(cell);
  return out;
}

FBArtChannel ChannelOf(const std::string &reads) {
  if (reads == "fcs/left-aileron-pos-rad") return FBArtChannel::AileronL;
  if (reads == "fcs/right-aileron-pos-rad") return FBArtChannel::AileronR;
  if (reads == "fcs/dht-left-pos-rad") return FBArtChannel::ElevonL;
  if (reads == "fcs/dht-right-pos-rad") return FBArtChannel::ElevonR;
  if (reads == "fcs/rudder-pos-rad") return FBArtChannel::Rudder;
  if (reads == "fcs/lef-pos-deg") return FBArtChannel::Lef;
  if (reads == "fcs/speedbrake-pos-deg") return FBArtChannel::Speedbrake;
  if (reads == "gear/gear-pos-norm") return FBArtChannel::Gear;
  if (reads == "gear/tailhook-pos-norm") return FBArtChannel::Hook;
  if (reads == "fcs/canopy-pos-norm") return FBArtChannel::Canopy;
  return FBArtChannel::None;
}

bool ChannelIsNorm(FBArtChannel c) {
  return c == FBArtChannel::Gear || c == FBArtChannel::Hook || c == FBArtChannel::Canopy;
}
bool ChannelIsRad(FBArtChannel c) {
  return c == FBArtChannel::AileronL || c == FBArtChannel::AileronR || c == FBArtChannel::ElevonL ||
         c == FBArtChannel::ElevonR || c == FBArtChannel::Rudder;
}

bool ReadWholeFile(const char *path, std::string &out) {
  std::FILE *f = std::fopen(path, "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  const long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (n <= 0) { std::fclose(f); return false; }
  out.resize((size_t)n);
  const size_t rd = std::fread(&out[0], 1, (size_t)n, f);
  std::fclose(f);
  return rd == (size_t)n;
}

}   // namespace

float FBUnitModel::PartAngleDeg(const Part &p, const float art[(int)FBArtChannel::Count]) {
  if (p.Ch == FBArtChannel::None || p.Ch >= FBArtChannel::Count) return 0.0f;
  const float v = art[(int)p.Ch];
  if (ChannelIsNorm(p.Ch)) {
    float t = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    if (p.Inverted) t = 1.0f - t;
    return p.LoDeg + (p.HiDeg - p.LoDeg) * t;
  }
  const float a = ChannelIsRad(p.Ch) ? v * kDeg : v;
  const float lo = p.LoDeg < p.HiDeg ? p.LoDeg : p.HiDeg;
  const float hi = p.LoDeg < p.HiDeg ? p.HiDeg : p.LoDeg;
  return a < lo ? lo : (a > hi ? hi : a);
}

const FBUnitModel::Component *FBUnitModel::FindComponent(const std::string &node) const {
  for (const auto &c : Components_)
    if (c.first == node) return &c.second;
  return nullptr;
}

int FBUnitModel::PickLod(double rangeM) const {
  for (size_t i = 0; i + 1 < Lods_.size(); i++)
    if (Lods_[i].MaxRangeM > 0.0 && rangeM <= Lods_[i].MaxRangeM) return (int)i;
  return Lods_.empty() ? -1 : (int)Lods_.size() - 1;
}

bool FBUnitModel::LoadDir(const char *typeName, const char *dir) {
  TypeName_ = typeName ? typeName : "";
  Components_.clear();
  Lods_.clear();

  char path[512];
  std::snprintf(path, sizeof path, "%s/%s.asset.json", dir, TypeName_.c_str());
  std::string text;
  if (!ReadWholeFile(path, text)) {
    FBLog::Error("render", "unit_model_sidecar", {{"path", path}});
    return false;
  }
  FBJson doc;
  if (!doc.Parse(text.c_str(), text.size())) {
    FBLog::Error("render", "unit_model_sidecar_parse", {{"path", path}});
    return false;
  }
  const FBJson::Ref root = doc.Root();

  const FBJson::Ref comps = root["components"];
  for (size_t i = 0; i < comps.Size(); i++) {
    const FBJson::Ref c = comps[i];
    Component cm;
    cm.Ch = ChannelOf(c["reads"].Str());
    if (cm.Ch == FBArtChannel::None) continue;   /* `(statisch)` rows: rails and stores */
    const FBJson::Ref lim = c["limits_deg"];
    cm.LoDeg = (float)lim[(size_t)0].Num(0.0);
    cm.HiDeg = (float)lim[(size_t)1].Num(0.0);
    for (const std::string &n : ExpandNodes(c["node"].Str())) {
      /* The asset stands on its wheels: a LEG's built pose is extended, which is where gear-pos-norm
       * reads 1, while a DOOR is built flush and opens as the value rises. */
      cm.Inverted = cm.Ch == FBArtChannel::Gear && n.compare(0, 10, "gear.door.") != 0;
      Components_.emplace_back(n, cm);
    }
  }

  const FBJson::Ref lods = root["lods"];
  const FBJson::Ref steps = root["lod_switch"]["steps"];
  int built = 0;
  for (size_t i = 0; i < lods.Size(); i++) {
    const std::string file = lods[i]["file"].Str();
    if (file.empty()) continue;
    std::snprintf(path, sizeof path, "%s/%s", dir, file.c_str());
    /* The sidecar's own rule: max_range_m is the distance beyond which dropping to the NEXT level is
     * invisible. `null` (the last step) means no upper bound. */
    const double maxR = i < steps.Size() ? steps[i]["max_range_m"].Num(0.0) : 0.0;
    if (BuildLod(path, maxR)) built++;
  }
  if (!built) return false;

  int tris = 0;
  for (const Lod &l : Lods_) tris += l.Triangles;
  FBLog::Info("render", "unit_model", {{"type", TypeName_}, {"lods", built},
                                       {"parts", (int)Lods_.front().Parts.size()},
                                       {"materials", Lods_.front().MatCount},
                                       {"tex0", Lods_.front().TexW},
                                       {"trisTotal", tris},
                                       {"lod0MaxRangeM", Lods_.front().MaxRangeM},
                                       {"nozzleZ", Lods_.front().NozzleOff[2]},
                                       {"nozzleRadM", Lods_.front().NozzleRadiusM}});
  return true;
}

bool FBUnitModel::BuildLod(const char *glbPath, double maxRangeM) {
  FBGlb glb;
  if (!glb.LoadFile(glbPath)) {
    FBLog::Error("render", "unit_model_glb", {{"path", glbPath}, {"why", glb.Error()}});
    return false;
  }

  Lod lod;
  lod.MaxRangeM = maxRangeM;

  /* Materials. A BLEND surface is composited against the unlit structure behind it HERE, because this
   * draw is opaque: one draw per unit is what keeps an empty registry free, and a second, sorted pass
   * for one canopy is not worth the pass it would cost. Consequence, stated: the cockpit tub behind the
   * glass is occluded by the glass instead of showing through. */
  lod.MatCount = (int)glb.Materials.size();
  if (lod.MatCount > kMaxUnitMaterials) lod.MatCount = kMaxUnitMaterials;
  for (int i = 0; i < lod.MatCount; i++) {
    const FBGlbMaterial &m = glb.Materials[(size_t)i];
    const float a = m.Blend ? m.BaseColor[3] : 1.0f;
    for (int k = 0; k < 3; k++) lod.MatColor[i][k] = m.BaseColor[k] * a;
    lod.MatColor[i][3] = m.BaseColorTex >= 0 ? 1.0f : 0.0f;
    if (m.BaseColorTex >= 0 && !lod.TexW) {
      const FBGlbImage &im = glb.Images[(size_t)m.BaseColorTex];
      if (im.W > 0 && im.W == im.H) {
        lod.TexW = im.W;
        lod.TexRgba = im.Rgba;
      }
    }
  }

  /* Part 0 is the static airframe; its base is the identity because every static mesh is baked into
   * model space outright. */
  lod.Parts.push_back(Part{});

  float nzMin[3] = {1e30f, 1e30f, 1e30f}, nzMax[3] = {-1e30f, -1e30f, -1e30f};

  struct Visit { int Node; float ToPart[12]; int Part; };
  const float kIdent[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
  std::vector<Visit> stack;
  for (int r : glb.Roots) {
    Visit v{r, {}, 0};
    std::memcpy(v.ToPart, kIdent, sizeof kIdent);
    stack.push_back(v);
  }

  while (!stack.empty()) {
    const Visit v = stack.back();
    stack.pop_back();
    if (v.Node < 0 || (size_t)v.Node >= glb.Nodes.size()) continue;
    const FBGlbNode &n = glb.Nodes[(size_t)v.Node];

    const float noScale[3] = {1.0f, 1.0f, 1.0f};
    float tr[12], sc[12] = {n.S[0], 0, 0, 0, 0, n.S[1], 0, 0, 0, 0, n.S[2], 0};
    TrsToMat(n.T, n.R, noScale, tr);

    const Component *comp = FindComponent(n.Name);
    float self[12];
    int part = v.Part;
    if (comp && (int)lod.Parts.size() < kMaxUnitParts) {
      Part p;
      p.Node = n.Name;
      p.Ch = comp->Ch;
      p.LoDeg = comp->LoDeg;
      p.HiDeg = comp->HiDeg;
      p.Inverted = comp->Inverted;
      p.Parent = v.Part;
      /* The hinge frame: everything above it, then this node's own placement. The rotation the frame
       * carries IS the hinge axis — the sidecar states "lokal X" for every component. */
      Mul34(v.ToPart, tr, p.Base);
      part = (int)lod.Parts.size();
      lod.Parts.push_back(p);
      std::memcpy(self, sc, sizeof sc);   /* below the hinge only the node's own scale remains */
    } else {
      float trs[12];
      Mul34(tr, sc, trs);
      Mul34(v.ToPart, trs, self);
    }

    if (n.Mesh >= 0 && (size_t)n.Mesh < glb.MeshFirstPrim.size()) {
      const bool isNozzle = n.Name.compare(0, 6, "nozzle") == 0;
      const uint32_t first = glb.MeshFirstPrim[(size_t)n.Mesh], cnt = glb.MeshPrimCount[(size_t)n.Mesh];
      for (uint32_t pi = first; pi < first + cnt; pi++) {
        const FBGlbPrimitive &prim = glb.Prims[pi];
        int mat = prim.Material;
        if (mat < 0 || mat >= lod.MatCount) mat = 0;
        const uint32_t tag = (uint32_t)part | ((uint32_t)mat << 16);
        /* The primitive's vertex RANGE is copied once and its indices are rebased onto it; a mesh used
         * by two nodes would be re-emitted, which in this asset never happens (one node per mesh). */
        const uint32_t base = (uint32_t)lod.Verts.size();
        for (uint32_t k = 0; k < prim.VertexCount; k++) {
          const uint32_t src = prim.VertexFirst + k;
          FBUnitVertex vx{};
          const float *sp = &glb.Pos[(size_t)src * 3];
          const float *sn = &glb.Nrm[(size_t)src * 3];
          for (int r = 0; r < 3; r++) {
            vx.P[r] = self[r * 4 + 0] * sp[0] + self[r * 4 + 1] * sp[1] + self[r * 4 + 2] * sp[2] + self[r * 4 + 3];
            vx.N[r] = self[r * 4 + 0] * sn[0] + self[r * 4 + 1] * sn[1] + self[r * 4 + 2] * sn[2];
          }
          vx.Uv[0] = glb.Uv[(size_t)src * 2];
          vx.Uv[1] = glb.Uv[(size_t)src * 2 + 1];
          vx.Tag = tag;
          if (isNozzle)
            for (int r = 0; r < 3; r++) {
              if (vx.P[r] < nzMin[r]) nzMin[r] = vx.P[r];
              if (vx.P[r] > nzMax[r]) nzMax[r] = vx.P[r];
            }
          lod.Verts.push_back(vx);
        }
        for (uint32_t k = 0; k < prim.Count; k++)
          lod.Idx.push_back(base + (glb.Idx[prim.First + k] - prim.VertexFirst));
      }
    }

    for (uint32_t c = 0; c < n.ChildCount; c++) {
      Visit cv{glb.Children[n.FirstChild + c], {}, part};
      std::memcpy(cv.ToPart, self, sizeof self);
      stack.push_back(cv);
    }
  }

  /* The exhaust plane, and the aft face is the one that matters: the plume starts where the gas leaves.
   * A level that declares no such node simply has no nozzle and draws no flame. */
  if (nzMax[2] > nzMin[2]) {
    lod.HasNozzle = true;
    lod.NozzleOff[0] = 0.5f * (nzMin[0] + nzMax[0]);
    lod.NozzleOff[1] = 0.5f * (nzMin[1] + nzMax[1]);
    lod.NozzleOff[2] = nzMax[2];
    lod.NozzleRadiusM = 0.25f * ((nzMax[0] - nzMin[0]) + (nzMax[1] - nzMin[1]));
  }

  lod.Triangles = (int)(lod.Idx.size() / 3);
  Lods_.push_back(std::move(lod));
  return true;
}

} // namespace FlightBox::Render
