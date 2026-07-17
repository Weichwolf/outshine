/* FlightBox renderer — the sky and star draw passes. The GLSL sources live in gfx/shaders.h; this is
 * the draw side: the fullscreen sky dome and the real-star GL_POINTS pass, each over the frame's
 * camera (w3_cam) and atmosphere (w3_atmo). Reads the shared w3_gl handles and the origin w3_O; the
 * star catalogue + celestial maths are in stars.h. Included from world3d.h after those are defined.
 *
 * The <0.6 star-day cutoff and the sky shader's own `day` are preserved as they were -- unifying the
 * three `day` curves (atmo linear / sky smoothstep / this cutoff) is a separate visual decision. */
#ifndef W3_SKY_H
#define W3_SKY_H

/* Sky dome: fullscreen quad, per-pixel ray from the camera basis; depth writes OFF so the terrain
 * paints over it. `aspect` is the viewport W/H the projection used (not carried in w3_cam). */
static void w3_draw_sky(const w3_cam *C, const w3_atmo *A, float aspect){
  const float RAD=(float)M_PI/180.f;
  glDepthMask(GL_FALSE); glDisable(GL_DEPTH_TEST);
  glUseProgram(w3_gl.pSky);
  glUniform3fv(w3_gl.skF,1,C->f); glUniform3fv(w3_gl.skS,1,C->sr); glUniform3fv(w3_gl.skU,1,C->up);
  glUniform1f(w3_gl.skTan,tanf(W3_FOV*RAD*0.5f)); glUniform1f(w3_gl.skAsp,aspect);
  glUniform3fv(w3_gl.skSun,1,A->sun); glUniform3fv(w3_gl.skMoon,1,A->moon);
  glUniform1f(w3_gl.skMoonPh,A->moon_ph); glUniform1f(w3_gl.skCloud,A->cloud);
  glBindBuffer(GL_ARRAY_BUFFER,w3_gl.skyVBO); glEnableVertexAttribArray(w3_gl.skPos);
  glVertexAttribPointer(w3_gl.skPos,2,GL_FLOAT,GL_FALSE,0,0); glDrawArrays(GL_TRIANGLES,0,6);
  glDisableVertexAttribArray(w3_gl.skPos);
}

/* Real stars: place each above-horizon catalogue star at its true alt/az (wall-clock sidereal time +
 * origin), far along that direction, additively blended, fading toward day. Runs with the sky's
 * depth-off state. The celestial maths is in stars.h because it is pure and therefore checkable --
 * Polaris must stand at the observer's latitude, due north, and nothing in a screenshot says whether
 * it does. The clock is passed IN rather than read there: an input, not a dependency. */
static void w3_draw_stars(const w3_cam *C, const w3_atmo *A, const float eye[3]){
  if(A->day<0.6f){
    double lst=w3_lst_deg(w3_gmst_deg((double)time(NULL)), w3_O.lon);
    static float sv[W3_NSTARS*4]; int ns=0;
    for(int i=0;i<W3_NSTARS;i++){
      w3_stardir d=w3_star_dir(lst,w3_O.lat,W3_STARS[i].ra,W3_STARS[i].dec);
      if(!d.above) continue;
      sv[ns*4]=eye[0]+d.e*40000.f; sv[ns*4+1]=eye[1]+d.u*40000.f; sv[ns*4+2]=eye[2]-d.n*40000.f;
      sv[ns*4+3]=W3_STARS[i].mag; ns++;
    }
    if(ns>0){
      glEnable(GL_BLEND); glBlendFunc(GL_ONE,GL_ONE);
      glUseProgram(w3_gl.pStar); glUniformMatrix4fv(w3_gl.stMVP,1,GL_FALSE,C->mvp); glUniform1f(w3_gl.stDay,A->day);
      glBindBuffer(GL_ARRAY_BUFFER,w3_gl.starVBO); glBufferData(GL_ARRAY_BUFFER,(size_t)ns*16,sv,GL_DYNAMIC_DRAW);
      glEnableVertexAttribArray(w3_gl.stPos); glEnableVertexAttribArray(w3_gl.stMag);
      glVertexAttribPointer(w3_gl.stPos,3,GL_FLOAT,GL_FALSE,16,0);
      glVertexAttribPointer(w3_gl.stMag,1,GL_FLOAT,GL_FALSE,16,(void*)12);
      glDrawArrays(GL_POINTS,0,ns);
      glDisableVertexAttribArray(w3_gl.stPos); glDisableVertexAttribArray(w3_gl.stMag);
      glDisable(GL_BLEND);
    }
  }
}

#endif
