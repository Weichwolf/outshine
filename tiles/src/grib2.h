#ifndef FB_GRIB2_H
#define FB_GRIB2_H
#include <stddef.h>
#include <stdint.h>

/* Reader for exactly the GRIB2 shape NCEP ships in the GFS pgrb2 products: grid definition
 * template 3.0 (regular lat/lon) with scanning mode 0, product definition template 4.0/4.8, and
 * data representation template 5.0 (simple packing) or 5.3 (complex packing + spatial
 * differencing). Every GFS record fb-tiles asks for is 5.3 today.
 *
 * Anything outside that is refused BY NAME (fb_grib2_last_error) instead of guessed at -- this
 * parses an upstream response, so a shape we don't understand has to fail cleanly rather than
 * decode into plausible-looking garbage. Deliberately unsupported: section 6 bitmaps and the
 * template 5.3 missing-value management (both absent from every field we fetch; supporting them
 * half-way is how a decoder starts lying). */

typedef struct {
    uint8_t  discipline, category, number;   /* GRIB2 code tables 0.0 / 4.1 / 4.2 */
    uint8_t  level_type;                     /* code table 4.5 (100 isobaric, 103 m AGL, ...) */
    double   level_value;                    /* first fixed surface, scale factor applied (Pa, m) */
    uint32_t nx, ny, npoints;
    double   lat0, lon0, dlat, dlon;         /* degrees; (lat0,lon0) is grid point (0,0) */
    int64_t  ref_epoch;                      /* reference (model run) time, UTC seconds */
    int64_t  valid_epoch;                    /* ref_epoch + forecast time */
} fb_grib2_field;

/* `v` holds f->npoints values in grid order and is owned by the walk -- borrow, don't keep.
 * Return 0 to keep walking; any non-zero value stops the walk and becomes its result. */
typedef int (*fb_grib2_sink)(const fb_grib2_field *f, const float *v, void *ud);

/* Number of fields handed to `sink`, or < 0 on a malformed/unsupported message. */
int fb_grib2_walk(const uint8_t *buf, size_t n, fb_grib2_sink sink, void *ud);

const char *fb_grib2_last_error(void);

#endif
