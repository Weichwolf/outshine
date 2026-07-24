/* libfbclock — LD_PRELOAD virtual clock. Accelerates an UNMODIFIED binary by a
 * fixed factor: monotonic time runs FB_TIME_SCALE x faster, sleeps shrink by the
 * same factor. iNav-SITL's scheduler and micros() ride on CLOCK_MONOTONIC and its
 * pacing usleep()/nanosleep(); scaling both makes the vanilla firmware step faster
 * than real time without touching its source. The World process rides the same
 * shim, so physics-time and iNav-time co-accelerate.
 *
 * FB_TIME_SCALE=1 (or unset) = passthrough (real time). */
#define _GNU_SOURCE
#include <time.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>

static double scale = 0.0;                 /* 0 = uninitialised */
static struct timespec epoch;              /* real MONOTONIC at first use */
static int (*real_clock_gettime)(clockid_t, struct timespec*);
static int (*real_nanosleep)(const struct timespec*, struct timespec*);
static int (*real_clock_nanosleep)(clockid_t, int, const struct timespec*, struct timespec*);

static double ts2s(const struct timespec *t){ return t->tv_sec + t->tv_nsec*1e-9; }
static void s2ts(double s, struct timespec *t){ t->tv_sec=(time_t)s; t->tv_nsec=(long)((s-t->tv_sec)*1e9); }

static void init(void){
    real_clock_gettime     = dlsym(RTLD_NEXT, "clock_gettime");
    real_nanosleep         = dlsym(RTLD_NEXT, "nanosleep");
    real_clock_nanosleep   = dlsym(RTLD_NEXT, "clock_nanosleep");
    const char *e = getenv("FB_TIME_SCALE");
    scale = (e && atof(e) > 0.0) ? atof(e) : 1.0;
    real_clock_gettime(CLOCK_MONOTONIC, &epoch);
}

/* virtual = epoch + (real-epoch)*scale, applied to the free-running clocks only */
static int scaled_now(clockid_t id, struct timespec *t){
    int rc = real_clock_gettime(id, t);
    if(rc || scale==1.0) return rc;
    if(id==CLOCK_MONOTONIC || id==CLOCK_MONOTONIC_RAW || id==CLOCK_BOOTTIME){
        s2ts(ts2s(&epoch) + (ts2s(t)-ts2s(&epoch))*scale, t);
    }
    return rc;
}

int clock_gettime(clockid_t id, struct timespec *t){
    if(scale==0.0) init();
    return scaled_now(id, t);
}

int nanosleep(const struct timespec *req, struct timespec *rem){
    if(scale==0.0) init();
    if(scale==1.0) return real_nanosleep(req, rem);
    struct timespec s; s2ts(ts2s(req)/scale, &s);
    return real_nanosleep(&s, rem);           /* rem in real units; callers that retry re-scale next call */
}

int clock_nanosleep(clockid_t id, int flags, const struct timespec *req, struct timespec *rem){
    if(scale==0.0) init();
    if(scale==1.0) return real_clock_nanosleep(id, flags, req, rem);
    if(flags & TIMER_ABSTIME){
        /* absolute deadline on a scaled clock: convert to a relative real wait */
        struct timespec now; scaled_now(id, &now);
        double dv = ts2s(req) - ts2s(&now);
        if(dv <= 0) return 0;
        struct timespec s; s2ts(dv/scale, &s);
        return real_nanosleep(&s, NULL);
    }
    struct timespec s; s2ts(ts2s(req)/scale, &s);
    return real_clock_nanosleep(id, 0, &s, rem);
}

/* usleep()/sleep() route through nanosleep in glibc, so they inherit the scaling. */
