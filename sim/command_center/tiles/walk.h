/* FlightBox renderer — the quadtree walk (Ulrich 2002): each chunk draws itself or its four
 * children, never both. Split by measured SSE against W3_EPS, coarsen (never hole) at the budget.
 * Builds the w3_D draw list; the per-pass split/over counters live here and world3d_stream_at reads them. */
#ifndef W3_TILES_WALK_H
#define W3_TILES_WALK_H
/* --- the tree walk -------------------------------------------------------------------------
 *
 * One rule, and everything else falls out of it:
 *
 *     a chunk draws ITSELF, or its four children -- never both.
 *
 * That is the whole difference from the ring stack. There is no overlap to bias away, so no
 * polygon offset, no draw order, no z-fighting: each square metre belongs to exactly one chunk.
 *
 * Refinement is async and never blocks or leaves a hole. If a chunk wants to split but its
 * children have not arrived, the PARENT keeps drawing while the children load; the swap happens
 * only when all four are resident. "LOD ist doch nur ein VBO pointer" -- exactly: the chunks are
 * already baked and cached, so switching level is picking a different vbo, and a chunk that is not
 * there yet simply is not picked yet.
 *
 * Loading is nearest-first because the walk is depth-first from the camera outward, and fb-tiles
 * has a concurrency cap: whatever asks first gets the network. */
#ifndef W3_TRACE
#define W3_TRACE 0                             /* 1 = log every stream pass */
#endif

/* Can this chunk be replaced by its four children right now? Asks for them either way -- that ask
 * is what starts the fetch. Each child is requested at the SAME step w3_walk will pick when it
 * recurses into it (w3_chunk_px on the child's own coords), so in production the two callers agree
 * and no size is baked twice. W3_LOD_INJECT bumps the step by one to force the disagreement the
 * side-by-side slots exist to absorb -- the proof control, compiled out otherwise. */
static int w3_children_ready(int z,uint32_t x,uint32_t y,int ci[4],
                             double lat,double alt,double ctx,double cty){
  int ok=1;
  for(int q=0;q<4;q++){
    long cx=x*2+(q&1), cy=y*2+(q>>1);
    int lod=w3_lod_for_px(w3_chunk_px(z+1,cx,cy,lat,alt,ctx,cty));
    if(W3_LOD_INJECT) lod = (lod+1<w3_lod_cap) ? lod+1 : lod-1;
    if(lod<0) lod=0;
    ci[q]=w3_cache_get(z+1,(uint32_t)cx,(uint32_t)cy,lod,0);
    if(ci[q]<0) ok=0;                 /* still in flight: keep drawing the parent */
  }
  return ok;
}
static void w3_emit(int ci,int lod){
  if(ci<0) return;
  /* Dropping here is a HOLE in the ground, not a coarser chunk -- the split guard is supposed to
   * have coarsened long before. Count it loudly: a silent drop here would look exactly like the
   * tree working, which is the one thing this must never do. */
  if(w3_frame.nD>=W3_BUDGET){ w3_frame.over++; return; }
  { int L=w3_cache[ci].z-W3_ROOTZ; if(L>=0&&L<8) w3_frame.lvl[L]++; }
  w3_D[w3_frame.nD].vbo=w3_cache[ci].vbo; w3_D[w3_frame.nD].nverts=w3_cache[ci].nverts;
  w3_D[w3_frame.nD].tex[0]=w3_pick_lod(&w3_cache[ci],W3_GROUND_OSM,lod);
  w3_D[w3_frame.nD].tex[1]=w3_pick_lod(&w3_cache[ci],W3_GROUND_PHOTO,lod);
  for(int a=0;a<3;a++){ w3_D[w3_frame.nD].bmin[a]=w3_cache[ci].bmin[a]; w3_D[w3_frame.nD].bmax[a]=w3_cache[ci].bmax[a];
                        w3_D[w3_frame.nD].origin[a]=w3_cache[ci].origin[a]; }
  w3_frame.nD++;
}
/* Recurse. (lat,lon,alt) = camera; (tx,ty) = camera's fractional tile coord at THIS level. */
static void w3_walk(int z,long x,long y,double lat,double alt,double tx,double ty){
  long n=1L<<z; if(x<0||y<0||x>=n||y>=n) return;        /* off the map: a real hole, not a gap */
  double span=w3_tile_span(z,lat);
  /* horizontal distance from the camera to the nearest point of this chunk */
  double dx=fabs(tx-((double)x+0.5))-0.5, dy=fabs(ty-((double)y+0.5))-0.5;
  if(dx<0)dx=0; if(dy<0)dy=0;
  double horiz=sqrt(dx*dx+dy*dy)*span;
  if(horiz-span*0.71>W3_REACH) return;                  /* past the drawn world */
  double dist=sqrt(horiz*horiz+alt*alt); if(dist<1.0) dist=1.0;

  /* Texture step for THIS chunk from its own on-screen size -- the same arithmetic
   * w3_children_ready used to request it, so no size is baked twice for one tile. */
  int lod=w3_lod_for_px(span*(double)W3_SSE_K/dist);
  int ci=w3_cache_get(z,(uint32_t)x,(uint32_t)y,lod,(z==W3_ROOTZ));
  if(ci<0) return;                                      /* not here yet; a later frame gets it */

  /* SSE from THIS chunk's own measured error. Flat terrain saturates by itself, mountains
   * subdivide by themselves -- no per-zoom table, no assumption about where we are flying. */
  float sse = w3_cache[ci].err * W3_SSE_K / (float)dist;
  if(z>=W3_MAXZ || sse<=W3_EPS){ w3_emit(ci,lod); return; }

  /* BUDGET: coarsen, never hole. Refusing to split draws this chunk instead -- blurrier, but the
   * ground is still there. Dropping it from the draw list would punch a hole in the world, which
   * is the one thing the tree exists to prevent. */
  if(w3_frame.nD+4>W3_BUDGET){ w3_frame.over++; w3_emit(ci,lod); return; }

  w3_frame.split_want++;
  int cc[4];
  double ctx=tx*2.0, cty=ty*2.0;
  if(!w3_children_ready(z,(uint32_t)x,(uint32_t)y,cc,lat,alt,ctx,cty)){
    w3_frame.split_wait++; w3_ground.dirty=1;                 /* keep the streamer awake for them */
    w3_emit(ci,lod);                                    /* parent covers the ground meanwhile */
    return;
  }
  for(int q=0;q<4;q++) w3_walk(z+1,x*2+(q&1),y*2+(q>>1),lat,alt,ctx,cty);
}
#endif
