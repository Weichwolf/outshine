#ifndef FB_WXFMT_H
#define FB_WXFMT_H
#include <stdint.h>

/* The /wx wire format ("FBWX"). This header IS the contract: fb-tiles writes it, the simulator
 * reads it, and a baked gym fixture is a byte-for-byte copy of one response. Full prose in
 * doc/flightbox/world-and-terrain.md, section "Wetter -- /wx".
 *
 * Everything is little-endian. Layout is header (FB_WX_HEADER_BYTES) + field_count descriptors
 * (FB_WX_DESC_BYTES each) + the payload planes, each descriptor carrying its own absolute offset
 * so a reader never has to accumulate sizes.
 *
 * Sample order inside a plane is row-major, row 0 = lat0 (north), column 0 = lon0, i.e.
 * sample(i,j) sits at lat = lat0 + j*dlat, lon = lon0 + i*dlon. Values are
 * value = offset + raw * scale, in the SI unit implied by the variable code.
 *
 * FB_WX_FMT_VER is the immutable-cache handle for generated weather bytes, exactly what
 * FB_OSM_STYLE_VER is for bakes: it appears in the on-disk filename, so any change to what a byte
 * means moves the artefact instead of poisoning the old one.
 *
 * Nothing in a blob depends on WHEN it was produced -- one GFS run at one format version is one
 * byte sequence, on every machine, across restarts and rebuilds. That is what lets a baked gym
 * fixture be compared by checksum instead of by tolerance. */
#define FB_WX_MAGIC        0x58574246u   /* 'F','B','W','X' little-endian */
#define FB_WX_FMT_VER      1
#define FB_WX_HEADER_BYTES 64
#define FB_WX_DESC_BYTES   24

/* Variable codes. The unit follows from the code, so no unit field: 1/2 m/s, 3 m, 4 %, 5 m. */
#define FB_WX_VAR_WIND_U   1   /* eastward wind component, m/s */
#define FB_WX_VAR_WIND_V   2   /* northward wind component, m/s */
#define FB_WX_VAR_HEIGHT   3   /* geopotential height, m */
#define FB_WX_VAR_CLOUD    4   /* cloud cover, % */
#define FB_WX_VAR_VIS      5   /* horizontal visibility, m */

/* Level codes. level_value carries metres for AGL and hPa for isobaric; 0 for the rest. */
#define FB_WX_LEV_AGL          1
#define FB_WX_LEV_ISOBARIC     2
#define FB_WX_LEV_CLOUD_LOW    3
#define FB_WX_LEV_CLOUD_MID    4
#define FB_WX_LEV_CLOUD_HIGH   5
#define FB_WX_LEV_ATMOSPHERE   6
#define FB_WX_LEV_SURFACE      7
#define FB_WX_LEV_CLOUD_CEIL   8

#define FB_WX_HDR_FLAG_LON_WRAP 0x01   /* column nx-1 is one dlon short of column 0 again */
#define FB_WX_FLD_FLAG_MISSING  0x01   /* raw == missing_raw means "no value here" */

#define FB_WX_SOURCE_GFS_0P25   1

/* Header field offsets, so a reader can be written without transcribing a table.
 *   0 u32 magic          4 u16 format_version   6 u16 header_bytes
 *   8 u16 nx            10 u16 ny
 *  12 f32 lat0          16 f32 lon0            20 f32 dlat          24 f32 dlon
 *  28 u32 run_epoch     32 u32 valid_epoch     36 u32 reserved (zero)
 *  40 u16 field_count   42 u16 desc_bytes      44 u32 payload_bytes
 *  48 u8  flags         49 u8  source          50 u16 grid_step     52 u8[12] reserved (zero)
 *
 * Descriptor field offsets:
 *   0 u8  var            1 u8  level_kind       2 u16 level_value
 *   4 u8  bits (8|16)    5 u8  flags            6 u16 missing_raw
 *   8 f32 scale         12 f32 offset
 *  16 u32 payload_off   20 u32 payload_bytes                                                   */

#endif
