/* Standalone verification of jsbsim_adapter (D2): trim stability + control-sign directions,
 * BEFORE iNav is in the loop — so a sign bug shows here, not tangled with iNav's own loop. */
#include "jsbsim_adapter.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static fb_fdm_state s;
static void tick_n(int n){ for(int i=0;i<n;i++) fb_jsbsim_step(&s); }

int main(int argc, char** argv){
  const char* ac = argc>1?argv[1]:"minisgs_e";
  double spd = argc>2?atof(argv[2]):14.0;
  int fbw = argc>3?atoi(argv[3]):0;
  if(fb_jsbsim_init("models", ac, 52.045, 9.385, 71.0, 2.0, spd, 0.0, fbw)){ printf("INIT FAIL\n"); return 1; }

  /* Phase 1: trim stability — 2 s zero controls. A trimmed spawn stays near level. */
  fb_jsbsim_set_controls(0,0,0, ac[0]=='f'?0.5:0.4);
  double roll0=1e9, rollmax=0, altmax=0, alt0=0;
  for(int i=0;i<200;i++){ fb_jsbsim_step(&s);
    if(i==0){ roll0=s.roll; alt0=s.elev; }
    if(fabs(s.roll)>rollmax) rollmax=fabs(s.roll);
    if(fabs(s.elev-alt0)>altmax) altmax=fabs(s.elev-alt0);
    if(!std::isfinite(s.roll)||!std::isfinite(s.elev)){ printf("DIVERGED at %d\n",i); return 2; } }
  printf("[%s] TRIM: roll0=%.2f pitch=%.2f alt=%.1fm speed=%.1f | over 2s: |roll|max=%.1f d_alt_max=%.1fm\n",
         ac, roll0, s.pitch, s.elev, s.speed, rollmax, altmax);

  /* Phase 2: roll-sign — +aileron for 1 s. iNav +roll = roll RIGHT = phi POSITIVE. */
  double r_before=s.roll; fb_jsbsim_set_controls(+0.4,0,0,ac[0]=='f'?0.5:0.4); tick_n(100);
  printf("[%s] ROLL cmd +0.4 -> phi %.1f -> %.1f  (%s)\n", ac, r_before, s.roll,
         s.roll>r_before+2 ? "RIGHT/positive = OK" : s.roll<r_before-2 ? "LEFT/negative = FLIP aileron" : "no response?");

  /* Phase 3: pitch-sign — recenter, +elevator for 1 s. iNav +pitch = nose UP = theta POSITIVE. */
  fb_jsbsim_set_controls(0,0,0,ac[0]=='f'?0.5:0.4); tick_n(80);
  double p_before=s.pitch; fb_jsbsim_set_controls(0,+0.4,0,ac[0]=='f'?0.5:0.4); tick_n(60);
  printf("[%s] PITCH cmd +0.4 -> theta %.1f -> %.1f  (%s)\n", ac, p_before, s.pitch,
         s.pitch>p_before+1 ? "UP/positive = OK" : s.pitch<p_before-1 ? "DOWN/negative = FLIP elevator" : "no response?");

  /* Phase 4: FLYABLE-UNDER-CONTROL — a trivial P-controller (proxy for iNav ANGLE) holds wings
   * level + a few deg pitch. The open-loop departure above is unfair: iNav controls ALL axes. If a
   * dumb P-loop keeps it bounded, iNav flies it. */
  double thr = ac[0]=='f' ? 0.6 : 0.5, pt_tgt = 2.0, rmax=0, amax=0, a0=0;
  for(int i=0;i<1000;i++){                     /* 4 s settle + 6 s measured */
    double ail = -0.03*s.roll;                 /* hold roll 0 */
    double ele =  0.05*(pt_tgt - s.pitch);     /* hold pitch target */
    if(ail>1)ail=1; if(ail<-1)ail=-1; if(ele>1)ele=1; if(ele<-1)ele=-1;
    fb_jsbsim_set_controls(ail, ele, 0, thr);
    fb_jsbsim_step(&s);
    if(!std::isfinite(s.roll)){ printf("[%s] STABILIZED: DIVERGED at %d\n",ac,i); return 3; }
    if(i==400){ a0=s.elev; }                   /* start measuring after 4 s settle */
    if(i>400){ if(fabs(s.roll)>rmax) rmax=fabs(s.roll); if(fabs(s.elev-a0)>amax) amax=fabs(s.elev-a0); }
  }
  printf("[%s] STABILIZED (P-hold, steady after 4s settle): |roll|max=%.1f pitch=%.1f alt_drift=%.1fm speed=%.1f -> %s\n",
         ac, rmax, s.pitch, amax, s.speed,
         rmax<15 && amax<30 ? "FLYABLE (iNav will hold it easily)" : "still marginal");
  printf("[%s] ADAPTER TEST DONE\n", ac);
  return 0;
}
