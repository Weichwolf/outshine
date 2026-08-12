#include "BuildingDraw.h"
#include "SceneTargets.h"
#include <cmath>
#include "ChunkVtx.h"
#include "FacadeUv.h"
#include "SceneScale.h"
#include "SurfaceState.h"
/* NO CLOUD INFLUENCE ON LIT SURFACES. Owner, 2026-08-07: the deck neither shadows nor dims the
 * ground for now, so both transmittances are 1 and the whole CloudShadow/CloudDensity splice is
 * gone from this stage. The cloud pass still DRAWS the deck; it just does not light through it. */
#include "ShadowSample.h"
#include "SurfaceLight.h"
#include <string>

namespace outshine::Render {

static const char *kBuildingWGSL = R"(
/* anc.xyz = anchor - camera (m); sun.xyz = ECEF direction to the sun, sun.w = the daylight factor;
 * up.xyz = geodetic up at the anchor, up.w = the ambient night floor this frame. */
struct B { mvp : mat4x4f, anc : vec4f, sun : vec4f, up : vec4f };
@group(0) @binding(0) var<uniform> b : B;
@group(0) @binding(1) var<storage, read> I : Irr;
@group(0) @binding(2) var<uniform> C : Csm;
@group(0) @binding(3) var shMap : texture_depth_2d;
@group(0) @binding(4) var shSamp : sampler_comparison;

/* [SET] REFLECTANCES, not display colours: these are what litRadiance multiplies, so an authored
 * colour here would be off scale everywhere downstream.
 *   render   0.40-0.55 for lime and mineral render; 0.46 is the middle of a weathered Altstadt wall
 *   pantile  a red-clay pantile measures 0.10-0.15 with the red dominant
 *   deck     bitumen and gravel on a flat roof, 0.08-0.12, grey
 *   stone    a sandstone sill or a parapet coping, 0.32
 *   brick    a chimney stack, 0.13 with the red dominant
 *   zinc     the plant on a roof, 0.22, neutral
 *   glass    what comes back through the pane from an unlit room; the PANE's own return is the
 *            Fresnel term below, which is what actually makes a window read as glass */
const kRender : vec3f = vec3f(0.46, 0.43, 0.385);
const kPantile : vec3f = vec3f(0.145, 0.072, 0.050);
/* [SET] the two other roofs a German town carries: a Rheinland slate at 0.055 neutral-blue, and a
 * grey concrete interlocking tile at 0.11. Which one a roof is, is the building's own draw. */
const kSlate : vec3f = vec3f(0.055, 0.058, 0.066);
const kConcreteTile : vec3f = vec3f(0.115, 0.112, 0.108);
const kDeck : vec3f = vec3f(0.095, 0.092, 0.088);
const kStone : vec3f = vec3f(0.34, 0.325, 0.295);
const kBrick : vec3f = vec3f(0.135, 0.078, 0.062);
const kZinc : vec3f = vec3f(0.22, 0.225, 0.23);
const kGlass : vec3f = vec3f(0.030, 0.033, 0.036);
/* [SET] a painted frame is 0.68-0.72, a pale blind or net curtain returns about a third of what
 * falls on it, a stained timber door about a tenth. */
const kFrame : vec3f = vec3f(0.70, 0.69, 0.665);
const kBlind : vec3f = vec3f(0.33, 0.325, 0.31);
const kDoor : vec3f = vec3f(0.105, 0.070, 0.048);
/* [SET] the made ground. A worn concrete flag is 0.30-0.35, a kerb stone is cast and lighter until
 * it weathers, asphalt off a carriageway is 0.07-0.10. */
const kFlag : vec3f = vec3f(0.30, 0.295, 0.285);
const kKerbStone : vec3f = vec3f(0.335, 0.330, 0.318);
/* [SET] what a double-glazed unit passes at normal incidence. The German norm since 1995 is a
 * sealed unit at 0.78-0.82, and it is what stands between the slot and the room behind it. */
const kPaneThru : f32 = 0.82;

/* [SET] A WINDOW IS A HOLE 0.14 m DEEP. That is the reveal of a rendered masonry wall with the frame
 * set back behind the outer leaf; it is the number the parallax marches against, and the reveal it
 * exposes is lit by the same sun as the wall — which is why the shadow inside an opening moves with
 * the sun instead of being drawn into it. */
const kRecessM : f32 = 0.14;
/* The metres one unit of uv is worth. The uv carries STOREYS and BAYS so that one rhythm serves
 * every building, and these two turn a depth in metres back into that measure. They are nominal: a
 * building whose storey is 3.4 m gets a recess that reads 0.12 m instead of 0.14, which is under the
 * width of the frame it sits in. */
const kFloorNominalM : f32 = 2.90;
const kBayNominalM : f32 = 3.10;

fn hash13(p : vec3f) -> f32 {
  var q = fract(p * vec3f(0.1031, 0.1030, 0.0973));
  q += vec3f(dot(q, q.yzx + 33.33));
  return fract((q.x + q.y) * q.z);
}

fn vnoise(p : vec3f) -> f32 {
  let i = floor(p);
  let f = p - i;
  let w = f * f * (3.0 - 2.0 * f);
  let a = mix(hash13(i + vec3f(0,0,0)), hash13(i + vec3f(1,0,0)), w.x);
  let bb = mix(hash13(i + vec3f(0,1,0)), hash13(i + vec3f(1,1,0)), w.x);
  let c = mix(hash13(i + vec3f(0,0,1)), hash13(i + vec3f(1,0,1)), w.x);
  let d = mix(hash13(i + vec3f(0,1,1)), hash13(i + vec3f(1,1,1)), w.x);
  return mix(mix(a, bb, w.y), mix(c, d, w.y), w.z);
}

/* THE BUILDING'S OWN DRAW. Colour, roof material and weathering used to be smooth functions of
 * PLACE, so two neighbours a wall apart came out the same building twice; the identity rides in the
 * vertex (core/FacadeUv.h) and is hashed from the footprint's own corner, so it is a fact about
 * there that reproduces and is discontinuous exactly where the buildings are. */
fn identOf(ident : i32, stream : f32) -> f32 {
  return hash13(vec3f(f32(ident) * 0.6180339 + 0.117, stream, 3.71));
}

struct BOut { @builtin(position) pos : vec4f, @location(0) nrm : vec3f, @location(1) uvb : vec2f,
              @location(2) loc : vec3f };

/* `loc` is the position relative to the mesh ANCHOR, not to the camera: a grain that is a function
 * of where the wall is must not move when the eye does, and the camera-relative position the
 * lighting needs is one add away. */
@vertex fn vs(@location(0) p : vec3f, @location(1) uvw : vec2f, @location(2) nb : vec3f) -> BOut {
  var o : BOut;
  o.pos = b.mvp * vec4f(p + b.anc.xyz, 1.0);
  o.nrm = nb;
  o.uvb = uvw;
  o.loc = p;
  return o;
}

/* WHAT THE FRAGMENT TURNED OUT TO BE. The facade decides all five, because a window's glass, its
 * reveal and the wall around it are one function of one uv and differ in every one of them. `sun` is
 * how much of the sun the opening's OWN geometry lets through — a head and a jamb occlude a pane
 * exactly as a wall occludes a street, and it multiplies the cascade's visibility rather than the
 * albedo, because that is what it is. */
struct Skin { alb : vec3f, nrm : vec3f, mirror : f32, ao : f32, sun : f32 };

/* THE OPENING, marched rather than drawn. The view ray is followed into a rectangular box sunk into
 * the wall: if it reaches the back it is the pane and its frame, and if it meets a side first it is
 * that side's reveal, with that side's normal. Nothing here is painted — the darkness inside an
 * opening is the reveal turning away from the sun, computed by the same lighting as the wall it is
 * cut into, and the sill shadow moves when the sun does. */
struct Hole { x0 : f32, x1 : f32, y0 : f32, y1 : f32, door : f32, blind : f32 };

/* THE OPENING SHADOWS ITSELF, and until now it did not: the march ran along the VIEW only, so at
 * near-normal incidence no side was ever hit, every fragment was the pane, and a window was a flat
 * rectangle of one value. Marching the same box along the LIGHT is the self-shadowing extension of
 * relief mapping (Policarpo & Oliveira, I3D 2005; RTR4 6.8.4-6.8.5) — and on a BOX it needs no march
 * at all: the ray from a point at depth `depth` reaches the wall plane after travelling exactly that
 * far, so one test of where it lands says whether the head, the jamb or the sill cut it. */
fn openingSun(at : vec2f, depth : f32, h : Hole, lx : f32, ly : f32) -> f32 {
  let q = vec2f(at.x + lx * depth, at.y + ly * depth);
  /* One centimetre of a bay is the softening: narrower is aliasing, wider is a painted gradient. */
  let e = 0.004;
  let sx = smoothstep(h.x0 - e, h.x0 + e, q.x) * (1.0 - smoothstep(h.x1 - e, h.x1 + e, q.x));
  let sy = smoothstep(h.y0 - e, h.y0 + e, q.y) * (1.0 - smoothstep(h.y1 - e, h.y1 + e, q.y));
  return sx * sy;
}

fn opening(cell : vec2f, h : Hole, sx : f32, sy : f32, lx : f32, ly : f32, tan : vec3f,
           bit : vec3f, nrm : vec3f, skin : ptr<function, Skin>) -> bool {
  if (cell.x < h.x0 || cell.x > h.x1 || cell.y < h.y0 || cell.y > h.y1) { return false; }
  var t = kRecessM;
  var side = 0;
  if (sx > 1.0e-5) { let q = (h.x1 - cell.x) / sx; if (q < t) { t = q; side = 1; } }
  if (sx < -1.0e-5) { let q = (h.x0 - cell.x) / sx; if (q < t) { t = q; side = 2; } }
  if (sy > 1.0e-5) { let q = (h.y1 - cell.y) / sy; if (q < t) { t = q; side = 3; } }
  if (sy < -1.0e-5) { let q = (h.y0 - cell.y) / sy; if (q < t) { t = q; side = 4; } }
  /* How much sky a point that far down a slot still sees: an opening 1.2 m across with a 0.14 m
   * reveal loses roughly that fraction of its hemisphere by the time it reaches the back. */
  let shut = 1.0 - 0.62 * clamp(1.0 - t / kRecessM, 0.0, 1.0);
  let at = vec2f(cell.x + sx * t, cell.y + sy * t);
  (*skin).sun = openingSun(at, t, h, lx, ly);
  if (side != 0) {
    (*skin).alb = kRender * 1.06;   /* a reveal is the same render, newer, out of the weather */
    (*skin).mirror = 0.0;
    (*skin).ao = shut;
    if (side == 1) { (*skin).nrm = -tan; }
    else if (side == 2) { (*skin).nrm = tan; }
    else if (side == 3) { (*skin).nrm = -bit; }
    else { (*skin).nrm = bit; }
    return true;
  }
  let f = vec2f((at.x - h.x0) / (h.x1 - h.x0), (at.y - h.y0) / (h.y1 - h.y0));
  let frame = f.x < 0.08 || f.x > 0.92 || f.y < 0.06 || f.y > 0.94 || abs(f.x - 0.5) < 0.028;
  (*skin).nrm = nrm;
  (*skin).ao = shut;
  if (frame) {
    (*skin).alb = kFrame;
    (*skin).mirror = 0.0;
    return true;
  }
  if (h.door > 0.5) {
    (*skin).alb = kDoor;
    (*skin).mirror = 0.10;
    return true;
  }
  /* WHAT IS BEHIND THE PANE, and it is not one thing: an unlit room returns almost nothing, a pale
   * blind returns a third of what falls on it. That difference along a facade is what stops a row of
   * windows reading as a row of identical black rectangles. A blind sits BEHIND the glass, so it is
   * the deepest thing in the slot and sees the least sky — it used to be given MORE than the reveal
   * above it and read as a pale panel stuck on the wall. */
  (*skin).alb = mix(kGlass, kBlind, h.blind);
  (*skin).mirror = 1.0 - 0.45 * h.blind;
  (*skin).ao = shut * kPaneThru;
  return true;
}

/* THE WALL. uv.x carries the style and the bay coordinate, uv.y the identity and the storey
 * (core/FacadeUv.h), so the rhythm below is one function for the whole town and every building's own
 * storey height, bay width and colour are already in the geometry it was written from. */
fn facade(uv : vec2f, nrm : vec3f, upv : vec3f, view : vec3f, sun : vec3f, loc : vec3f) -> Skin {
  let code = i32(floor(uv.x / kBayCeil));
  let bayx = uv.x - kBayCeil * f32(code);
  let style = code % kStyleCount;
  let stand = code / kStyleCount;
  let ident = i32(floor(uv.y / kStoreyCeil));
  let storey = uv.y - kStoreyCeil * f32(ident) - 1.0;

  var s : Skin;
  s.nrm = nrm;
  s.mirror = 0.0;
  s.ao = 1.0;
  s.sun = 1.0;

  let bit = normalize(upv - nrm * dot(nrm, upv));
  let tan = normalize(cross(bit, nrm));
  let vn = max(dot(view, nrm), 1.0e-3);
  let sx = -(dot(view, tan) / vn) / kBayNominalM;
  let sy = -(dot(view, bit) / vn) / kFloorNominalM;
  let sn = max(dot(sun, nrm), 1.0e-3);
  let lx = -(dot(sun, tan) / sn) / kBayNominalM;
  let ly = -(dot(sun, bit) / sn) / kFloorNominalM;
  let cell = vec2f(fract(bayx), fract(storey));

  /* WHICH OPENING THIS ONE IS. dot(nrm, loc) is the plane of this face, exactly constant over the
   * whole of it, so two buildings with the same rhythm still get different windows and no boundary
   * can ever run THROUGH one — which a grid on the fragment's own position does. */
  let plane = round(dot(nrm, loc) * 0.5) + round(dot(nrm, vec3f(31.7, 17.3, 7.1)) * 4.0);
  let id = vec3f(floor(bayx), floor(storey), plane + f32(ident) * 0.37);
  let r2 = hash13(id + vec3f(7.7, 3.1, 5.3));
  let ground = storey < 1.0 && storey >= 0.0;

  var h : Hole;
  h.blind = smoothstep(0.55, 0.95, r2);
  h.door = 0.0;
  h.x0 = 0.30; h.x1 = 0.70; h.y0 = 0.32; h.y1 = 0.80;
  if (style == kBlock || style == kTower) { h.x0 = 0.22; h.x1 = 0.78; h.y0 = 0.28; h.y1 = 0.82; }
  if (style == kSpire) { h.x0 = 0.40; h.x1 = 0.60; h.y0 = 0.22; h.y1 = 0.88; }
  if (style == kHall) {
    /* A hall is not storeyed: it opens in one band of clerestory glazing under the eaves and is
     * otherwise sheet. The band is where the storey the geometry gave it happens to fall. */
    h.x0 = 0.10; h.x1 = 0.90; h.y0 = 0.62; h.y1 = 0.84;
    h.blind = 0.0;
  }
  if (style == kOutbuilding) { h.x0 = 0.38; h.x1 = 0.62; h.y0 = 0.42; h.y1 = 0.70; }
  if (ground) {
    /* A SHOPFRONT IS A FRONT. It used to be given to every wall of a class, so the back of a block
     * opened onto its yard as widely as its street elevation did; the standing comes off the wall
     * that was measured against the nearest carriageway. */
    if (stand >= kFront && (style == kBlock || style == kTerrace || style == kTower)) {
      h.x0 = 0.14; h.x1 = 0.86; h.y0 = 0.14; h.y1 = 0.78;
      h.blind = h.blind * 0.4;
    } else if (style != kHall && style != kSpire) {
      h.y0 = 0.30; h.y1 = 0.76;
    }
    /* THE DOOR IS THE ENTRANCE BAY, and it is one bay wide because the mesher cut it out of the
     * face. A hash over the rhythm put it on whichever wall the numbers fell on, including the one
     * facing the back garden. */
    if (stand == kEntrance) {
      h.x0 = 0.36; h.x1 = 0.64; h.y0 = 0.02; h.y1 = 0.70; h.door = 1.0;
    }
  }
  /* The opening is tested before the render is mixed: a pane, a frame and a reveal use none of the
   * wall's noise, and a facade is about a third openings. */
  if (opening(cell, h, sx, sy, lx, ly, tan, bit, nrm, &s)) { return s; }

  /* Render is mixed on site out of local sand, and WHICH mix a house got is the house's own choice:
   * value spreads about a third either way, and the hue runs with it because the pigment that
   * darkens it is the same iron that makes it ochre. */
  let grain = vnoise(loc * 14.0);
  let weather = vnoise(loc * 0.9 + vec3f(2.7, 5.1, 1.3));
  let tint = identOf(ident, 1.0);
  let hue = tint * tint;
  /* A block or a tower is panel and concrete, greyer and flatter than a rendered house; an
   * outbuilding is unrendered masonry. */
  var base = kRender;
  if (style == kBlock || style == kTower) { base = kRender * vec3f(0.86, 0.88, 0.92); }
  if (style == kHall) { base = kRender * vec3f(0.78, 0.80, 0.83); }
  if (style == kOutbuilding) { base = kBrick * 2.2; }
  var wall = base * (0.58 + 0.80 * tint);
  wall = vec3f(wall.r * (0.90 + 0.26 * hue), wall.g * (0.97 + 0.04 * hue),
               wall.b * (1.10 - 0.32 * hue));
  s.alb = wall * (0.93 + 0.14 * grain) * (0.94 + 0.12 * weather);

  /* The sill under the opening, as relief and not as a line: it is stone, it stands proud of the
   * wall, and what makes it visible is that its top faces the sky and the wall does not. */
  let onSill = h.door < 0.5 && cell.x > h.x0 - 0.03 && cell.x < h.x1 + 0.03 &&
               cell.y > h.y0 - 0.055 && cell.y < h.y0;
  if (onSill) {
    s.alb = kStone * (0.9 + 0.2 * grain);
    s.nrm = normalize(nrm * 0.55 + bit * 0.83);
  }
  return s;
}

fn roofSkin(kind : i32, ident : i32, nrm : vec3f, upv : vec3f, loc : vec3f) -> Skin {
  var s : Skin;
  s.nrm = nrm;
  s.mirror = 0.0;
  s.ao = 1.0;
  s.sun = 1.0;
  let grain = vnoise(loc * 9.0);
  let batch = identOf(ident, 5.0);   /* one roof is one firing, and the firing is the building's */
  if (kind == kRoofPitch) {
    /* Pantile coursing, 0.33 m to the course, read up the slope of the very plane being shaded — so
     * a hip and a gable course in their own directions with no second parameter. */
    let slope = normalize(upv - nrm * dot(nrm, upv));
    let up = dot(loc, slope);
    let across = dot(loc, normalize(cross(slope, nrm)));
    let course = abs(fract(up / 0.33) - 0.5) * 2.0;
    let roll = abs(fract(across / 0.24) - 0.5) * 2.0;
    let relief = 0.86 + 0.20 * course * course + 0.10 * roll;
    /* WHICH ROOF THIS IS, and it is the building's draw and not the place's: a clay pantile street
     * with one slate roof in it is a German town, a street of one clay is a rendering of one. */
    let draw = identOf(ident, 7.0);
    var cover = kPantile;
    if (draw > 0.78) { cover = kSlate; }
    else if (draw > 0.58) { cover = kConcreteTile; }
    s.alb = cover * (0.72 + 0.56 * batch) * relief * (0.93 + 0.14 * grain);
    /* The lap between two courses is a step, so the normal tips over it rather than being shaded. */
    s.nrm = normalize(nrm + slope * (0.22 * (fract(up / 0.33) - 0.5)));
  } else if (kind == kRoofFlat) {
    s.alb = kDeck * (0.86 + 0.30 * grain);
  } else if (kind == kSoffit) {
    s.alb = kRender * 0.86;      /* the soffit is the same render, and it is the shadow that shows */
    s.ao = 0.55;
  } else if (kind == kLedge) {
    s.alb = kStone * (0.92 + 0.16 * grain);
  } else if (kind == kParapet) {
    s.alb = kRender * (0.58 + 0.80 * identOf(ident, 1.0)) * (0.93 + 0.14 * grain);
  } else if (kind == kPlinth) {
    /* THE BASE COURSE, and it is stone rather than render because splash off the pavement destroys
     * lime. Darker than the wall over it because it is wet more often, and coursed, so the relief
     * the geometry gives it is read by a joint the shading agrees with. */
    let joint = abs(fract(dot(loc, upv) / 0.28) - 0.5) * 2.0;
    s.alb = kStone * (0.52 + 0.22 * identOf(ident, 1.0)) * (0.88 + 0.20 * grain) *
            (0.86 + 0.20 * joint);
  } else if (kind == kKerb) {
    /* A kerb stone is 1 m long and its joints are what say so at a distance where the upstand has
     * already collapsed to a line. */
    let joint = abs(fract(dot(loc, normalize(cross(upv, nrm))) / 1.0) - 0.5) * 2.0;
    s.alb = kKerbStone * (0.90 + 0.16 * grain) * (0.90 + 0.16 * joint);
  } else if (kind == kPavement) {
    /* Concrete flags, 0.30 m square, laid across the footway. The band is bounded by the building
     * line and the kerb, so its width is where the house stands and not a number. */
    let e = dot(loc, normalize(cross(upv, vec3f(0.577, 0.577, 0.577))));
    let n = dot(loc, normalize(cross(upv, normalize(cross(upv, vec3f(0.577, 0.577, 0.577))))));
    let jx = abs(fract(e / 0.30) - 0.5) * 2.0;
    let jy = abs(fract(n / 0.30) - 0.5) * 2.0;
    let joint = max(jx, jy);
    s.alb = kFlag * (0.88 + 0.22 * vnoise(loc * 2.7)) * (0.90 + 0.16 * joint) *
            (0.94 + 0.12 * grain);
  } else if (kind == kTrim) {
    let course = abs(fract(dot(loc, upv) / 0.075) - 0.5) * 2.0;
    s.alb = kBrick * (0.82 + 0.28 * grain) * (0.90 + 0.18 * course);
  } else {
    s.alb = kZinc * (0.94 + 0.10 * grain);
    s.mirror = 0.25;
  }
  return s;
}

struct BFrag { @location(0) col : vec4f, @location(1) vel : vec2f };
@fragment fn fs(in : BOut) -> BFrag {
  let rel = in.loc + b.anc.xyz;
  let nrmB = normalize(in.nrm);
  let upB = normalize(b.up.xyz);
  let view = normalize(-rel);
  let sunB = normalize(b.sun.xyz);
  let face = i32(round(-in.uvb.x));
  let kind = select(0, face % kFacadeStride, in.uvb.x < 0.0);
  var s : Skin;
  if (in.uvb.x >= 0.0) { s = facade(in.uvb, nrmB, upB, view, sunB, in.loc); }
  else { s = roofSkin(kind, face / kFacadeStride, nrmB, upB, in.loc); }

  let sunVis = csmSunVis(shMap, shSamp, C, rel, nrmB, sunB) * s.sun;
  var o : BFrag;
  var col = litRadiance(I, s.alb * s.ao, 1.0, s.nrm, upB, sunB, sunVis, 1.0, 1.0, b.up.w);
  /* GLASS IS A MIRROR BEFORE IT IS A SURFACE. Schlick against the pane's own normal: at normal
   * incidence 4 % of the sky comes back and the room behind wins, at a grazing angle nearly all of
   * it does — which is the whole reason a row of windows reads as glass and not as dark paint. */
  if (s.mirror > 0.0) {
    let f = 0.04 + 0.96 * pow(1.0 - max(dot(view, s.nrm), 0.0), 5.0);
    let mirrorDir = reflect(-view, s.nrm);
    let glint = pow(max(dot(mirrorDir, sunB), 0.0), 900.0) * sunVis;
    let env = (I.sky.xyz * kInvPi + I.sun.xyz * glint) * kSceneExposure;
    col = vec4f(mix(col.xyz, env, f * s.mirror), col.w);
  }
  o.col = col;
  /* A building is world-fixed, so its motion is the camera's and the resolve gets that from depth.
   * The write is not redundant: this stage draws AFTER the ground cover and a facade in front of a
   * blade would otherwise keep the blade's velocity. */
  o.vel = vec2f(kVelStatic);
  return o;
}
)";

