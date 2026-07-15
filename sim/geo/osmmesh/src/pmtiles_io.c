/* libosmmesh/src/pmtiles_io.c
 *
 * File-backed IO backend for the PMTiles reader. Uses stdio (fopen/fseek/
 * fread). Windows Note: fseek with a long offset is fine for the current
 * fixture sizes, but for multi-GB archives we may later need _fseeki64 /
 * fseeko64. Kept simple for T2; revisit when we first hit >2 GB inputs.
 */

/* glibc hides fseeko/off_t under strict -std=c99 (__STRICT_ANSI__). Request
 * the POSIX.1-2008 surface before any libc header so the non-Windows branch
 * below sees them. MSYS2/UCRT is unaffected (it uses _fseeki64). */
#if !defined(_WIN32)
#  define _POSIX_C_SOURCE 200809L
#endif

#include "osmmesh/pmtiles.h"

#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE *fp;
} pm_file_ctx;

static int pm_file_read(void *ctx, uint64_t offset, size_t len, void *buf) {
    pm_file_ctx *c = (pm_file_ctx *)ctx;
    if (!c || !c->fp) return -1;
    /* Cast to long; safe for <2 GB. Revisit for large archives. */
#if defined(_WIN32)
    if (_fseeki64(c->fp, (long long)offset, SEEK_SET) != 0) return -1;
#else
    if (fseeko(c->fp, (off_t)offset, SEEK_SET) != 0) return -1;
#endif
    if (fread(buf, 1, len, c->fp) != len) return -1;
    return 0;
}

static uint64_t pm_file_size(void *ctx) {
    pm_file_ctx *c = (pm_file_ctx *)ctx;
    if (!c || !c->fp) return 0;
#if defined(_WIN32)
    long long cur = _ftelli64(c->fp);
    if (cur < 0) return 0;
    if (_fseeki64(c->fp, 0, SEEK_END) != 0) return 0;
    long long end = _ftelli64(c->fp);
    _fseeki64(c->fp, cur, SEEK_SET);
    return end < 0 ? 0 : (uint64_t)end;
#else
    long cur = ftell(c->fp);
    if (cur < 0) return 0;
    if (fseek(c->fp, 0, SEEK_END) != 0) return 0;
    long end = ftell(c->fp);
    fseek(c->fp, cur, SEEK_SET);
    return end < 0 ? 0 : (uint64_t)end;
#endif
}

/* Internal entry points (referenced from pmtiles.c via extern). */
int osmmesh_pmtiles__file_open(osmmesh_pmtiles_io *out, const char *path);
void osmmesh_pmtiles__file_close(osmmesh_pmtiles_io *io);

int osmmesh_pmtiles__file_open(osmmesh_pmtiles_io *out, const char *path) {
    if (!out || !path) return -1;
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    pm_file_ctx *c = (pm_file_ctx *)calloc(1, sizeof(*c));
    if (!c) { fclose(fp); return -1; }
    c->fp = fp;
    out->read = pm_file_read;
    out->size = pm_file_size;
    out->ctx  = c;
    return 0;
}

void osmmesh_pmtiles__file_close(osmmesh_pmtiles_io *io) {
    if (!io || !io->ctx) return;
    pm_file_ctx *c = (pm_file_ctx *)io->ctx;
    if (c->fp) fclose(c->fp);
    free(c);
    io->ctx = NULL;
    io->read = NULL;
    io->size = NULL;
}
