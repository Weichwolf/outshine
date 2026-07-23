/* SUPERSEDED IN PRODUCTION: flightsim.h now runs fb/FBAutopilot + fb/FBFlightControl (C++ port of
 * this file, numerics equivalence-tested). As of this writing nothing else includes flightctl.h
 * either (validator/critic drive real iNav SITL, not this law) — kept on disk in case a native
 * harness still needs the plain-C form; delete once confirmed dead.
 *
 * FlightBox — thin fly-by-wire + small autopilot (no iNav). Pure C; runs in the CC (WASM + native).
 * Takes the JSBSim state (fb_fdm_state) and emits normalized surface/throttle commands for
 * fb_jsbsim_set_controls. Two layers:
 *   FBW inner  — attitude hold: bank/pitch command -> aileron/elevator (PD, rate-damped), yaw damper
 *                + turn coordination, airspeed hold -> throttle.
 *   Autopilot  — modes: MANUAL (pass-through stick) and LOITER (bank-to-circle around a geo center at
 *                a target altitude and radius). The loiter is the camera platform for worldwide shots.
 *
 * Surface sign conventions are per-axis constants (FC_SGN_*) so a model whose flight_control maps a
 * control the other way is one flip, not a hunt. Verified against the c172 in the smoke harness.
 */
#ifndef FB_FLIGHTCTL_H
#define FB_FLIGHTCTL_H
#include <math.h>
#include "jsbsim_adapter.h"

#define FC_G      9.80665
#define FC_R2D    57.29577951308232
#define FC_MPD    111320.0            /* metres per degree latitude (spherical approx) */

/* control-surface sign: +1 if positive fcs cmd moves the axis in the positive state direction */
#define FC_SGN_ROLL   (+1.0)         /* + aileron -> + roll (right) */
#define FC_SGN_PITCH  (-1.0)         /* raw-surface: + elevator -> nose DOWN in JSBSim convention -> flip */
#define FC_SGN_YAW    (+1.0)
#define FC_SGN_FLCS_ROLL   (+1.0)    /* FLCS: + aileron-cmd -> + roll rate (right) */
#define FC_SGN_FLCS_PITCH  (+1.0)    /* FLCS: + elevator-cmd -> pull (nose up / more g) */

typedef enum { FC_MANUAL = 0, FC_LOITER = 1 } fc_mode;

typedef struct {
  fc_mode mode;
  /* loiter target */
  double lat, lon;        /* circle centre (deg) */
  double alt;             /* target altitude (m ASL) */
  double radius;          /* circle radius (m) */
  int    dir;             /* +1 clockwise, -1 counter-clockwise */
  double spd;             /* target true airspeed (m/s) */
  /* manual stick (used in FC_MANUAL), each -1..1 / 0..1 */
  double m_roll, m_pitch, m_yaw, m_thr;
  /* limits */
  double bank_max, pitch_max;   /* deg */
  /* inner-loop kind: 1 = command a real FLCS (roll-rate + g stick inputs, e.g. the F-16 — the FLCS is
   * the stabilizer); 0 = raw-surface attitude-hold PD (models with no FLCS / fbw-override). */
  int flcs;
  /* gains (attitude-hold PD + loops); defaults via fc_defaults() */
  double kp_roll, kd_roll, kp_pitch, kd_pitch, kd_yaw, k_coord, thr_trim, kp_spd, k_hdg, k_alt;
  /* FLCS-command gains: bank error -> roll-rate stick; g-command PI + climb bias + alt->vs */
  double k_rollrate;     /* bank error (deg) -> roll-rate stick */
  double rollstick_max;  /* roll-stick clamp: a full-deflection roll-in makes the F-16's differential
                            stabilators pitch the nose down (nz dips to -2 g at NEUTRAL pitch stick) and
                            the FLCS g-PID winds up into a +3.4 g recovery overshoot. A pilot rolls in
                            smoothly; capping the commanded roll rate removes both spikes at the source. */
  double k_g, k_gi;      /* g-command error (g) -> pitch stick: proportional + integral (kills steady-state g error) */
  double g_iterm;        /* g-command integrator STATE (mutable across calls) */
  double nz_slew;        /* max commanded-g change per second (transient shaping, see below) */
  double nz_prev;        /* last slewed nzCmd STATE (0 = uninitialised -> seeded from current nz) */
  double k_vs2g;         /* vertical-speed error (m/s) -> commanded-g bias for altitude hold */
  double k_vsi;          /* vs-error integral -> commanded-g: collapses the false equilibria where a
                            standing alt error exactly cancels the turn-g feedforward (sim-critic run 1) */
  double vs_iterm;       /* vs-error integrator STATE */
  double k_ny, k_nyi;    /* lateral-g nulling -> pedals (P + integral): coordinates the turn when the
                            model's yaw damper holds steady rudder in a turn (no washout) and makes it slip */
  double ny_iterm;       /* pedal integrator STATE */
  double k_ti;           /* speed-error integral -> throttle (kills the standing 20 m/s speed error) */
  double thr_iterm;      /* throttle integrator STATE */
  double k_xt;           /* loiter cross-track: metres off the ring -> degrees of course correction */
  double k_xti;          /* cross-track integral (deg/s per m): closes the last few % of ring radius */
  double xt_iterm;       /* cross-track integrator STATE (deg) */
} fc_ctl;

