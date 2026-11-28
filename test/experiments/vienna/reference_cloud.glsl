uniform int cloudKind;
uniform vec4 cloudLayer;
uniform vec2 cloudOriginKm;
uniform sampler3D cloudNoise;

vec2 cloudSphere(float radius, float vertical, float boundary) {
  float b = radius * vertical;
  float discriminant = b*b + (boundary-radius)*(boundary+radius);
  if (discriminant < 0.) return vec2(1., -1.);
  float root = sqrt(discriminant);
  return vec2(-b-root, -b+root);
}

vec2 cloudSpan(Medium air, float radius, float vertical) {
  vec2 span = cloudSphere(radius, vertical, air.BottomRadiusKm + cloudLayer.y);
  vec2 inner = cloudSphere(radius, vertical, air.BottomRadiusKm + cloudLayer.x);
  span.x = max(span.x, 0.);
  if (inner.y > span.x) {
    if (inner.x > span.x) span.y = min(span.y, inner.x);
    else span.x = inner.y;
  }
  vec2 ground = cloudSphere(radius, vertical, air.BottomRadiusKm);
  if (ground.y >= ground.x && ground.x >= 0.) span.y = min(span.y, ground.x);
  return span;
}

float cloudAltitude(float base, float radius, float vertical, float distance) {
  float delta = distance * (2. * radius * vertical + distance);
  return radius - base + delta / (sqrt(radius*radius+delta) + radius);
}

float cloudDensity(vec2 position, float altitude) {
  float z = (altitude - cloudLayer.x) / (cloudLayer.y - cloudLayer.x);
  float profile = smoothstep(0., .18, z) * (1. - smoothstep(.58, 1., z));
  vec2 p = cloudOriginKm + position;
  float weather = textureLod(cloudNoise, vec3(p / 48., .29), 0.).r;
  float shape = textureLod(cloudNoise, vec3(p / 12., altitude / 3.7), 0.).g - .5;
  float erosion = textureLod(cloudNoise, vec3(p / 4.7, altitude / 2.2), 0.).b - .5;
  if (cloudKind == 1) {
    float edge = weather + .54*shape + .18*erosion - cloudLayer.z - .20*z*z;
    return profile * smoothstep(-.01, .125, edge);
  }
  float edge = weather + .24*shape + .065*erosion - cloudLayer.z;
  return profile * mix(.32, .96, smoothstep(-.04, .18, edge));
}

float cloudSun(Medium air, float radius, vec3 sun, vec2 start, int steps) {
  if (sun.z <= 0.) return 0.;
  float vertical = clamp(sun.z + dot(start, sun.xy) / radius, -1., 1.);
  vec2 span = cloudSpan(air, radius, vertical);
  float lengthKm = max(0., span.y-span.x);
  float stride = lengthKm / float(steps);
  float depth = 0.;
  for (int i=0; i<steps; ++i) {
    float distance = span.x + (float(i)+.5)*stride;
    float altitude = cloudAltitude(air.BottomRadiusKm, radius, vertical, distance);
    depth += cloudDensity(start+distance*sun.xy, altitude);
  }
  return exp(-depth*stride*cloudLayer.w);
}

vec3 cloudRadiance(Medium air, float radius, vec3 dir, vec3 sun, vec3 sky,
                   sampler2D transmittance, out float opacity) {
  opacity = 0.;
  vec2 span = cloudSpan(air, radius, dir.z);
  span.y = min(span.y, 95.);
  if (span.y <= span.x) return sky;
  int steps = int(clamp(ceil((span.y-span.x)/.14), 8., 48.));
  float stride = (span.y-span.x)/float(steps);
  float transmission = 1.;
  vec3 light = vec3(0.);
  float forward = .24*pow(max(0.,dot(dir,sun)), 18.);
  vec3 incident = textureLod(transmittance,
      mediumTransmittanceUv(air, MediumLook(air.BottomRadiusKm+cloudLayer.y, sun.z),
                           textureSize(transmittance,0)), 0.).rgb;
  for (int i=0; i<steps && transmission>.003; ++i) {
    float distance = span.x+(float(i)+.5)*stride;
    float altitude = cloudAltitude(air.BottomRadiusKm, radius, dir.z, distance);
    vec2 p = distance * dir.xy;
    float density = cloudDensity(p, altitude);
    if (density <= .001) continue;
    float sunlight = cloudSun(air, air.BottomRadiusKm+altitude, sun, p, 5);
    float inner = (altitude-cloudLayer.x)/(cloudLayer.y-cloudLayer.x);
    vec3 ambient = mix(vec3(.107,.124,.151),vec3(.185,.198,.215),inner);
    vec3 colour = ambient + incident*((.64+forward)*sunlight+.20*sqrt(sunlight));
    float stepT = exp(-density*cloudLayer.w*stride);
    light += transmission*(1.-stepT)*colour;
    transmission *= stepT;
  }
  opacity = 1.-transmission;
  vec3 foreground = segmentTransmission(transmittance, air, radius, dir.z, span.x);
  float distant = 1.-smoothstep(48., 95., span.x);
  return sky + distant*foreground*(light - opacity*sky);
}
