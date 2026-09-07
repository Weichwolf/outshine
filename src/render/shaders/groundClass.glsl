






struct GroundGrid {
  uint W, H, CellsAt, SeedsAt, RefsAt, EdgesAt;
  float OrgE, OrgN, CellM;
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
  if (yi < ya || yi >= yb) { return 0; }
  return b.x > a.x ? -1 : 1;
}

float groundSegDist(vec2 p, vec2 a, vec2 b) {
  const vec2 d = b - a;
  const float l2 = dot(d, d);
  const float t = l2 > 0.0 ? clamp(dot(p - a, d) / l2, 0.0, 1.0) : 0.0;
  return length(a + t * d - p);
}

const float kGroundNoEdgeM = 1.0e30;




int groundClassAt(
                                vec2 at,
                                out float edgeM,
                                out int runnerUp) {
  int best = -1;
  int bestRank = -1;
  int second = -1;
  int secondRank = -1;
  edgeM = kGroundNoEdgeM;
  runnerUp = -1;
  for (uint b = 0; b < 2u; ++b) {
    const uint head = groundClasses[b];
    if (head == 0u) { continue; }
    const GroundGrid g = groundGridAt(head);
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
    for (uint s = 0; s < seeds; ++s) {
      const uint seedAt = g.SeedsAt + (seedFirst + s) * 3u;
      const uint w0 = groundClasses[seedAt];
      const uint refFirst = groundClasses[seedAt + 1u];
      const float halfW = uintBitsToFloat(groundClasses[seedAt + 2u]);
      const int tpl = int(w0 & 0xFFu);
      const int rank = int((w0 >> 8) & 0xFFu);
      const uint refs = (w0 >> 16) & 0xFFu;
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
      if (halfW > 0.0) {
        if (d > halfW) { continue; }
        d = halfW - d;
      } else if (wind == 0) {
        continue;
      }
      if (rank > bestRank) {
        second = best;
        secondRank = bestRank;
        best = tpl;
        bestRank = rank;
        edgeM = d;
      } else if (rank > secondRank) {
        second = tpl;
        secondRank = rank;
      }
    }
    if (best >= 0) { break; }
  }
  runnerUp = second;
  return best;
}

vec4 groundWears(int which, uint rows) {
  const uint row = which >= 0 && uint(which) < rows ? uint(which) : rows;
  return vec4(groundPalette[4u + row * 4u + 0u],
                groundPalette[4u + row * 4u + 1u],
                groundPalette[4u + row * 4u + 2u],
                groundPalette[4u + row * 4u + 3u]);
}


vec4 groundWearsSlope(int which, uint rows, float slopeDeg) {
  const uint row = which >= 0 && uint(which) < rows ? uint(which) : rows;
  const float limitDeg = groundPalette[4u * (rows + 2u) + row];
  const float bandDeg = groundPalette[2];
  const float bare = smoothstep(limitDeg, limitDeg + bandDeg, slopeDeg);
  const int rock = int(floatBitsToUint(groundPalette[1]));
  return mix(groundWears(which, rows), groundWears(rock, rows), bare);
}