typedef struct { double roll, pitch, yaw, thr; } fc_out;

static inline double fc_clamp(double v, double lo, double hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline double fc_wrap180(double a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

/* local NE offset (m) from (lat0,lon0) to (lat1,lon1), equirectangular (fine over loiter scale) */
static inline void fc_ll_to_ne(double lat0, double lon0, double lat1, double lon1, double *n, double *e) {
  *n = (lat1 - lat0) * FC_MPD;
  *e = fc_wrap180(lon1 - lon0) * FC_MPD * cos(lat0 * M_PI / 180.0);   /* Wrap180: antimeridian-safe */
}
/* point `dist` m from (lat0,lon0) on bearing `brg` (deg, 0=N,90=E) */
static inline void fc_ne_dest(double lat0, double lon0, double dist, double brg, double *lat, double *lon) {
  double b = brg * M_PI / 180.0;
  *lat = lat0 + (dist * cos(b)) / FC_MPD;
  *lon = lon0 + (dist * sin(b)) / (FC_MPD * cos(lat0 * M_PI / 180.0));
}

static inline void fc_defaults(fc_ctl *c) {
  c->mode = FC_LOITER; c->dir = 1; c->radius = 1200; c->spd = 30; c->alt = 0;
  c->bank_max = 35; c->pitch_max = 15;
  c->kp_roll = 0.022; c->kd_roll = 0.004;
  c->kp_pitch = 0.06; c->kd_pitch = 0.010;
  c->kd_yaw = 0.006; c->k_coord = 0.004;
  c->thr_trim = 0.55; c->kp_spd = 0.03;
  c->k_hdg = 0.6; c->k_alt = 0.05;
  c->m_thr = 0.55;
  c->flcs = 0;
  c->k_rollrate = 0.06; c->rollstick_max = 1.0;
  c->k_g = 0.25; c->k_gi = 0.8; c->g_iterm = 0.0;  /* g-command PI (soft: it wraps the FLCS's OWN g-PID) */
  c->nz_slew = 1.5; c->nz_prev = 0.0;
  c->k_vs2g = 0.05;       /* vertical-speed error (m/s) -> commanded-g bias */
  c->k_vsi = 0.02; c->vs_iterm = 0.0;
  c->k_ny = 1.5; c->k_nyi = 2.0; c->ny_iterm = 0.0;
  c->k_ti = 0.002; c->thr_iterm = 0.0;
  c->k_xt = 0.02;         /* metres off the ring -> degrees of course correction */
  c->k_xti = 0.0004; c->xt_iterm = 0.0;
}

/* Preset for the F-16 (or any airframe with a real FLCS): command the FLCS like a pilot. */
static inline void fc_f16(fc_ctl *c) {
  fc_defaults(c);
  c->flcs = 1;
  c->bank_max = 60; c->pitch_max = 30;          /* the FLCS/AoA-limiter enforces the real envelope */
  c->spd = 220; c->thr_trim = 0.85; c->kp_spd = 0.02;
  c->k_hdg = 0.8; c->k_alt = 0.08;              /* alt error -> target vertical speed (m/s) */
  c->k_rollrate = 0.05; c->rollstick_max = 0.15;   /* gentle pilot roll-in: nz stays 0.7..1.9 vs -1.1..+3.0 at 0.35 (measured) */ c->k_g = 0.25; c->k_gi = 0.8; c->g_iterm = 0.0; c->nz_slew = 1.5; c->nz_prev = 0.0; c->k_vs2g = 0.05;
  c->k_vsi = 0.02; c->vs_iterm = 0.0; c->k_ny = 1.5; c->k_nyi = 2.0; c->ny_iterm = 0.0; c->k_ti = 0.002; c->thr_iterm = 0.0;
  c->k_xt = 0.02; c->k_xti = 0.0004; c->xt_iterm = 0.0;   /* ring capture on the F-16's km-scale loiter */
}

/* Compute controls for this state (fixed 100 Hz sim step). `c` is mutable: the g-command integrator
 * state lives in it. `dbg` (optional) receives loiter diagnostics. */
static inline void fc_update(fc_ctl *c, const fb_fdm_state *s, fc_out *o, double *dbg_radius, double *dbg_bankcmd) {
  if (c->mode == FC_MANUAL) {
    o->roll = c->m_roll; o->pitch = c->m_pitch; o->yaw = c->m_yaw; o->thr = c->m_thr;
    if (dbg_radius) *dbg_radius = 0; if (dbg_bankcmd) *dbg_bankcmd = 0;
    return;
  }
  /* ---- LOITER outer: sliding-carrot bank-to-circle ---- */
  /* Tangent + feedforward-bank + cross-track capture. On the ring in steady state the cross-track term
   * is zero and the aircraft flies the tangent, so the heading error vanishes and bankCmd == the pure
   * feedforward bank for the turn — no double-count (an earlier carrot law summed a heading-to-carrot
   * bank AND the feedforward, over-banking ~12° so bank and load factor disagreed). */
  double n, e; fc_ll_to_ne(c->lat, c->lon, s->lat, s->lon, &n, &e);   /* centre -> aircraft */
  double d = hypot(n, e);
  double angPos = atan2(e, n) * FC_R2D;                                /* aircraft's angular position on the ring */
  double tangent = angPos + c->dir * 90.0;                             /* flight-path tangent to the ring here */
  double xtErr = d - c->radius;
  c->xt_iterm = fc_clamp(c->xt_iterm + c->k_xti * xtErr * 0.01, -15.0, 15.0);  /* slow integral: the P term alone
                            leaves a few % standing radius error (the ff bank under-turns because of the model's
                            residual slip, see below) */
  double xtCorr = c->dir * fc_clamp(c->k_xt * xtErr + c->xt_iterm, -50.0, 50.0);  /* steer inward if outside, outward if inside */
  double desCourse = tangent + xtCorr;
  double hdgErr = fc_wrap180(desCourse - s->yaw);
  double V = fmax(s->speed, 12.0);
  double ffBank = atan2(V * V, c->radius * FC_G) * FC_R2D * c->dir;    /* feedforward bank for the ring turn */
  double bankCmd = fc_clamp(ffBank + c->k_hdg * hdgErr, -c->bank_max, c->bank_max);
  double altErr = c->alt - s->elev;
  double pitchCmd = fc_clamp(c->k_alt * altErr, -c->pitch_max, c->pitch_max);

  /* ---- FBW inner ---- */
  if (c->flcs) {
    /* Command the real FLCS like a pilot: roll-rate stick from bank error, g stick from vertical-speed
     * error (neutral pitch = 1 g, so this biases the g the FLCS holds — incl. the extra g a bank needs
     * to stay level). yaw = 0: the FLCS coordinates the turn (ARI / yaw damper). The FLCS's own AoA/g
     * limiter caps the envelope, so we can command freely and let it protect the jet. */
    /* g-command = FEEDFORWARD the load factor a coordinated level turn needs at this bank (nz=1/cos φ),
     * plus an altitude-hold bias (climb/descent -> extra/less g), driven by a g-error feedback so the
     * FLCS actually delivers it. Without the 1/cos(φ) feedforward the jet flies chronically under-g'd in
     * the turn and never closes the loiter ring (sim-critic run 1). */
    /* Turn-g feedforward from the ACTUAL bank, not the target: during the roll-in the wings are not
     * yet there, and commanding the final turn's g early winds up BOTH integrators (ours and the
     * FLCS's own g-PID with its 0.3 s actuator lag) -> a 3.8 g entry spike ringing to near-0 g
     * (sim-critic run 1 v2). The g a turn needs NOW depends on the bank NOW. */
    double bc = fmin(fabs(s->roll), 80.0) * (M_PI / 180.0);
    double targetVs = fc_clamp(c->k_alt * altErr, -50.0, 50.0);    /* alt error -> desired climb rate (m/s) */
    double vsErr = targetVs - s->vy;
    /* vs-error INTEGRAL: without it any standing alt error whose g-bias exactly cancels the 1/cos
     * feedforward is a stable (wrong) equilibrium — the jet parked 60 m high, 500 m wide, under-g'd.
     * Integrating vs error forces vy -> targetVs, which forces alt -> target (sim-critic run 1). */
    c->vs_iterm = fc_clamp(c->vs_iterm + c->k_vsi * vsErr * 0.01, -0.5, 0.5);
    double nzRaw = 1.0 / cos(bc) + c->k_vs2g * vsErr + c->vs_iterm; /* turn g + climb/descent g bias */
    /* slew-limit the g command (bare-model yardstick: a mild single overshoot, no ringing) */
    if (c->nz_prev == 0.0) c->nz_prev = s->nz;
    double step = c->nz_slew * 0.01;
    double nzCmd = fc_clamp(nzRaw, c->nz_prev - step, c->nz_prev + step);
    c->nz_prev = nzCmd;
    double gErr = nzCmd - s->nz;
    /* Blend the g stick in only once the bank is (nearly) established — a pilot rolls in with neutral
     * pitch and lets the FLCS hold ~1 g, then pulls. Driving the g loop mid-roll stacks onto the
     * FLCS's own g-PID (steep stick->g gain + 0.3 s actuator lag) -> 3-4 g entry spikes with a
     * negative-g ring (sim-critic run 1 v2). Integrators freeze while blended out (anti-windup). */
    double bankErr = fabs(bankCmd - s->roll);
    double blend = fc_clamp(1.0 - (bankErr - 5.0) / 15.0, 0.0, 1.0);   /* 1 below 5 deg err, 0 above 20 deg */
    c->g_iterm = fc_clamp(c->g_iterm + blend * c->k_gi * gErr * 0.01, -1.0, 1.0);
    o->roll  = FC_SGN_FLCS_ROLL  * fc_clamp(c->k_rollrate * (bankCmd - s->roll), -c->rollstick_max, c->rollstick_max);
    o->pitch = FC_SGN_FLCS_PITCH * (blend * fc_clamp(c->k_g * gErr + c->g_iterm, -1, 1));
    /* pedals: null the lateral load factor (what a pilot does when the ball is out). The model's yaw
     * FLCS feeds RAW yaw rate back to the rudder (gain 100 at cruise Vg, NO washout, f16.xml Yaw channel)
     * so a steady turn holds steady rudder against the turn: uncorrected beta ~2 deg / ny ~ -0.25 g, the
     * force tilt lags Euler roll ~12 deg, the ring never closes. Pedal (P+I) cancels what it can; the
     * rate term (~ +2.7 at loiter r) exceeds the +-1 cmd clip, so ny ~ -0.10 REMAINS - model-intrinsic,
     * not a law bug. The flown dynamics stay force-coherent: w = g*tan(phi_eff)/V with
     * phi_eff = phi - atan(|ny|/nz), verified to ~1%. */
    c->ny_iterm = fc_clamp(c->ny_iterm + c->k_nyi * s->ny * 0.01, -0.6, 0.6);
    o->yaw   = fc_clamp(c->k_ny * s->ny + c->ny_iterm, -1, 1);
  } else {
    o->roll  = FC_SGN_ROLL  * fc_clamp(c->kp_roll  * (bankCmd  - s->roll)  - c->kd_roll  * s->p, -1, 1);
    o->pitch = FC_SGN_PITCH * fc_clamp(c->kp_pitch * (pitchCmd - s->pitch) - c->kd_pitch * s->q, -1, 1);
    o->yaw   = FC_SGN_YAW   * fc_clamp(-c->kd_yaw * s->r + c->k_coord * bankCmd, -1, 1);
  }
  double spdErr = c->spd - s->speed;
  c->thr_iterm = fc_clamp(c->thr_iterm + c->k_ti * spdErr * 0.01, -c->thr_trim, 1.0 - c->thr_trim);   /* anti-windup bound = the PHYSICAL throttle range from trim ([0,1]), not an arbitrary constant — a fixed +-0.4 pinned at the F-16's low loiter throttle and left +2.9 m/s standing (sim-critic) */
  o->thr   = fc_clamp(c->thr_trim + c->kp_spd * spdErr + c->thr_iterm, 0, 1);
  if (dbg_radius) *dbg_radius = d; if (dbg_bankcmd) *dbg_bankcmd = bankCmd;
}
#endif /* FB_FLIGHTCTL_H */
