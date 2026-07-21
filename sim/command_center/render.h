#ifndef W3_RENDER_H
#define W3_RENDER_H
/* The scene passes: sky/stars (sky.h) + terrain, composed by world3d_render_scene. Included after
 * sky.h; reads w3_gl/w3_D/w3_frame/w3_ground. HUD is separate (hud.h) -- the codec runs between
 * them. */

#ifdef W3_USE_OSM
/* Bind a draw-list tile's albedo (ground mode = texture index) + vertex arrays, then draw. MVP is
 * set per-tile by the caller. */
static void w3_draw_tile(int i) {
  GLuint _t = w3_D[i].tex[w3_ground.mode];
  if (!_t) _t = w3_D[i].tex[W3_GROUND_OSM];
  glBindTexture(GL_TEXTURE_2D, _t);
  glBindBuffer(GL_ARRAY_BUFFER, w3_D[i].vbo);
  glVertexAttribPointer(w3_gl.wtPos, 3, GL_FLOAT, GL_FALSE, W3_VTX_STRIDE, W3_VTX_OFF(pos));
  glVertexAttribPointer(w3_gl.wtUV, 2, GL_FLOAT, GL_FALSE, W3_VTX_STRIDE, W3_VTX_OFF(uv));
  glVertexAttribPointer(w3_gl.wtNorm, 3, GL_FLOAT, GL_FALSE, W3_VTX_STRIDE, W3_VTX_OFF(norm));
  glDrawArrays(GL_TRIANGLES, 0, w3_D[i].nverts);
}

/* Camera-relative ECEF terrain: camera sits at the ECEF origin, each tile's model translation is
 * (origin_ecef - cam_ecef) done in double then cast to float -- best precision at the camera. The
 * frustum is built once from the untranslated viewproj; each tile's AABB is shifted before the
 * cull. uSunUp = sin(sun elevation) so the terminator doesn't read a meaningless uSun.y. */
static void w3_draw_terrain(const w3_cam *C, const w3_atmo *A, const double cam_ecef[3],
                            const float sun_ecef[3], float sun_up) {
  glUseProgram(w3_gl.pWT);
  glUniform3fv(w3_gl.wtHaze, 1, A->haze);
  glUniform1f(w3_gl.wtLight, A->light);
  glUniform3fv(w3_gl.wtSun, 1, sun_ecef);
  glUniform1f(w3_gl.wtSunUp, sun_up);
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(w3_gl.wtTex, 0);
  glEnableVertexAttribArray(w3_gl.wtPos);
  glEnableVertexAttribArray(w3_gl.wtUV);
  glEnableVertexAttribArray(w3_gl.wtNorm);
  const w3_frustum fr = w3_frustum_from(C->mvp);
  w3_frame.nvis = 0;
  for (int i = 0; i < w3_frame.nD; i++) {
    float trans[3];
    for (int a = 0; a < 3; a++)
      trans[a] = (float)(w3_D[i].origin[a] - cam_ecef[a]);
    float bmn[3], bmx[3];
    for (int a = 0; a < 3; a++) {
      bmn[a] = w3_D[i].bmin[a] + trans[a];
      bmx[a] = w3_D[i].bmax[a] + trans[a];
    }
    if (!w3_aabb_visible(&fr, bmn, bmx)) continue;
    w3_frame.nvis++;
    float mvpT[16];
    w3_mvp_translate(mvpT, C->mvp, trans);
    glUniformMatrix4fv(w3_gl.wtMVP, 1, GL_FALSE, mvpT);
    w3_draw_tile(i);
  }
  glDisableVertexAttribArray(w3_gl.wtUV);
  glDisableVertexAttribArray(w3_gl.wtNorm);
}
#endif /* W3_USE_OSM */

