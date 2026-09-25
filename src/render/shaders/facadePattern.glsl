float facadeBand(float coordinate, float low, float high, float width) {
  return smoothstep(low - width, low + width, coordinate) *
         (1.0 - smoothstep(high - width, high + width, coordinate));
}

void applyFacade(vec2 encoded, inout vec3 albedo, inout float roughness,
                 inout float metalness) {
  if (encoded.x < 0.0) {
    float trim = mod(-encoded.x - 1.0, 16.0);
    if (trim > 7.5 && trim < 8.5) {
      albedo *= vec3(0.68, 0.70, 0.72);
      roughness = max(roughness, 0.91);
    }
    return;
  }

  float group = floor(encoded.x / 256.0);
  float style = mod(group, 8.0);
  float standing = floor(group / 8.0);
  float bay = encoded.x - group * 256.0;
  float ident = floor(encoded.y / 64.0);
  float storey = encoded.y - ident * 64.0 - 1.0;
  float bayPhase = fract(bay);
  float floorPhase = fract(storey);
  float footprint = max(fwidth(bay), fwidth(storey));
  float detail = 1.0 - smoothstep(0.45, 1.25, footprint);
  float edge = clamp(0.5 * footprint, 0.002, 0.15);
  bool hall = style > 3.5 && style < 4.5;

  float joint = facadeBand(floorPhase, 0.015, 0.040, edge);
  albedo *= mix(vec3(0.98), vec3(0.91, 0.92, 0.93), joint * detail);

  float windowLow = hall ? 0.49 : 0.38;
  float windowHigh = hall ? 0.90 : 0.84;
  float outer = facadeBand(bayPhase, 0.12, 0.88, edge) *
                facadeBand(floorPhase, windowLow - 0.04, windowHigh + 0.04, edge);
  float inner = facadeBand(bayPhase, 0.18, 0.82, edge) *
                facadeBand(floorPhase, windowLow, windowHigh, edge);
  float entrance = standing > 1.5 && storey >= -0.05 && storey < 1.0
      ? facadeBand(bayPhase, 0.14, 0.86, edge) *
        facadeBand(floorPhase, 0.04, 0.91, edge)
      : 0.0;
  float window = inner * (1.0 - entrance) * detail;
  float frame = max(outer - inner, 0.0) * (1.0 - entrance) * detail;
  float door = entrance * detail;

  vec3 glass = albedo * vec3(0.22, 0.30, 0.39);
  vec3 metal = albedo * vec3(0.58, 0.62, 0.65);
  vec3 doorway = albedo * vec3(0.31, 0.36, 0.39);
  albedo = mix(albedo, glass, window);
  albedo = mix(albedo, metal, frame);
  albedo = mix(albedo, doorway, door);
  roughness = mix(roughness, 0.26, window);
  roughness = mix(roughness, 0.48, frame);
  metalness = mix(metalness, 0.42, frame);
}
