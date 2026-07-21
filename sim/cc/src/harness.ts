// Flight harness — the CC's command sequence, run against the LIVE hub. iNav flies natively; the CC only
// commands over the standard radio (RC + MSP mission), exactly as on real hardware. This drives one
// mission (arm -> altitude-gated ANGLE climb-out -> NAV WP -> optional RTH autoland) and streams progress
// so a human — or the WASM CC on :8080 — watches it fly in parallel. Verification lives in the test
// harness (declarative specs + ring scoring); this module is just "make it fly".

import { CC, sleep, type Telem } from './client.js';
import type { Mission } from './mission.js';

// RC channel map (matches the shared iNav aux config): [roll,pitch,thr,yaw, AUX1=ARM, AUX2=ANGLE,
// AUX3=NAV RTH, AUX4=NAV WP, AUX5=gear, AUX6=speedbrake].
const NEUTRAL = () => [1500, 1500, 1000, 1500, 1000, 1000, 1000, 1500, 1000, 1000];

export interface FlyOpts {
  angleHoldAltM?: number;      // climb wings-level in ANGLE to this AGL before handing to NAV (marginal airframes)
  climbPitch?: number;         // RC pitch during the ANGLE climb (>1500 = nose up)
  autoland?: boolean;          // after the last WP, RTH -> safehome FW autoland
  onTick?: (t: Telem, phase: string) => void;
}

/** Fly the resolved mission to completion (all WPs, then optional autoland) or until `timeoutMs`.
 *  Returns when the flight ends; never throws for a nav outcome — the caller's spec decides pass/fail. */
export async function flyMission(cc: CC, m: Mission, timeoutMs: number, opts: FlyOpts = {}): Promise<void> {
  const angleHold = opts.angleHoldAltM ?? 0;
  const climbPitch = opts.climbPitch ?? 1500;
  const land = opts.autoland ? {
    lat: m.land.lat, lon: m.land.lon, heading: m.land.headingDeg,
    approachAlt: 120, landAlt: m.land.elevM - m.takeoff.elevM,
  } : undefined;

  const t0 = Date.now();
  let calDone = false, uploaded = false, established = false, landing = false, phase = 'boot';
  while (Date.now() - t0 < timeoutMs) {
    const t = cc.t;
    const rc = NEUTRAL();

    if (!calDone) {
      if (t.calibrating === false && Date.now() - t0 > 500) calDone = true;   // wait out iNav sensor cal
      phase = 'calibrating';
    } else if (!t.armed) {
      if ((Date.now() - t0) % 3000 >= 1500) { rc[4] = 2000; rc[3] = 2000; }   // pulse ARM + YAW_HI (bypass NAV_UNSAFE)
      phase = 'arming';
    } else {
      if (!uploaded) { cc.uploadMission(m.waypoints.map((w) => ({ lat: w.lat, lon: w.lon, altRel: w.altRel ?? 0 })), land); uploaded = true; }
      const agl = t.aglM ?? 0;
      if (agl >= angleHold) established = true;                               // latch: once safe, stay in NAV
      if (angleHold > 0 && !established) {
        rc[4] = 2000; rc[5] = 2000; rc[2] = 1600; rc[1] = climbPitch;         // wings-level ANGLE climb-out
        phase = 'climb';
      } else {
        if (land && (t.navWp ?? 0) >= m.waypoints.length && (t.navState === 3 || t.navState === 4)) landing = true;
        if (landing) { rc[4] = 2000; rc[6] = 2000; phase = 'autoland'; }      // ARM + NAV RTH -> safehome autoland
        else { rc[4] = 2000; rc[7] = 2000; phase = t.navState === 5 ? 'enroute' : 'nav'; }
      }
    }

    cc.rc(rc);
    opts.onTick?.(t, phase);
    await sleep(33);                                                          // ~30 Hz RC (iNav fails safe without it)
  }
}
