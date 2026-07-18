#ifndef FB_CC_JSBRIDGE_H
#define FB_CC_JSBRIDGE_H
/* Browser <-> fb-tiles fetches (EM_JS). window.FB_TILES_URL is the base. Include after <emscripten.h>. */

/* Origin ground, startup only: sync so w3_O.yoff is the REAL ground on the first (only) frame.
 * block=1 waits for a cold origin DEM tile -- without it a 503 spawns the camera underground until
 * the worker's yoff lands seconds later. Sync + startup-only, so the wait is off the frame loop. */
EM_JS(double, fb_fetch_elev, (double lat, double lon), {
  try {
    var base = window.FB_TILES_URL; if(!base) return -1e9;
    var x = new XMLHttpRequest();
    x.open('GET', base + '/elev?lat=' + lat + '&lon=' + lon + '&block=1', false);
    x.send(null);
    if(x.status>=200 && x.status<300){ var v=parseFloat(x.responseText); if(isFinite(v)) return v; }
  } catch(e){}
  return -1e9;
})

/* Ground under the aircraft, ASYNC so it never blocks the frame loop: request kicks off a fetch if
 * none is in flight, get returns the last resolved ASL ground (-1e9 until the first lands). */
EM_JS(void, fb_ground_request, (double lat, double lon), {
  var G = Module.__fbGround || (Module.__fbGround = { val:-1e9, busy:false });
  if(G.busy) return; var base = window.FB_TILES_URL; if(!base) return; G.busy=true;
  fetch(base + '/elev?lat=' + lat + '&lon=' + lon)
    .then(function(r){ return r.ok ? r.text() : null; })
    .then(function(t){ if(t!==null){ var v=parseFloat(t); if(isFinite(v)) Module.__fbGround.val=v; } Module.__fbGround.busy=false; })
    .catch(function(){ Module.__fbGround.busy=false; });
})
EM_JS(double, fb_ground_get, (void), { var G=Module.__fbGround; return G?G.val:-1e9; })

/* One HYG star band into the WASM heap, startup only. A sync XHR can't set responseType=arraybuffer,
 * so the x-user-defined charset makes each responseText char one raw byte. Returns bytes or -1. */
EM_JS(int, fb_fetch_stars, (int band, uint8_t *dst, int maxbytes), {
  try {
    var base = window.FB_TILES_URL; if(!base) return -1;
    var x = new XMLHttpRequest();
    x.open('GET', base + '/t/stars/' + band + '/0/0', false);
    x.overrideMimeType('text/plain; charset=x-user-defined');
    x.send(null);
    if(x.status>=200 && x.status<300){
      var s = x.responseText, n = s.length;
      if(n > maxbytes) return -1;
      for(var i=0;i<n;i++) HEAPU8[dst+i] = s.charCodeAt(i) & 0xff;
      return n;
    }
  } catch(e){}
  return -1;
})

#endif
