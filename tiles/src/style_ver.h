/* Shared between fb-tiles (bake filenames) and the client (bake URL ?v=): the OSM bake style version.
 * Bake responses are Cache-Control:immutable, so the URL must change whenever the rendering does —
 * otherwise browsers keep serving pre-fix tiles for a year (the stripe-bug-still-visible incident). */
#ifndef FB_STYLE_VER_H
#define FB_STYLE_VER_H
#define FB_OSM_STYLE_VER 12
#define FB_STYLE_STR2(x) #x
#define FB_STYLE_STR(x) FB_STYLE_STR2(x)
#define FB_OSM_STYLE_VER_S FB_STYLE_STR(FB_OSM_STYLE_VER)
#endif