void BuildingDraw::Configure(const Gpu &gpu, const SceneLight &light) {
  Device = gpu.Device;
  Queue = gpu.Queue;
  Light = light;

  const std::string src = std::string(kSceneScaleWGSL) + kSurfaceLightWGSL + ShadowSampleWGSL()
                        + kVelocityWGSL + FacadeUvWGSL() + kBuildingWGSL;
  wgpu::ShaderSourceWGSL wsl{};
  wsl.code = src.c_str();
  wgpu::ShaderModuleDescriptor smd{};
  smd.nextInChain = &wsl;
  wgpu::ShaderModule m = Device.CreateShaderModule(&smd);

  wgpu::VertexAttribute attr[3] = {};
  attr[0].format = wgpu::VertexFormat::Float32x3; attr[0].offset = 0;  attr[0].shaderLocation = 0;
  attr[1].format = wgpu::VertexFormat::Float32x2; attr[1].offset = 12; attr[1].shaderLocation = 1;
  attr[2].format = wgpu::VertexFormat::Float32x3; attr[2].offset = 20; attr[2].shaderLocation = 2;
  wgpu::VertexBufferLayout vbl{};
  vbl.arrayStride = kVertexStrideB;
  vbl.attributeCount = 3;
  vbl.attributes = attr;

  wgpu::ColorTargetState ct{};
  ct.format = gpu.HdrFormat;

  wgpu::DepthStencilState ds{};
  ds.format = wgpu::TextureFormat::Depth32Float;
  ds.depthCompare = wgpu::CompareFunction::Greater;

  wgpu::RenderPipelineDescriptor rp{};
  rp.vertex.module = m;
  rp.vertex.bufferCount = 1;
  rp.vertex.buffers = &vbl;
  wgpu::FragmentState fs{};
  fs.module = m;
  wgpu::ColorTargetState cts[2] = {ct, VelocityTarget(true)};
  fs.targetCount = 2;
  fs.targets = cts;
  rp.fragment = &fs;
  rp.depthStencil = &ds;
  /* THE STATE COMES OFF THE DERIVATION TABLE. A building is opaque and writes depth, and its winding
   * is now TRUSTED: the generator orients every outline counter-clockwise before it raises a wall,
   * so a back face is a face nobody can see and the fragment stage never runs on one. */
  static constexpr Material kMasonry{};
  static constexpr SurfaceState kState = StateOf(kMasonry);
  ds.depthWriteEnabled = kState.WritesDepth();
  rp.primitive.cullMode = CullsBackFaces(kState, Winding::Trusted) ? wgpu::CullMode::Back
                                                                   : wgpu::CullMode::None;
  Pipe = Device.CreateRenderPipeline(&rp);

  wgpu::BufferDescriptor bd{};
  bd.size = kUniFloats * sizeof(float);
  bd.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  Uni = Device.CreateBuffer(&bd);

  wgpu::BindGroupEntry be[5] = {};
  be[0].binding = 0; be[0].buffer = Uni; be[0].size = kUniFloats * sizeof(float);
  be[1].binding = 1; be[1].buffer = Light.Irradiance; be[1].size = wgpu::kWholeSize;
  be[2].binding = 2; be[2].buffer = Light.Cascades;   be[2].size = kShadowUniFloats * sizeof(float);
  be[3].binding = 3; be[3].textureView = Light.ShadowAtlas;
  be[4].binding = 4; be[4].sampler = Light.ShadowCompare;
  wgpu::BindGroupDescriptor bg{};
  bg.layout = Pipe.GetBindGroupLayout(0);
  bg.entryCount = 5;
  bg.entries = be;
  Bind = Device.CreateBindGroup(&bg);
}

