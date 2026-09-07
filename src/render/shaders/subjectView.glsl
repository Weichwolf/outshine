layout(std140, set = 1, binding = 0) uniform SubjectView { mat4 viewProj; mat4 prevViewProj; mat4 lightFromWorld; vec4 shift; vec4 prevShift; } s;
