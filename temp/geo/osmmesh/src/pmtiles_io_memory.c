/* libosmmesh/src/pmtiles_io_memory.c
 *
 * Memory-backed IO backend for the PMTiles reader. The reader stores a
 * borrowed (data, len) pair; read() performs a bounds-checked memcpy and
 * size() returns the buffer length. No allocation beyond the small ctx
 * struct itself.
 *
 * Intended for WASM: the browser fetches the entire .pmtiles file into
 * an ArrayBuffer via fetch() + response.arrayBuffer(), copies the bytes
 * into the wasm linear memory via HEAPU8.set(), and passes (ptr, len) to
 * osmmesh_pmtiles_open_memory. HTTP-range-based streaming is deferred to
 * a later phase because it complicates the directory-traversal paths
 * (every leaf-dir fetch becomes a network round-trip) for little MVP gain
 * on our ~10 MB archives.
 */

#include "osmmesh/pmtiles.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t         len;
} pm_mem_ctx;

static int pm_mem_read(void *ctx, uint64_t offset, size_t len, void *buf) {
    pm_mem_ctx *c = (pm_mem_ctx *)ctx;
    if (!c || !c->data) return -1;
    /* Bounds check: offset + len must stay within [0, c->len]. Guard against
     * overflow on the addition too. */
    if (offset > (uint64_t)c->len) return -1;
    if ((uint64_t)len > (uint64_t)c->len - offset) return -1;
    memcpy(buf, c->data + (size_t)offset, len);
    return 0;
}

static uint64_t pm_mem_size(void *ctx) {
    pm_mem_ctx *c = (pm_mem_ctx *)ctx;
    if (!c) return 0;
    return (uint64_t)c->len;
}

/* Internal entry points (referenced from pmtiles.c via extern). */
int  osmmesh_pmtiles__mem_open(osmmesh_pmtiles_io *out,
                                const uint8_t *data, size_t len);
void osmmesh_pmtiles__mem_close(osmmesh_pmtiles_io *io);

int osmmesh_pmtiles__mem_open(osmmesh_pmtiles_io *out,
                               const uint8_t *data, size_t len) {
    if (!out || !data) return -1;
    pm_mem_ctx *c = (pm_mem_ctx *)calloc(1, sizeof(*c));
    if (!c) return -1;
    c->data = data;
    c->len  = len;
    out->read = pm_mem_read;
    out->size = pm_mem_size;
    out->ctx  = c;
    return 0;
}

void osmmesh_pmtiles__mem_close(osmmesh_pmtiles_io *io) {
    if (!io || !io->ctx) return;
    free(io->ctx);
    io->ctx  = NULL;
    io->read = NULL;
    io->size = NULL;
}
