#version 410 core
in vec3 position;
in vec4 offsetAngle;
in float scale;
uniform mat4 viewProjection;
out vec3 point;
flat out vec2 rotation;
void main(){
  float c=cos(offsetAngle.w),s=sin(offsetAngle.w);
  rotation=vec2(c,s);
  vec3 p=position*scale;
  point=vec3(c*p.x-s*p.y,s*p.x+c*p.y,p.z)+offsetAngle.xyz;
  gl_Position=viewProjection*vec4(point,1.);
}
