#ifndef FB_CC_CODEC_H
#define FB_CC_CODEC_H
/* EVS video link: WebCodecs H.264 encode->decode, modelling the lossy 5.8 GHz downlink.
 * Include after <emscripten.h> and the GL headers. Returns 0 from init where WebCodecs is absent.
 */

// clang-format off
EM_JS(int, fb_codec_init, (int w, int h), {
  if (typeof VideoEncoder === 'undefined' || typeof VideoDecoder === 'undefined') {
    console.warn('[fb] WebCodecs nicht verfuegbar — Rohbild ohne Video-Artefakte.'); return 0;
  }
  const S = { ready:false, frame:null, n:0 }; Module.__fb = S;
  S.decoder = new VideoDecoder({ output:(f)=>{ if(S.frame) S.frame.close(); S.frame=f; },
                                 error:(e)=>console.error('[fb] dec',e) });
  S.encoder = new VideoEncoder({
    output:(chunk,meta)=>{ if(meta && meta.decoderConfig && S.decoder.state==='unconfigured') S.decoder.configure(meta.decoderConfig);
                           if(S.decoder.state==='configured') S.decoder.decode(chunk); },
    error:(e)=>console.error('[fb] enc',e) });
  try {
    S.encoder.configure({ codec:'avc1.42001f', width:w, height:h,
      bitrate: 1500000, framerate: 60, latencyMode:'realtime', avc:{format:'avc'} });
  } catch(e){ console.error('[fb] enc.configure', e); return 0; }
  S.ready = true; console.log('[fb] WebCodecs bereit', w+'x'+h); return 1;
})

EM_JS(void, fb_codec_push, (int ptr, int w, int h, double ts), {
  const S = Module.__fb; if(!S||!S.ready) return;
  if(S.encoder.encodeQueueSize > 2) return;
  let vf; try { vf = new VideoFrame(HEAPU8.subarray(ptr, ptr+w*h*4),
    { format:'RGBA', codedWidth:w, codedHeight:h, timestamp:ts }); } catch(e){ return; }
  const key = (S.n % 50) === 0; S.n++;
  try { S.encoder.encode(vf, { keyFrame:key }); } catch(e){}
  vf.close();
})

EM_JS(int, fb_codec_upload, (int texId), {
  const S = Module.__fb; if(!S||!S.frame) return 0;
  const tex = GL.textures[texId]; if(!tex) return 0;
  GLctx.bindTexture(GLctx.TEXTURE_2D, tex);
  try { GLctx.texImage2D(GLctx.TEXTURE_2D,0,GLctx.RGBA,GLctx.RGBA,GLctx.UNSIGNED_BYTE, S.frame); }
  catch(e){ return 0; }
  return 1;
})
  // clang-format on

#endif
