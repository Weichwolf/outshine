/* FlightBox renderer — the HUD/OSD: regenerated every frame and drawn on top of the decoded video
 * (never encoded into it). Text is tile blits from the MAX7456 font atlas (FBMax7456.h); the geometry
 * here is only the LINE primitives (tape rails, ticks, carets, boxes, FPM ring, conformal horizon).
 */
#ifndef FBHUDSYMBOLOGY_H
#define FBHUDSYMBOLOGY_H
/* HUD line primitives (2D pixel coords), regenerated every frame (glBufferData DYNAMIC). */
static float w3_hud[131072]; /* line segments: rails/ticks/carets/boxes */
static int w3_hudN;
/* Filled TRIANGLES (same x,y,r,g,b): thin AA quads read as smooth lines at any angle (unlike GL_LINE,
 * which stair-steps when tilted). Used for the conformal horizon. */
static float w3_hudT[16384];
static int w3_hudTN;
static const char *W3_STN[] = {"DISARM", "ARMED", "CLIMB", "LOITER", "MANUAL", "RTH", "AUTO", "LOWLEVEL"};
static void w3_line(float x0, float y0, float x1, float y1, float r, float g, float b) {
  if (w3_hudN > 131052) return;
  w3_hud[w3_hudN++] = x0;
  w3_hud[w3_hudN++] = y0;
  w3_hud[w3_hudN++] = r;
  w3_hud[w3_hudN++] = g;
  w3_hud[w3_hudN++] = b;
  w3_hud[w3_hudN++] = x1;
  w3_hud[w3_hudN++] = y1;
  w3_hud[w3_hudN++] = r;
  w3_hud[w3_hudN++] = g;
  w3_hud[w3_hudN++] = b;
}
/* Text = tile blits from the MAX7456 font atlas (FBMax7456.h). */
static void w3_text(float x, float y, float s, float r, float g, float b, const char *t) {
  mx_text(x, y, s, r, g, b, t);
}
static void w3_printf(float x, float y, float s, float r, float g, float b, const char *fmt, ...) {
  char bb[96];
  va_list a;
  va_start(a, fmt);
  vsnprintf(bb, 96, fmt, a);
  va_end(a);
  mx_text(x, y, s, r, g, b, bb);
}

static void w3_qvert(float x, float y, float r, float g, float b) {
  if (w3_hudTN > 16374) return;
  w3_hudT[w3_hudTN++] = x;
  w3_hudT[w3_hudTN++] = y;
  w3_hudT[w3_hudTN++] = r;
  w3_hudT[w3_hudTN++] = g;
  w3_hudT[w3_hudTN++] = b;
}
/* A thin filled quad along (x0,y0)-(x1,y1), half-width hw -> a smooth AA line at any angle (two
 * tris). */
