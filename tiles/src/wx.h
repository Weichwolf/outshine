#ifndef FB_WX_H
#define FB_WX_H

/* /wx -- global wind and cloud state from the NOAA GFS analysis, in the packed FBWX format
 * (src/wxfmt.h). Call once before the worker pool starts. */
int  fb_wx_init(const char *cache_dir);

/* Blocks THIS worker until the current run's blob exists, exactly like /bake: no deadline, no 202,
 * the client's HTTP timeout is the only boundary. N concurrent callers produce ONE build. */
void fb_wx_handle(int fd);

void fb_wx_stats(long *served, long *built, long *disk_hits, long *fetch_fail,
                 long *decode_fail, long *stale, long *run_fallback);

#endif
