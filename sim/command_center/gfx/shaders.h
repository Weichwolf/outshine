/* FlightBox renderer — GLSL program sources and the compile/link helpers. The uniform/attribute
 * locations and program handles stay in world3d.h, where world3d_init wires them in one block. */
#ifndef W3_GFX_SHADERS_H
#define W3_GFX_SHADERS_H
static GLuint w3_shader(GLenum t, const char *src) {
  GLuint s = glCreateShader(t);
  glShaderSource(s, 1, &src, 0);
  glCompileShader(s);
  GLint ok;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char b[512];
    glGetShaderInfoLog(s, 512, 0, b);
    printf("shader err: %s\n", b);
  }
  return s;
}
static GLuint w3_prog(const char *vs, const char *fs) {
  GLuint p = glCreateProgram();
  glAttachShader(p, w3_shader(GL_VERTEX_SHADER, vs));
  glAttachShader(p, w3_shader(GL_FRAGMENT_SHADER, fs));
  glLinkProgram(p);
  return p;
}

/* uHaze = horizon/fog colour for the current time of day (set from sun elevation +
 * cloud on the CPU) so distant geometry fades into the real sky, day or night. */
static const char *W3_VSW = "attribute vec3 aPos; attribute vec3 aCol; uniform mat4 uMVP; varying "
                            "vec3 vCol; varying float vFog;"
                            "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vCol=aCol; "
                            "vFog=clamp(p.z/6000.0,0.0,1.0); }";
static const char *W3_FSW =
    "precision mediump float; varying vec3 vCol; varying float vFog; uniform vec3 uHaze; uniform "
    "float uLight;"
    "void main(){ gl_FragColor=vec4(mix(vCol*uLight,uHaze,vFog*0.7),1.0); }";
/* textured terrain: OSM landcover/roads/buildings baked to a per-tile ortho texture */
static const char *W3_VSWT =
    "attribute vec3 aPos; attribute vec2 aUV; attribute vec3 aNorm; uniform mat4 uMVP;"
    "varying vec2 vUV; varying float vFog; varying vec3 vNorm;"
    "void main(){ vec4 p=uMVP*vec4(aPos,1.0); gl_Position=p; vUV=aUV; vNorm=aNorm; "
    "vFog=clamp((p.w-3000.0)/28000.0,0.0,1.0); }";
/* Self-rendered lighting: the tile texture is treated as ALBEDO (base colour) and lit by
 * OUR sun (uSun, direction-to-sun in render space E=+X,up=+Y,N=-Z) via the terrain slope
 * normal, plus sky ambient — so relief/shadows track our real, dynamic sun instead of any
 * lighting baked into the texture. uLight carries the day->night level, uHaze the horizon. */
static const char *W3_FSWT =
    "precision mediump float; varying vec2 vUV; varying float vFog; varying vec3 vNorm;"
    "uniform sampler2D uTex; uniform vec3 uHaze; uniform float uLight; uniform vec3 uSun; uniform "
    "float uSunUp;"
    /* uSunUp = sin(sun elevation), passed separately so the day/night terminator does not read it
     * off uSun.y -- in the ECEF path uSun is an ECEF direction whose .y is not the local vertical.
     * On the ENU path uSunUp == uSun.y, so this is numerically identical there. */
    "void main(){ vec3 N=normalize(vNorm); float sup=smoothstep(-0.05,0.12,uSunUp);"
    "  float diff=max(0.0,dot(N,uSun))*sup;"
    "  float shade=0.55+0.65*diff;" /* 0.55 sky ambient .. +direct sun on lit slopes */
    "  vec3 c=texture2D(uTex,vUV).rgb*shade*uLight;"
    "  gl_FragColor=vec4(mix(c,uHaze,vFog*0.6),1.0); }";
static const char *W3_VSH =
    "attribute vec2 aPos; attribute vec3 aCol; uniform vec2 uScale; varying vec3 vCol;"
    "void main(){ gl_Position=vec4(aPos.x*uScale.x-1.0, 1.0-aPos.y*uScale.y, 0.0,1.0); vCol=aCol; "
    "}";
static const char *W3_FSH =
    "precision mediump float; varying vec3 vCol; void main(){ gl_FragColor=vec4(vCol,1.0); }";

/* ---- sky dome: fullscreen pass, per-pixel ray from the camera basis ----
 * Colours the whole sky by the real sun position (day / dusk / night gradient),
 * draws sun + moon discs and a night star field, and blends procedural clouds.
 * Drawn first each frame with depth writes off; terrain paints over it. */
static const char *W3_VSKY =
    "attribute vec2 aPos; uniform vec3 uF,uS,uU; uniform float uTan,uAsp; varying vec3 vRay;"
    "void main(){ vRay=uF + uS*(aPos.x*uTan*uAsp) + uU*(aPos.y*uTan); "
    "gl_Position=vec4(aPos,0.999,1.0); }";