static void w3_qline(float x0, float y0, float x1, float y1, float hw, float r, float g, float b) {
  float dx = x1 - x0, dy = y1 - y0, L = sqrtf(dx * dx + dy * dy);
  if (L < 1e-3f) return;
  float px = -dy / L * hw, py = dx / L * hw;
  float ax = x0 + px, ay = y0 + py, bx = x1 + px, by = y1 + py, cx2 = x1 - px, cy2 = y1 - py,
        dx2 = x0 - px, dy2 = y0 - py;
  w3_qvert(ax, ay, r, g, b);
  w3_qvert(bx, by, r, g, b);
  w3_qvert(cx2, cy2, r, g, b);
  w3_qvert(ax, ay, r, g, b);
  w3_qvert(cx2, cy2, r, g, b);
  w3_qvert(dx2, dy2, r, g, b);
}
/* Approximate a circle as `seg` chords -- the Flight-Path-Marker ring. */
static void w3_circle(float cx, float cy, float rad, int seg, float r, float g, float b) {
  float px = cx + rad, py = cy;
  for (int i = 1; i <= seg; i++) {
    float a = (float)i / (float)seg * 6.2831853f;
    float x = cx + rad * cosf(a), y = cy + rad * sinf(a);
    w3_line(px, py, x, y, r, g, b);
    px = x;
    py = y;
  }
}
/* A rectangle outline -- the boxed current value on a tape. */
static void w3_box(float x0, float y0, float x1, float y1, float r, float g, float b) {
  w3_line(x0, y0, x1, y0, r, g, b);
  w3_line(x1, y0, x1, y1, r, g, b);
  w3_line(x1, y1, x0, y1, r, g, b);
  w3_line(x0, y1, x0, y0, r, g, b);
}
/* Full OSD. Font has no '%' (implied by label); colour = green good / amber caution / red warning. */
static void w3_build_hud(const FlightBox::FBState *t, int W, int H, int have) {
  w3_hudN = 0;
  w3_hudTN = 0;
  mx_reset();
  float cx = W / 2, cy = H / 2;
  const float RAD = (float)M_PI / 180.f, HG_R = 0.30f, HG_G = 1.0f,
              HG_B = 0.40f; /* monochrome HUD green */
  /* Waterline / boresight: the FIXED aircraft reference (nose / longitudinal axis), screen-locked.
   */
  w3_line(cx - 28, cy, cx - 10, cy, HG_R, HG_G, HG_B);
  w3_line(cx + 10, cy, cx + 28, cy, HG_R, HG_G, HG_B);
  w3_line(cx - 10, cy, cx, cy + 7, HG_R, HG_G, HG_B);
  w3_line(cx, cy + 7, cx + 10, cy, HG_R, HG_G, HG_B);
  if (!have) {
    w3_text(cx - 60, 30, 3, 1, 0.8f, 0.2f, "NO TELEMETRY");
    return;
  }

  /* Primary attitude field (MIL-STD-1787): FPM + one conformal horizon segment. Conformal vertical
   * scale K from the 80 deg FOV: an angle e above the boresight sits at cy - K*tan(e). */
  float K = (H * 0.5f) / tanf(40.f * RAD), pitch = t->pitch;
  /* Flight-Path-Marker: gamma = climb angle from vs/gs; pitch - gamma sets it below the waterline.
   * Horizontal drift not modelled -> centred. */
  float gamma = atan2f(t->vs, t->gs > 0.5f ? t->gs : 0.5f) / RAD;
  float fx = cx, fy = cy - K * tanf((gamma - pitch) * RAD);
  if (fy < cy - H * 0.45f) fy = cy - H * 0.45f;
  if (fy > cy + H * 0.45f) fy = cy + H * 0.45f;
  w3_circle(fx, fy, 7, 10, HG_R, HG_G, HG_B);
  w3_line(fx - 7, fy, fx - 18, fy, HG_R, HG_G, HG_B);
  w3_line(fx + 7, fy, fx + 18, fy, HG_R, HG_G, HG_B);
  w3_line(fx, fy - 7, fx, fy - 15, HG_R, HG_G, HG_B);
  /* Compact attitude bar: two short conformal segments flanking the waterline (gap +/-36 px, end
   * +/-86 px), tilting with roll from the SAME camera projection as the scene. Centre = horizon
   * point straight ahead (az=yaw); a second point at +20 deg gives the tilt. */
  {
    static const float HC0[3] = {0, 0, 0};
    w3_cam HC = w3_cam_from(t->yaw, t->pitch, t->roll, HC0, W3_FOV, (float)W / H, 1.f, 1000.f);
    float Kc = (H * 0.5f) / tanf(W3_FOV * 0.5f * RAD), p[2][2];
    /* Dip depends on height above the curvature reference (ellipsoid/ASL), NOT local ground
     * clearance -- AGL made the horizon breathe with terrain relief during a level loiter. */
    float dip = w3_horizon_dip_rad(t->alt > 1 ? t->alt : w3_agl), cd = cosf(dip), sd = sinf(dip);
    for (int k = 0; k < 2; k++) {
      float az = (t->yaw + (k ? 20.f : 0.f)) * RAD;
      float d[3] = {cd * sinf(az), -sd, -cd * cosf(az)}; /* horizon dipped below level by θ_dip */
      float xc = d[0] * HC.sr[0] + d[1] * HC.sr[1] + d[2] * HC.sr[2];
      float yc = d[0] * HC.up[0] + d[1] * HC.up[1] + d[2] * HC.up[2];
      float zc = d[0] * HC.f[0] + d[1] * HC.f[1] + d[2] * HC.f[2];
      if (zc < 0.05f) zc = 0.05f;
      p[k][0] = cx + Kc * xc / zc;
      p[k][1] = cy - Kc * yc / zc;
    }
    float ddx = p[1][0] - p[0][0], ddy = p[1][1] - p[0][1], LL = sqrtf(ddx * ddx + ddy * ddy);
    if (LL > 1.f) {
      float ux = ddx / LL, uy = ddy / LL, mx = p[0][0], my = p[0][1], half = 86.f, gap = 36.f;
      w3_qline(mx - ux * half, my - uy * half, mx - ux * gap, my - uy * gap, 1.0f, HG_R, HG_G,
               HG_B);
      w3_qline(mx + ux * gap, my + uy * gap, mx + ux * half, my + uy * half, 1.0f, HG_R, HG_G,
               HG_B);
    }
  }
  float hdg = t->yaw < 0 ? t->yaw + 360 : t->yaw;

  /* ===== Heading tape (top): moving scale centred on heading, boxed value + up-caret, home
   * pointer. Ticks every 5 deg, labels every 30 (N/03/06/E...). home_bearing is relative to the
   * nose, so on a nose-centred tape the home marker sits at that offset from centre. ===== */
  {
    float hpd = 5.f, hy1 = 40;
    /* Marks at TRUE continuous positions cx+(hh-hdg)*hpd -- a value-snapped grid makes labels blink
     * on/off as the tape scrolls. */
    for (int hh = (int)floorf((hdg - 45.f) / 5.f) * 5; hh <= (int)hdg + 45; hh += 5) {
      float sx = cx + ((float)hh - hdg) * hpd;
      if (sx < cx - 202.f || sx > cx + 202.f) continue;
      int hn = ((hh % 360) + 360) % 360;
      float tk = (hn % 30 == 0) ? 11.f : 6.f;
      w3_line(sx, hy1, sx, hy1 - tk, HG_R, HG_G, HG_B);
      if (hn % 30 == 0) {
        char nb[4];
        const char *L = nb;
        if (hn == 0)
          L = "N";
        else if (hn == 90)
          L = "E";
        else if (hn == 180)
          L = "S";
        else if (hn == 270)
          L = "W";
        else
          snprintf(nb, 4, "%02d", hn / 10);
        w3_text(sx - (L[1] ? 7.f : 3.f), hy1 - tk - 15, 2.f, HG_R, HG_G, HG_B, L);
      }
    }
    w3_line(cx - 200, hy1, cx + 200, hy1, HG_R, HG_G, HG_B);
    w3_line(cx - 7, hy1 + 7, cx, hy1, HG_R, HG_G, HG_B);
    w3_line(cx, hy1, cx + 7, hy1 + 7, HG_R, HG_G, HG_B); /* up-caret */
    w3_box(cx - 26, hy1 + 8, cx + 26, hy1 + 30, HG_R, HG_G, HG_B);
    w3_printf(cx - 22, hy1 + 13, 2.f, HG_R, HG_G, HG_B, "%03.0f", hdg);
    float hb = t->home_bearing;
    if (hb > 44) hb = 44;
    if (hb < -44) hb = -44;
    float hsx = cx + hb * hpd;
    w3_line(hsx, hy1 - 1, hsx - 6, hy1 - 11, HG_R, HG_G, HG_B);
    w3_line(hsx, hy1 - 1, hsx + 6, hy1 - 11, HG_R, HG_G, HG_B);
    w3_line(hsx - 6, hy1 - 11, hsx + 6, hy1 - 11, HG_R, HG_G, HG_B);
    w3_text(hsx - 3, hy1 - 25, 1.4f, HG_R, HG_G, HG_B, "H");
  }

  /* ===== Groundspeed tape (left): moving vertical scale, boxed value + caret.
   * TODO(doc/f16/hud-symbology.md): future stage switches this tape to CAS in knots; t->airspeed
   * (TAS) is already available in-process, unused here only because the tape's label is GS. =====
   */
  {
    float apx = 5.f, ax = 70.f, as = t->gs;
    for (int av = (int)floorf((as - 30.f) / 5.f) * 5; av <= (int)as + 30; av += 5) { /* continuous marks */
      if (av < 0) continue;
      float sy = cy - ((float)av - as) * apx;
      if (sy < cy - 150.f || sy > cy + 150.f) continue;
      float tk = (av % 10 == 0) ? 11.f : 6.f;
      w3_line(ax, sy, ax - tk, sy, HG_R, HG_G, HG_B);
      if (av % 10 == 0) w3_printf(ax - tk - 26, sy - 4, 1.7f, HG_R, HG_G, HG_B, "%3d", av);
    }
    w3_line(ax, cy - 150, ax, cy + 150, HG_R, HG_G, HG_B);
    w3_box(ax + 3, cy - 11, ax + 63, cy + 11, HG_R, HG_G, HG_B);
    w3_printf(ax + 9, cy - 7, 2.f, HG_R, HG_G, HG_B, "%3.0f", as);
    w3_line(ax, cy, ax + 3, cy - 6, HG_R, HG_G, HG_B);
    w3_line(ax, cy, ax + 3, cy + 6, HG_R, HG_G, HG_B); /* caret at the rail */
    w3_text(ax + 3, cy - 30, 1.4f, HG_R, HG_G, HG_B, "GS");
  }

  /* ===== Altitude tape (right): ASL scale + '<' caret; AGL (ASL - DEM ground) and VS below. ===== */
  {
    float mpx = 1.5f, axr = W - 70.f, asl = t->alt;
    for (int av = (int)floorf((asl - 100.f) / 10.f) * 10; av <= (int)asl + 100; av += 10) { /* continuous marks */
      if (av < 0) continue;
      float sy = cy - ((float)av - asl) * mpx;
      if (sy < cy - 150.f || sy > cy + 150.f) continue;
      float tk = (av % 20 == 0) ? 11.f : 6.f;
      w3_line(axr, sy, axr + tk, sy, HG_R, HG_G, HG_B);
      if (av % 20 == 0) w3_printf(axr + tk + 3, sy - 4, 1.6f, HG_R, HG_G, HG_B, "%3d", av);
    }
    w3_line(axr, cy - 150, axr, cy + 150, HG_R, HG_G, HG_B);
    w3_box(axr - 63, cy - 11, axr - 3, cy + 11, HG_R, HG_G, HG_B);
    w3_printf(axr - 58, cy - 7, 2.f, HG_R, HG_G, HG_B, "%3.0f", asl);
    w3_line(axr, cy, axr - 3, cy - 6, HG_R, HG_G, HG_B);
    w3_line(axr, cy, axr - 3, cy + 6, HG_R, HG_G, HG_B); /* < caret at the rail */
    w3_text(axr - 58, cy - 30, 1.4f, HG_R, HG_G, HG_B, "ASL");
    w3_printf(axr - 63, cy + 18, 1.6f, HG_R, HG_G, HG_B, "AGL%4.0f", w3_agl);
    w3_printf(axr - 63, cy + 36, 1.6f, HG_R, HG_G, HG_B, "VS%+4.0f", t->vs);
  }

  /* ===== Steering/glideslope cue, far LEFT edge (F-16 ILS style): err > 0 (above ideal path) puts
   * the caret BELOW centre -- fly down to it. README A7. |err| >= 90 = no approach mode active
   * (sentinel value) -> whole cue removed, not faked at centre (MIL-STD-1787). ===== */
  if (fabsf(t->glideslope_err) < 90.f) {
    float gx = 26.f, gsy = cy + t->glideslope_err * 7.f;
    if (gsy < cy - 40) gsy = cy - 40;
    if (gsy > cy + 40) gsy = cy + 40;
    w3_line(gx, cy - 42, gx, cy + 42, HG_R, HG_G, HG_B);
    for (int i = -2; i <= 2; i++)
      w3_line(gx - 4, cy + i * 20, gx + 4, cy + i * 20, HG_R, HG_G, HG_B); /* scale + centre tick */
    w3_qline(gx - 9, gsy - 5, gx, gsy, 1.2f, HG_R, HG_G, HG_B);
    w3_qline(gx, gsy, gx - 9, gsy + 5, 1.2f, HG_R, HG_G, HG_B);
    w3_qline(gx - 9, gsy + 5, gx - 9, gsy - 5, 1.2f, HG_R, HG_G, HG_B); /* caret -> scale */
    w3_text(gx - 8, cy + 48, 1.3f, HG_R, HG_G, HG_B, "GP");
  }
  /* ===== Secondary data + annunciations at the EDGES; the primary attitude field stays clean.
   * Green monochrome, only battery/link caution breaks to amber/red (MIL-STD allows it on mono). ===== */
  /* Flight-mode annunciation, bottom-left, boxed; LOITER (the only "away from pilot" mode this
   * autopilot has today) stands out in amber, same as RTH/failsafe would. */
  {
    int st = t->state == FlightBox::FBMode::Loiter ? 3
           : t->state == FlightBox::FBMode::LowLevel ? 7 : 4;   /* W3_STN index (LOITER/LOWLEVEL/MANUAL) */
    int rth = (st == 3 || st == 7);   /* auto modes annunciate amber (away-from-pilot) */
    float mr = rth ? 1.f : HG_R, mg = rth ? 0.85f : HG_G, mb = rth ? 0.2f : HG_B;
    w3_box(14, H - 42, 110, H - 16, mr, mg, mb);
    w3_printf(20, H - 37, 3.f, mr, mg, mb, "%s", W3_STN[st]);
  }
  /* Power + link, left edge above the mode box. Amber = caution, red = critical. */
  {
    float v = t->batt, r = HG_R, g = HG_G, b = HG_B;
    if (v < 10.f) {
      r = 1;
      g = 0.3f;
      b = 0.2f;
    } else if (v < 11.f) {
      r = 1;
      g = 0.8f;
      b = 0.2f;
    }
    w3_printf(14, H - 66, 2.f, r, g, b, "BAT %4.1fV", v);
  }
  {
    int q = t->rssi;
    float r = HG_R, g = HG_G, b = HG_B;
    if (q < 25) {
      r = 1;
      g = 0.3f;
      b = 0.2f;
    } else if (q < 50) {
      r = 1;
      g = 0.8f;
      b = 0.2f;
    }
    w3_printf(14, H - 86, 2.f, r, g, b, "LNK %3d", q);
  }
  /* Weather + sun/moon ephemeris, small, above -- night-flight planning. */
  w3_printf(14, H - 108, 1.6f, HG_R, HG_G, HG_B, "CLD %2.0f  VIS %3.1fKM", t->cloud * 100.f,
            t->vis / 1000.f);
  w3_printf(14, H - 126, 1.5f, HG_R, HG_G, HG_B, "SUN E%+3.0f A%3.0f  MOON E%+3.0f A%3.0f P%2.0f",
            t->sun_el, t->sun_az, t->moon_el, t->moon_az, t->moon_phase * 100.f);
  /* Vision source (SVS synthetic / EVS camera), top-right corner. */
  {
    int evs = (w3_ground.mode == W3_GROUND_PHOTO);
    w3_box(W - 70, 10, W - 14, 34, HG_R, HG_G, HG_B);
    w3_printf(W - 64, 14, 2.5f, HG_R, HG_G, HG_B, "%s", evs ? "EVS" : "SVS");
  }
  /* Waypoint block, bottom-right: placeholder wired to HOME (bearing + distance) until a real
   * waypoint system slots in. README A7. */
  {
    float bx = W - 200, by = H - 92;
    w3_box(bx, by, W - 14, H - 14, HG_R, HG_G, HG_B);
    w3_printf(bx + 8, by + 6, 2.f, HG_R, HG_G, HG_B, "WP 00 HOME");
    w3_printf(bx + 8, by + 26, 2.f, HG_R, HG_G, HG_B, "BRG %+4.0f", t->home_bearing);
    w3_printf(bx + 8, by + 46, 2.f, HG_R, HG_G, HG_B, "DIST %5.0f", t->home_dist);
  }
}
/* Draw the 2D HUD (line overlay) into the bound framebuffer at W×H pixel coords. */
static void world3d_render_hud(const FlightBox::FBState *t, int W, int H, int have) {
  glViewport(0, 0, W, H);
  glDisable(GL_DEPTH_TEST);
  w3_build_hud(t, W, H, have);
  glUseProgram(w3_gl.pH);
  glUniform2f(w3_gl.hScale, 2.0f / W, 2.0f / H);
  glEnableVertexAttribArray(w3_gl.hPos);
  glEnableVertexAttribArray(w3_gl.hCol);
  glBindBuffer(GL_ARRAY_BUFFER, w3_gl.hVBO);
  if (w3_hudTN > 0) { /* filled AA quads (conformal horizon) first, lines/text over */
    glBufferData(GL_ARRAY_BUFFER, w3_hudTN * 4, w3_hudT, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(w3_gl.hPos, 2, GL_FLOAT, GL_FALSE, 20, 0);
    glVertexAttribPointer(w3_gl.hCol, 3, GL_FLOAT, GL_FALSE, 20, (void *)8);
    glDrawArrays(GL_TRIANGLES, 0, w3_hudTN / 5);
  }
  glBufferData(GL_ARRAY_BUFFER, w3_hudN * 4, w3_hud, GL_DYNAMIC_DRAW);
  glVertexAttribPointer(w3_gl.hPos, 2, GL_FLOAT, GL_FALSE, 20, 0);
  glVertexAttribPointer(w3_gl.hCol, 3, GL_FLOAT, GL_FALSE, 20, (void *)8);
  glDrawArrays(GL_LINES, 0, w3_hudN / 5);
  mx_render(W, H); /* tile-blit text on top of the line primitives, same FBO */
}
#endif /* FBHUDSYMBOLOGY_H */