void BuildingDraw::SetMesh(const float *verts, uint32_t nverts, const uint32_t *idx, uint32_t nidx,
                             const DagCluster *clusters, int nclusters, const double anchor[3]) {
  NVerts = nverts;
  NIdx = nidx;
  Clusters.assign(clusters, clusters + (nclusters > 0 ? nclusters : 0));
  if (Clusters.empty() && nverts > 0) {
    DagCluster c{};
    c.Count = nidx;
    c.ParentErr = kDagRootErr;
    BoundingSphere(verts, nverts, 8, c.SelfCenter, &c.SelfRadius);
    Clusters.push_back(c);
  }
  BaseVerts = 0;
  for (const DagCluster &c : Clusters)
    if (c.Level == 0) BaseVerts = std::max(BaseVerts, c.First + c.Count);
  for (int i = 0; i < 3; i++) Anchor[i] = anchor[i];
  if (nverts == 0 || !Device) return;
  if (Cap < nverts) {
    wgpu::BufferDescriptor bd{};
    bd.size = (uint64_t)nverts * 8 * sizeof(float);
    bd.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    Vtx = Device.CreateBuffer(&bd);
    Cap = nverts;
  }
  Queue.WriteBuffer(Vtx, 0, verts, (size_t)nverts * 8 * sizeof(float));
  if (IdxCap < nidx) {
    wgpu::BufferDescriptor bd{};
    bd.size = (uint64_t)nidx * sizeof(uint32_t);
    bd.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    Idx = Device.CreateBuffer(&bd);
    IdxCap = nidx;
  }
  Queue.WriteBuffer(Idx, 0, idx, (size_t)nidx * sizeof(uint32_t));
}

