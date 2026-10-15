






#include "base/spatial/ClassSeedValues.h"

struct GroundGrid {
  uint W, H, CellsAt, SeedsAt, RefsAt, EdgesAt;
  float OrgE, OrgN, CellM, BoundaryHaloM;
};

GroundGrid groundGridAt(uint head) {
  GroundGrid g;
  g.W = groundClasses[head + 0];
  g.H = groundClasses[head + 1];
  g.OrgE = uintBitsToFloat(groundClasses[head + 2]);
  g.OrgN = uintBitsToFloat(groundClasses[head + 3]);
  g.CellM = uintBitsToFloat(groundClasses[head + 4]);
  g.CellsAt = groundClasses[head + 5];
  g.SeedsAt = groundClasses[head + 6];
  g.RefsAt = groundClasses[head + 7];
  g.EdgesAt = groundClasses[head + 8];
  g.BoundaryHaloM = uintBitsToFloat(groundClasses[head + 9]);
  return g;
}

int groundCrossX(vec2 a, vec2 b, float cy, float xa, float xb) {
  if ((a.y <= cy) == (b.y <= cy)) { return 0; }
  const float xi = a.x + (cy - a.y) * (b.x - a.x) / (b.y - a.y);
  if (xi < xa || xi >= xb) { return 0; }
  return b.y > a.y ? 1 : -1;
}

int groundCrossY(vec2 a, vec2 b, float cx, float ya, float yb) {
  if ((a.x <= cx) == (b.x <= cx)) { return 0; }
  const float yi = a.y + (cx - a.x) * (b.y - a.y) / (b.x - a.x);
  if (yi <= ya || yi > yb) { return 0; }
  return b.x > a.x ? -1 : 1;
}

float groundSegDist(vec2 p, vec2 a, vec2 b) {
  const vec2 d = b - a;
  const float l2 = dot(d, d);
  const float t = l2 > 0.0 ? clamp(dot(p - a, d) / l2, 0.0, 1.0) : 0.0;
  return length(a + t * d - p);
}

const float kGroundNoEdgeM = 1.0e30;




int groundMaterialAt(
                                vec2 at,
                                float halfFootprintM,
                                out float edgeM,
                                out int runnerUp,
                                out float haloM) {
  int best = -1;
  int bestRank = -1;
  int second = -1;
  int secondRank = -1;
  edgeM = kGroundNoEdgeM;
  runnerUp = -1;
  haloM = 0.0;
  for (uint b = 0; b < 2u; ++b) {
    const uint head = groundClasses[b];
    if (head == 0u) { continue; }
    const GroundGrid g = groundGridAt(head);
    if (g.BoundaryHaloM > 0.0 && halfFootprintM > 0.5 * g.CellM) { continue; }
    const int i = int(floor((at.x - g.OrgE) / g.CellM));
    const int j = int(floor((at.y - g.OrgN) / g.CellM));
    if (i < 0 || j < 0 || uint(i) >= g.W || uint(j) >= g.H) { continue; }
    const uint ci = g.CellsAt + (uint(j) * g.W + uint(i)) * 2u;
    const uint c0 = groundClasses[ci];
    const uint seeds = (c0 >> 16) & 0xFFu;
    const uint seedFirst = groundClasses[ci + 1u];
    if ((c0 & 0xFFu) != 0xFFu) {
      best = int(c0 & 0xFFu);
      bestRank = int((c0 >> 8) & 0xFFu);
    }
    const vec2 corner = vec2(g.OrgE + float(i) * g.CellM, g.OrgN + float(j) * g.CellM);
    int edgeClass = -1;
    int edgeRank = -1;
    float signedEdgeM = kGroundNoEdgeM;
    bool edgeInside = false;
    for (uint s = 0; s < seeds; ++s) {
      const uint seedAt = g.SeedsAt + (seedFirst + s) * uint(kClassSeedWordCount);
      const uint w0 = groundClasses[seedAt];
      const uint refFirst = groundClasses[seedAt + 1u];
      const uint refs = groundClasses[seedAt + 2u];
      const float halfW = uintBitsToFloat(groundClasses[seedAt + 3u]);
      const int tpl = int(w0 & 0xFFu);
      const int rank = int((w0 >> 8) & 0xFFu);
      int wind = int((w0 >> 24) & 0xFFu) - 128;
      float d = kGroundNoEdgeM;
      for (uint r = 0; r < refs; ++r) {
        const uint edge = g.EdgesAt + groundClasses[g.RefsAt + refFirst + r] * 4u;
        const vec2 p0 = vec2(uintBitsToFloat(groundClasses[edge]), uintBitsToFloat(groundClasses[edge + 1u]));
        const vec2 p1 =
            vec2(uintBitsToFloat(groundClasses[edge + 2u]), uintBitsToFloat(groundClasses[edge + 3u]));
        if (halfW <= 0.0) {
          wind += groundCrossX(p0, p1, corner.y, corner.x, at.x);
          wind += groundCrossY(p0, p1, at.x, corner.y, at.y);
        }
        d = min(d, groundSegDist(at, p0, p1));
      }
      bool inside = halfW > 0.0 ? d <= halfW : wind != 0;
      float signedDistance = halfW > 0.0 ? halfW - d : (inside ? d : -d);
      if (g.BoundaryHaloM > 0.0 && abs(signedDistance) <= g.BoundaryHaloM &&
          (rank > edgeRank || (rank == edgeRank && abs(signedDistance) < abs(signedEdgeM)))) {
        edgeClass = tpl;
        edgeRank = rank;
        signedEdgeM = signedDistance;
        edgeInside = inside;
      }
      if (!inside) { continue; }
      if (rank > bestRank) {
        second = best;
        secondRank = bestRank;
        best = tpl;
        bestRank = rank;
      } else if (rank > secondRank) {
        second = tpl;
        secondRank = rank;
      }
    }
    if (edgeRank >= 0 && edgeRank >= bestRank) {
      runnerUp = edgeInside ? second : best;
      if (runnerUp == edgeClass) { return best; }
      edgeM = signedEdgeM;
      haloM = g.BoundaryHaloM;
      return edgeClass;
    }
    if (best >= 0) { return best; }
  }
  return best;
}
