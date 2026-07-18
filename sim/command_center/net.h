#ifndef FB_CC_NET_H
#define FB_CC_NET_H
/* Telemetry/control WebSocket to the server that served the page. Telemetry lands in `telem`
 * (have_telem set on the first packet); control goes out via send_control() in cc.c. */

static EMSCRIPTEN_WEBSOCKET_T ws;
static int ws_open = 0;
static telem_packet_t telem;
static int have_telem = 0;

static EM_BOOL on_open(int t, const EmscriptenWebSocketOpenEvent *e, void *u) {
  (void)t;
  (void)e;
  (void)u;
  ws_open = 1;
  return 1;
}
static EM_BOOL on_close(int t, const EmscriptenWebSocketCloseEvent *e, void *u) {
  (void)t;
  (void)e;
  (void)u;
  ws_open = 0;
  return 1;
}
static EM_BOOL on_msg(int t, const EmscriptenWebSocketMessageEvent *e, void *u) {
  (void)t;
  (void)u;
  if (e->numBytes < 4) return 1;
  uint32_t mg;
  memcpy(&mg, e->data, 4);
  if (mg == FB_MAGIC_TELEM && e->numBytes == sizeof(telem_packet_t)) {
    memcpy(&telem, e->data, sizeof telem);
    have_telem = 1;
  }
  return 1;
}

static void net_init(void) {
  char url[256];
  const char *host = emscripten_run_script_string("location.host");
  snprintf(url, sizeof url, "ws://%s/ws", host && *host ? host : "127.0.0.1:8080");
  EmscriptenWebSocketCreateAttributes attr = {url, NULL, EM_TRUE};
  ws = emscripten_websocket_new(&attr);
  emscripten_websocket_set_onopen_callback(ws, 0, on_open);
  emscripten_websocket_set_onclose_callback(ws, 0, on_close);
  emscripten_websocket_set_onmessage_callback(ws, 0, on_msg);
}

#endif