void BuildingDraw::SetSun(const double sunEcef[3], const double up[3], float nightAmbient) {
  for (int i = 0; i < 3; i++) { SunDir[i] = sunEcef[i]; Up[i] = up[i]; }
  NightAmbient = nightAmbient;
}

void BuildingDraw::Encode(const FrameContext &ctx, ClusterCut &cut,
                          wgpu::RenderPassEncoder &pass) {
  DrawnVerts = 0;
  if (NVerts == 0 || !Vtx) return;

  const double eyeLocal[3] = {ctx.Eye[0] - Anchor[0], ctx.Eye[1] - Anchor[1], ctx.Eye[2] - Anchor[2]};
  const double rel[3] = {-eyeLocal[0], -eyeLocal[1], -eyeLocal[2]};
  /* A prism has no up: the DAG measured its error as a nearest-point distance, which is already the
   * length in every direction. */
  static const float kNoUp[3] = {0.0f, 0.0f, 0.0f};
  cut.Begin();
  cut.Take(0, Clusters.data(), (int)Clusters.size(), eyeLocal, rel, kNoUp);
  cut.Close();
  DrawnVerts = (uint32_t)cut.Indices();
  if (cut.Ranges().empty()) return;
  float u[kUniFloats] = {};
  for (int i = 0; i < 16; i++) u[i] = ctx.Mvp20[i];
  u[16] = (float)(Anchor[0] - ctx.Eye[0]);
  u[17] = (float)(Anchor[1] - ctx.Eye[1]);
  u[18] = (float)(Anchor[2] - ctx.Eye[2]);
  u[19] = 0.0f;
  u[20] = (float)SunDir[0]; u[21] = (float)SunDir[1]; u[22] = (float)SunDir[2]; u[23] = 0.0f;
  u[24] = (float)Up[0]; u[25] = (float)Up[1]; u[26] = (float)Up[2]; u[27] = NightAmbient;
  Queue.WriteBuffer(Uni, 0, u, sizeof u);

  pass.SetPipeline(Pipe);
  pass.SetBindGroup(0, Bind);
  pass.SetVertexBuffer(0, Vtx);
  pass.SetIndexBuffer(Idx, wgpu::IndexFormat::Uint32);
  for (const ClusterCut::Range &r : cut.Ranges()) pass.DrawIndexed(r.Count, 1, r.First, 0, 0);
}

} // namespace outshine::Render