/* Fallback ground: procedural wire terrain + buildings, drawn when no OSM tiles are resident. */
static void w3_draw_fallback(const w3_cam *C, const w3_atmo *A) {
  glUseProgram(w3_gl.pW);
  glUniformMatrix4fv(w3_gl.wMVP, 1, GL_FALSE, C->mvp);
  glUniform3fv(w3_gl.wHaze, 1, A->haze);
  glUniform1f(w3_gl.wLight, A->light);
  glEnableVertexAttribArray(w3_gl.wPos);
  glEnableVertexAttribArray(w3_gl.wCol);
  glBindBuffer(GL_ARRAY_BUFFER, w3_gl.vTerr);
  glVertexAttribPointer(w3_gl.wPos, 3, GL_FLOAT, GL_FALSE, 24, 0);
  glVertexAttribPointer(w3_gl.wCol, 3, GL_FLOAT, GL_FALSE, 24, (void *)12);
  glDrawArrays(GL_TRIANGLES, 0, w3_gl.nTerr);
  glBindBuffer(GL_ARRAY_BUFFER, w3_gl.vBld);
  glVertexAttribPointer(w3_gl.wPos, 3, GL_FLOAT, GL_FALSE, 24, 0);
  glVertexAttribPointer(w3_gl.wCol, 3, GL_FLOAT, GL_FALSE, 24, (void *)12);
  glDrawArrays(GL_TRIANGLES, 0, w3_gl.nBld);
  glDisableVertexAttribArray(w3_gl.wCol);
}

/* The aircraft camera image: sky + stars + terrain, in order, into the bound framebuffer. */
static void world3d_render_scene(const telem_packet_t *t, int W, int H, int have) {
  glViewport(0, 0, W, H);
  glEnable(GL_DEPTH_TEST);
  glClearColor(0.55f, 0.70f, 0.90f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  /* Eye in render-ENU for the sky/star infinity pass + procedural fallback ONLY; the ECEF terrain
   * carries its own cam_ecef (double) below, so this eye's exact height is immaterial. */
  float px = have ? t->x : 0, py = (have && t->alt > 2) ? t->alt : 120, pz = have ? -t->y : 0;
  const float eye[3] = {px, py, pz};
  const w3_cam C = w3_cam_from(have ? t->yaw : 0, have ? t->pitch : 0, have ? t->roll : 0, eye,
                               W3_FOV, (float)W / H, W3_NEAR, W3_FARPLANE);
  /* Atmosphere derived once (atmo.h, pure). SVS (OSM) is deliberately TIME-INDEPENDENT -- a
   * constant daylit database view, no day/night/stars/clouds; EVS (photo) keeps the real
   * sun/sky/stars. */
  const w3_atmo A = (w3_ground.mode == W3_GROUND_OSM) ? w3_atmo_synthetic() : w3_atmo_from(t, have);

  /* Sky+stars stay in local render-ENU (up=+Y): at infinity, so they key off the local vertical and
   * the horizon dip emerges as the ECEF terrain curves away below the flat local horizon. */
  w3_draw_sky(&C, &A, (float)W / H, sinf(w3_horizon_dip_rad(w3_agl)));
  if (w3_ground.mode == W3_GROUND_PHOTO) w3_draw_stars(&C, &A, eye); /* real stars -> EVS only */
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
#ifdef W3_USE_OSM
  if (w3_frame.nD > 0) {
    /* Camera geodetic -> ECEF: ENU offset (t->x east, t->y north) back to lat/lon around the
     * origin, alt = ASL as ellipsoidal height (geoid ignored). No telemetry: sit over the origin
     * ground. */
    const double MPD = (double)FB_M_PER_DEG_LAT;
    double clat = have ? w3_O.lat + (double)t->y / MPD : w3_O.lat;
    double clon = have ? w3_O.lon + (double)t->x / (MPD * cos(w3_O.lat * M_PI / 180.0)) : w3_O.lon;
    double calt = (have && t->alt > 1) ? (double)t->alt : (double)w3_O.yoff + 2.0;
    /* The eye must NEVER sink below the surface. Clamp to the aircraft's geometry-true floor: the DEM
     * ground under it (ground = ASL − AGL) plus the model belly clearance — where the airframe physically
     * rests on terrain. So on a crash the view stops at that height, never under the ground. */
    if (have) { double floor = (double)t->alt - (double)w3_agl + (double)w3_cam_clear; if (calt < floor) calt = floor; }
    osmmesh_geo cg = {clon, clat, calt};
    osmmesh_ecef ce = osmmesh_geo_to_ecef(cg);
    double cam_ecef[3] = {ce.x, ce.y, ce.z};
    const w3_cam Ce = w3_cam_ecef(have ? t->yaw : 0, have ? t->pitch : 0, have ? t->roll : 0, clat,
                                  clon, W3_FOV, (float)W / H, W3_NEAR, W3_FARPLANE);
    float sun_ecef[3];
    w3_render_to_ecef(clat, clon, A.sun, sun_ecef); /* topocentric sun -> ECEF axes */
    w3_draw_terrain(&Ce, &A, cam_ecef, sun_ecef, A.sun[1]);
  } else
#endif
    w3_draw_fallback(&C, &A);
}
#endif
