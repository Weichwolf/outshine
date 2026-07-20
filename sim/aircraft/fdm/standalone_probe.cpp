/* Standalone FDM stability probe: init the aircraft at its spawn IC, step JSBSim open-loop with
 * a fixed control input, print the trajectory. Isolates the FDM/model from iNav — if it tumbles
 * here with neutral surfaces, the model/spawn is the fault, not the autopilot. NOT shipped. */
#include "FGFDMExec.h"
#include "jsbsim_adapter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern JSBSim::FGFDMExec *fb_dbg_fdm();   /* probe-only accessor, defined in adapter under FB_PROBE */

int main(int argc, char **argv) {
  const char *ac  = argc > 1 ? argv[1] : "minisgs_e";
  double ground   = argc > 2 ? atof(argv[2]) : 398.59;
  double hoff     = argc > 3 ? atof(argv[3]) : 2.0;
  double spd      = argc > 4 ? atof(argv[4]) : 14.0;
  double thr      = argc > 5 ? atof(argv[5]) : 0.5;
  int    steps    = argc > 6 ? atoi(argv[6]) : 500;
  if (fb_jsbsim_init("/app/models", ac, 47.666, 9.498, ground, hoff, spd, 0.0, 0) != 0) {
    fprintf(stderr, "init failed\n"); return 1;
  }
  fb_fdm_state st;
  fb_jsbsim_set_ground(ground);
  int every = getenv("EVERY") ? atoi(getenv("EVERY")) : 10;
  int ramp  = getenv("RAMP") ? atoi(getenv("RAMP")) : 0;   /* steps to ramp throttle 0->thr */
  JSBSim::FGFDMExec *fdm = fb_dbg_fdm();
  printf("# ac=%s ground=%.2f hoff=%.1f spawn_spd=%.1f thr=%.2f ramp=%d\n", ac, ground, hoff, spd, thr, ramp);
  printf("# t  speed  roll  pitch |  rpm    thrust  engTq  Lmoment\n");
  for (int i = 0; i < steps; i++) {
    double tc = ramp > 0 && i < ramp ? thr * (double)i / ramp : thr;
    double inR = getenv("INR") ? atof(getenv("INR")) : 0.0;   /* fixed roll (aileron) cmd, iNav sign */
    double inP = getenv("INP") ? atof(getenv("INP")) : 0.0;
    double inY = getenv("INY") ? atof(getenv("INY")) : 0.0;
    fb_jsbsim_set_controls(inR, inP, inY, tc);
    fb_jsbsim_step(&st);
    if (i % every == 0) {
      double rpm = fdm->GetPropertyValue("propulsion/engine[0]/propeller-rpm");
      double thr_lbs = fdm->GetPropertyValue("propulsion/engine[0]/thrust-lbs");
      double etq = fdm->GetPropertyValue("propulsion/engine[0]/torque-ftlb");
      double Lm  = fdm->GetPropertyValue("moments/l-total-lbsft");
      double Nm = fdm->GetPropertyValue("moments/n-total-lbsft");
      double beta = fdm->GetPropertyValue("aero/beta-deg");
      printf("%5.2f spd=%5.2f phi=%6.1f psi=%6.1f r=%6.1f beta=%5.1f | Lmom=%7.2f Nmom=%7.2f\n",
             i * 0.01, st.speed, st.roll, st.yaw, st.r, beta, Lm, Nm);
    }
  }
  return 0;
}