static const char *W3_FSKY =
    "precision highp float; varying vec3 vRay;"
    "uniform vec3 uSun,uMoon; uniform float uMoonPh,uCloud,uSunDisc,uDayF,uDipSin;"
    "float h21(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }"
    "float vnoise(vec2 p){ vec2 i=floor(p),f=fract(p); f=f*f*(3.0-2.0*f);"
    "  float a=h21(i),b=h21(i+vec2(1,0)),c=h21(i+vec2(0,1)),d=h21(i+vec2(1,1));"
    "  return mix(mix(a,b,f.x),mix(c,d,f.x),f.y); }"
    /* uDipSin=sin(horizon dip): drop the horizon-referenced sky (gradient, dusk band, cloud deck)
     * by the dip so it meets the curved-away terrain edge instead of the flat local level. The
     * sun/moon discs stay on their true directions (r,uSun,uMoon), unshifted. */
    "void main(){ vec3 r=normalize(vRay); float hgt=clamp(r.y+uDipSin,-0.15,1.0);"
    "  float sEl=uSun.y; float day=uDayF;" /* unified daylight from atmo.h (w3_daylight), not
                                              recomputed */
    "  float t=pow(clamp(hgt,0.0,1.0),0.55);"
    "  vec3 dayC=mix(vec3(0.72,0.82,0.92),vec3(0.22,0.45,0.82),t);"
    "  vec3 ngtC=mix(vec3(0.05,0.06,0.13),vec3(0.01,0.02,0.06),t);"
    "  vec3 sky=mix(ngtC,dayC,day);"
    /* warm dusk band toward the sun, low on the horizon */
    "  float dusk=exp(-(sEl*sEl)/(0.18*0.18));" /* pow(neg,2.0) is undefined in GLES2 -> multiply */
    "  float low=exp(-(max(hgt,0.0)*max(hgt,0.0))/(0.20*0.20));"
    "  float tow=max(0.0,dot(normalize(vec3(r.x,0.0,r.z)),normalize(vec3(uSun.x,0.001,uSun.z))));"
    "  sky=mix(sky,vec3(0.98,0.46,0.20),dusk*low*(0.30+0.55*tow));"
    /* clouds: value-noise sheet projected on the dome, lit by day, silver by night */
    "  if(uCloud>0.01 && hgt>0.04){ vec2 cuv=r.xz/(hgt+0.25)*1.6;"
    "     float n=0.55*vnoise(cuv)+0.35*vnoise(cuv*2.3)+0.15*vnoise(cuv*4.7);"
    "     float cl=smoothstep(1.0-uCloud,1.0-uCloud*0.35,n)*smoothstep(0.04,0.22,hgt);"
    "     vec3 cc=mix(vec3(0.03,0.04,0.06),vec3(0.95,0.96,1.0),day);" /* dark night clouds (occlude,
                                                                         don't glow) -> stars read
                                                                         through */
    "     sky=mix(sky,cc,cl*0.85); }"
    /* (real stars are drawn as a separate GL_POINTS pass at their true alt/az) */
    /* moon disc + phase (visible when up, mostly at night) */
    "  float ma=length(cross(r,uMoon)); float md=smoothstep(0.012,0.006,ma);"
    "  float mb=(0.25+0.75*uMoonPh)*step(-0.03,uMoon.y)*(1.0-0.7*day);"
    "  sky=mix(sky,vec3(0.92,0.92,0.86),md*mb);"
    /* sun disc + glow (visible when up) */
    "  float sa=length(cross(r,uSun)); float sd=smoothstep(0.016,0.004,sa);"
    "  float glow=exp(-sa*7.0)*0.35 + exp(-sa*1.5)*0.12*day;"
    "  float sup=smoothstep(-0.06,0.0,uSun.y);"
    "  sky+=(sd*vec3(1.0,0.96,0.86)*2.2 + glow*vec3(1.0,0.80,0.55))*sup*uSunDisc;"
    "  gl_FragColor=vec4(sky,1.0); }";

/* ---- real stars: a GL_POINTS pass, each star placed at its true celestial direction ----
 * aBV is the catalogue B-V colour index; starColour maps it to the real spectral-class tint
 * (blue-white O/B .. white A .. yellow F/G .. orange K .. red M), computed per point in the vertex
 * shader and interpolated flat across the tiny sprite. */
static const char *W3_VSTAR =
    "attribute vec3 aPos; attribute float aMag; attribute float aBV; uniform mat4 uMVP; varying "
    "float vB; varying vec3 vCol;"
    "vec3 starColour(float bv){ float t=clamp(bv,-0.4,1.8);"
    "  vec3 "
    "blue=vec3(0.61,0.70,1.0),white=vec3(1.0,1.0,1.0),yellow=vec3(1.0,0.96,0.84),orange=vec3(1.0,0."
    "80,0.55),red=vec3(1.0,0.62,0.42);"
    "  if(t<0.0) return mix(blue,white,(t+0.4)/0.4);"
    "  if(t<0.6) return mix(white,yellow,t/0.6);"
    "  if(t<1.2) return mix(yellow,orange,(t-0.6)/0.6);"
    "  return mix(orange,red,(t-1.2)/0.6); }"
    "void main(){ gl_Position=uMVP*vec4(aPos,1.0);"
    /* Magnitude drives BRIGHTNESS, not size: a star is a point source. gl_PointSize stays small and
     * is hard-capped (~1 px faint .. ~2.6 px for the very brightest) so bright stars read brighter,
     * never as discs; the old 1.0+2.4*b let the brightest bloom into a 4 px blob. */
    "  float b=clamp(1.45-0.42*aMag,0.12,1.5);" /* brighter (lower magnitude) -> more intense */
    "  gl_PointSize=clamp(1.0+1.05*b,1.0,2.6); vB=b; vCol=starColour(aBV); }";
static const char *W3_FSTAR =
    "precision mediump float; varying float vB; varying vec3 vCol; uniform float uDay;"
    "void main(){ vec2 pc=gl_PointCoord-0.5; float d=dot(pc,pc);"
    "  float a=smoothstep(0.25,0.0,d)*vB*(1.0-uDay);" /* soft round point, fades out toward day */
    "  gl_FragColor=vec4(vCol*a, a); }";
#endif
