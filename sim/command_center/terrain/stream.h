/* FlightBox renderer — the per-frame terrain streamer: re-walk the quadtree around the camera,
 * trim what it no longer asks for, and latch "done" when nothing is in flight. Included from
 * within world3d.h's W3_USE_OSM block; drives w3_walk / w3_cache_trim from tiles/. */
#ifndef W3_TERRAIN_STREAM_H
#define W3_TERRAIN_STREAM_H
/* Called every frame. Two reasons to do work:
 *   1. the camera moved into a new chunk -> the tree wants a different set
 *   2. the last pass was INCOMPLETE -> chunks were still in flight from fb-tiles
 * (2) is what makes async sourcing work: a chunk that has not arrived is covered by its parent and
 * picked up on a later frame, so the world refines progressively and the frame loop never blocks.
 * Cached chunks cost nothing to re-visit, so retrying is cheap. */
static int w3_stream_done=0;
static void world3d_stream_at(double lat,double lon,double alt_agl){
  if(!w3_osm) return;
  uint32_t tx,ty; if(osmmesh_geo_to_tile(lon,lat,W3_MAXZ,&tx,&ty)!=0) return;
  int moved = !w3_O.have_tile || tx!=w3_O.tx || ty!=w3_O.ty;
  /* W3_LOD_SPIN (proof only, 0 in production): defeat the "converged -> sleep" early-out so the
   * tree walk runs EVERY frame. That is the adversarial condition the LOD-slot proof needs -- the
   * old size-disagreement thrash was 256 mipmaps/frame precisely because the walk ran every frame;
   * once it sleeps, no design re-bakes. Spinning here lets the mipmap counter separate a design
   * that re-bakes under a persistent disagreement (NOKEY) from one that does not (the slots). */
  if(!moved && w3_stream_done && w3_frame.sharpen==0 && !W3_LOD_SPIN) return;   /* climb keeps us awake */
  w3_O.tx=tx; w3_O.ty=ty; w3_O.have_tile=1;
  int b0=w3_frame.cache_bakes, h0=w3_frame.cache_hits;
  w3_ground.dirty=0;                     /* recomputed by the w3_cache_get calls below */
  unsigned mark=w3_touch+1;              /* everything the walk touches gets touch >= mark */
  w3_frame.pass_mark=mark;                     /* want_lod_max resets off this on a chunk's first touch */

  w3_frame.nD=0; w3_frame.split_want=0; w3_frame.split_wait=0; w3_frame.over=0; w3_frame.pending=0; w3_frame.sharpen=0;
  for(int i=0;i<8;i++) w3_frame.lvl[i]=0;
  double rtx,rty; w3_geo_to_tile_f(lat,lon,W3_ROOTZ,&rtx,&rty);
  /* Roots: enough W3_ROOTZ chunks to cover W3_REACH. The tree prunes the rest immediately. */
  int rad=(int)ceil(W3_REACH/w3_tile_span(W3_ROOTZ,lat))+1;
  for(int dy=-rad;dy<=rad;dy++)for(int dx=-rad;dx<=rad;dx++)
    w3_walk(W3_ROOTZ,(long)rtx+dx,(long)rty+dy,lat,alt_agl,rtx,rty);

  w3_cache_trim(mark);                   /* whatever the walk did not ask for is unreachable */

  int was_done = w3_stream_done;
  /* "Done" means the tree got everything it asked for AND every chunk shows the albedo we want.
   * Without the second half a TAB only took effect at the next chunk boundary, because this
   * function returns early once done and w3_cache_get, which does the upgrading, never ran again.
   * The tiles were on disk the whole time; we had simply stopped asking. */
  /* w3_frame.nD>0 is not pedantry: "nothing is waiting" is also true of a tree that asked for nothing.
   * Without it the first pass over a cold region -- where every chunk is still in flight -- sets
   * done=1, this function returns early forever, and the world stays empty with no error anywhere.
   * That is exactly what happened: "0 chunks drawn, 0 waiting", then silence. */
  /* "Done" is the FLOOR criterion: every chunk drawable (256 present), nothing waiting. Sharpening
   * up the ramp (w3_frame.sharpen) is deliberately NOT here -- it keeps the streamer awake (the early-out
   * above) but must not hold the gate, or a moving camera forever mid-climb on a fresh 2048 would
   * never let "0 pending" latch. */
  w3_stream_done = w3_frame.nD>0 && w3_frame.pending==0 && w3_frame.split_wait==0 && !w3_ground.dirty;
  /* Report on a transition, or when the sharpening count CHANGES (a step landed) -- not every frame
   * while it merely stays nonzero, which under motion would be every frame. */
  static int w3_last_sharpen=-1;
  if(moved || (w3_stream_done && !was_done) || w3_frame.sharpen!=w3_last_sharpen || W3_TRACE){
    w3_last_sharpen=w3_frame.sharpen;
    printf("[world3d] quadtree: %d chunks drawn (budget %d), %d wanted split, %d waiting,"
           " %d pending, %d sharpening, %d over budget | levels %d/%d/%d/%d/%d/%d/%d | baked %d, cached %d, evicted %d, vram %ldMB%s\n",
           w3_frame.nD,W3_BUDGET,w3_frame.split_want,w3_frame.split_wait,w3_frame.pending,w3_frame.sharpen,w3_frame.over,
           w3_frame.lvl[0],w3_frame.lvl[1],w3_frame.lvl[2],w3_frame.lvl[3],w3_frame.lvl[4],w3_frame.lvl[5],w3_frame.lvl[6],
           w3_frame.cache_bakes-b0,w3_frame.cache_hits-h0,w3_frame.cache_evict,w3_texvram()/(1024*1024),
           w3_stream_done?"":" (waiting on fb-tiles)");
  }
}
#endif
