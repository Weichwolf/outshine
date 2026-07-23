// Static fallback/doc copy of the config the hub (flightbox/server.c) generates dynamically at
// GET /config.js from ORIGIN_LAT/ORIGIN_LON/SIM_UTC/TILES_URL env -- that dynamic route always
// takes precedence (server.c special-cases the path before it ever falls through to serving this
// file). Kept so the defaults are readable without a running server and as a static-serving
// fallback if fbserver is ever bypassed. Grenchen defaults, same as web-up.sh.
window.FB_ORIGIN_LAT = 47.179846;
window.FB_ORIGIN_LON = 7.411427;
window.FB_SIM_UTC = 0;              // 0 = real time
window.FB_TILES_URL = 'http://localhost:8081';
window.FB_GROUND_CLEAR = 0;
