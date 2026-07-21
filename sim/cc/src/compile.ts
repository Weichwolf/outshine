// Task -> iNav compiler (FORMAT.md §5/§6). Each typed task emits its WAYPOINT/HOLD_TIME/JUMP/LAND/RTH
// sequence within ONE shared ≤15-waypoint pool (NAV_MAX_WAYPOINTS). The route iNav flies is ≤15 WPs; JUMP
// loops reuse WPs so orbits/patrols cost few slots, but dense area patterns are budget-bound — overflow
// reduces lane count and LOGS the truncation, or fails the compile with the reason (never a silent clip).

const D2R = Math.PI / 180;
export const NAV = { WAYPOINT: 1, HOLD_TIME: 3, RTH: 4, JUMP: 6, LAND: 8 } as const;
export const NAV_MAX_WAYPOINTS = 15;

export interface INavWp { lat: number; lon: number; altRel: number; action: number; p1: number; p2: number; p3: number; }
export interface CompileResult { wps: INavWp[]; truncations: string[]; }

const wp = (lat: number, lon: number, altRel: number, action: number = NAV.WAYPOINT, p1 = 0, p2 = 0, p3 = 0): INavWp =>
  ({ lat, lon, altRel, action, p1, p2, p3 });

function dest(lat: number, lon: number, brg: number, dist: number): [number, number] {
  const br = brg * D2R, clat = Math.cos(lat * D2R);
  return [lat + (Math.cos(br) * dist) / 111320, lon + (Math.sin(br) * dist) / (111320 * clat)];
}

/** Resolved task geometry the compiler consumes. altRel is home-relative metres (resolved upstream). */
export interface ResolvedTask {
  type: string; id?: string;
  points?: { lat: number; lon: number; altRel: number }[];     // p2p / generated-pattern legs
  center?: { lat: number; lon: number; altRel: number };       // orbit/racetrack/target
  radiusM?: number; orbitDir?: 'cw' | 'ccw'; dwellS?: number; minLaps?: number;
  poly?: { lat: number; lon: number }[]; laneSpacingM?: number; altRel?: number; sensorSwathM?: number;
  axisHdg?: number; lengthM?: number;                          // racetrack
}

/** Compile one task into iNav WPs, appending to the running pool. Returns the slots it used + truncations. */
function compileTask(t: ResolvedTask, budgetLeft: number): { wps: INavWp[]; truncations: string[] } {
  const tr: string[] = [];
  switch (t.type) {
    case 'p2p': case 'transit': case 'egress': case 'strike_pass': case 'tf_ingress':
      return { wps: (t.points ?? []).map((p) => wp(p.lat, p.lon, p.altRel)), truncations: tr };

    case 'overwatch': case 'relay_station': {
      // orbit entry WP at the standoff radius + HOLD_TIME loiter (native loiter, direction = global setting)
      const c = t.center!; const [la, lo] = dest(c.lat, c.lon, 0, t.radiusM ?? 400);
      return { wps: [wp(la, lo, c.altRel, NAV.HOLD_TIME, Math.round(t.dwellS ?? 120))], truncations: tr };
    }

    case 'cap_patrol': case 'perimeter': {
      // racetrack as 4 WPs + JUMP loop back to the first (count = laps-1); orbit_dir from WP ordering
      const c = t.center!; const half = (t.lengthM ?? 1200) / 2, r = t.radiusM ?? 300, ax = t.axisHdg ?? 0;
      const dir = t.orbitDir === 'ccw' ? -1 : 1;
      const [a1, o1] = dest(c.lat, c.lon, ax, half), [a2, o2] = dest(c.lat, c.lon, ax, -half);
      const [b1, p1] = dest(a1, o1, ax + 90 * dir, r), [b2, p2] = dest(a2, o2, ax + 90 * dir, r);
      const loops = (t.minLaps ?? 3) - 1;
      const pts = [wp(a1, o1, c.altRel), wp(b1, p1, c.altRel), wp(b2, p2, c.altRel), wp(a2, o2, c.altRel)];
      pts.push(wp(0, 0, 0, NAV.JUMP, 0 /* target = first WP of this task, patched by caller */, loops));
      return { wps: pts, truncations: tr };
    }

    case 'recon_area': case 'area_search': case 'sar_ladder': {
      // boustrophedon lanes across the polygon bbox; lanes bounded by the WP budget (§6 overflow rule)
      const poly = t.poly ?? []; const alt = t.altRel ?? 100; const spacing = t.laneSpacingM ?? (t.sensorSwathM ?? 300);
      let minLa = Infinity, maxLa = -Infinity, minLo = Infinity, maxLo = -Infinity;
      for (const p of poly) { minLa = Math.min(minLa, p.lat); maxLa = Math.max(maxLa, p.lat); minLo = Math.min(minLo, p.lon); maxLo = Math.max(maxLo, p.lon); }
      const clat = Math.cos(((minLa + maxLa) / 2) * D2R);
      const widthM = (maxLo - minLo) * 111320 * clat, heightM = (maxLa - minLa) * 111320;
      let lanes = Math.max(2, Math.ceil(heightM / spacing));
      const maxLanes = Math.max(2, budgetLeft - 1);           // 2 WP/lane end; keep 1 slot for LAND downstream
      if (lanes > maxLanes) { tr.push(`recon: ${lanes} lanes -> ${maxLanes} (WP budget); coverage will be lower`); lanes = maxLanes; }
      const wps: INavWp[] = []; const dLa = (maxLa - minLa) / (lanes - 1);
      for (let i = 0; i < lanes; i++) {
        const la = minLa + i * dLa; const leftFirst = i % 2 === 0;
        wps.push(wp(la, leftFirst ? minLo : maxLo, alt));
        wps.push(wp(la, leftFirst ? maxLo : minLo, alt));
      }
      void widthM;
      return { wps, truncations: tr };
    }

    case 'intercept': {
      const c = t.center!; return { wps: [wp(c.lat, c.lon, c.altRel)], truncations: tr };
    }
    default:
      tr.push(`unknown task type ${t.type} -> skipped`);
      return { wps: [], truncations: tr };
  }
}

/** Compile the whole phased mission to the shared iNav WP pool. transitWps/egressWps are pre-resolved legs. */
export function compileMission(opts: {
  transit: { lat: number; lon: number; altRel: number }[];
  tasks: ResolvedTask[];
  egress: { lat: number; lon: number; altRel: number }[];
  land?: { lat: number; lon: number; altRel: number };
}): CompileResult {
  const wps: INavWp[] = []; const truncations: string[] = [];
  for (const p of opts.transit) wps.push(wp(p.lat, p.lon, p.altRel));
  for (const t of opts.tasks) {
    const taskStart = wps.length;
    const r = compileTask(t, NAV_MAX_WAYPOINTS - wps.length - (opts.land ? 1 : 0));
    // patch a task-local JUMP target (loop back to this task's first WP)
    for (const w of r.wps) if (w.action === NAV.JUMP && w.p1 === 0) w.p1 = taskStart;
    wps.push(...r.wps); truncations.push(...r.truncations);
  }
  for (const p of opts.egress) wps.push(wp(p.lat, p.lon, p.altRel));
  if (opts.land) wps.push(wp(opts.land.lat, opts.land.lon, opts.land.altRel, NAV.LAND));
  if (wps.length > NAV_MAX_WAYPOINTS)
    throw new Error(`compile overflow: ${wps.length} WPs > ${NAV_MAX_WAYPOINTS} (reduce tasks/lanes or split the mission)`);
  return { wps, truncations };
}
