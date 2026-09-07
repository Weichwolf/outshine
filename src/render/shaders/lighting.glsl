struct Light { vec4 tint; vec4 place; vec4 beam; vec4 cone; };
struct Lights { vec4 count; vec4 environment; vec4 bounced; vec4 up; vec4 skyGround; vec4 viewPosition; Light items[16]; };
layout(std140, set = 3, binding = 1) uniform Lighting { Lights lights; };
