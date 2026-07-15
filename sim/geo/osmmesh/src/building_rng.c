/* libosmmesh/src/building_rng.c
 *
 * xorshift64* — tiny PRNG used to pick roof shape / storey count when OSM
 * tags don't specify. Seed = osm_id (reproducible), or a coord-stream hash
 * when osm_id is missing. State 0 is avoided (xorshift would stick there).
 */

#include "building_internal.h"

#include <stddef.h>
#include <stdint.h>

/* Vigna's xorshift64* (https://en.wikipedia.org/wiki/Xorshift). */
static uint64_t xs64_step(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

void osmmesh_building_rng_seed(osmmesh_building_rng *r, uint64_t seed)
{
    /* Avoid the all-zero fixed point and thin out weak seeds by mixing with
     * splitmix64's golden gamma. */
    uint64_t s = seed + 0x9E3779B97F4A7C15ULL;
    s ^= s >> 30; s *= 0xBF58476D1CE4E5B9ULL;
    s ^= s >> 27; s *= 0x94D049BB133111EBULL;
    s ^= s >> 31;
    if (s == 0) s = 0x9E3779B97F4A7C15ULL;
    r->state = s;
}

uint64_t osmmesh_building_rng_u64(osmmesh_building_rng *r)
{
    return xs64_step(&r->state);
}

float osmmesh_building_rng_f01(osmmesh_building_rng *r)
{
    /* 24-bit fraction is plenty for roof-shape weights. */
    uint32_t bits = (uint32_t)(xs64_step(&r->state) >> 40);
    return (float)bits * (1.0f / 16777216.0f);
}

uint32_t osmmesh_building_rng_u32(osmmesh_building_rng *r, uint32_t n)
{
    if (n == 0) return 0;
    /* Rejection sampling to avoid modulo bias. Effectively free for our tiny
     * n (<= 4 choices). */
    uint32_t limit = 0xFFFFFFFFu - (0xFFFFFFFFu % n);
    for (;;) {
        uint32_t v = (uint32_t)(xs64_step(&r->state) >> 32);
        if (v < limit) return v % n;
    }
}

uint64_t osmmesh_building_rng_hash_u32(const uint32_t *data, size_t n)
{
    /* 64-bit FNV-1a over the byte stream. */
    uint64_t h = 0xCBF29CE484222325ULL;
    const uint8_t *p = (const uint8_t *)data;
    size_t bytes = n * sizeof(uint32_t);
    for (size_t i = 0; i < bytes; ++i) {
        h ^= (uint64_t)p[i];
        h *= 0x100000001B3ULL;
    }
    return h ? h : 1ULL;
}
