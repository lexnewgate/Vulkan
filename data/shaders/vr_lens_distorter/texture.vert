#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;

layout (binding = 0) uniform UBO 
{
  mat4 projection;
  mat4 model;
  vec4 lightPos;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;

void DistortImageDiff(inout vec4 p)
{
  vec2 pNDC = p.xy / p.w;
  // Convert to polar coords:
  float radius = length(pNDC);
  if (radius == 0)
    return;

  float theta = atan(pNDC.y,pNDC.x);
    
  float radiusF = radius/1.68;
  radius = radius-(0.24*pow(radiusF,4)+0.22*pow(radiusF,2));
  // Convert back to Cartesian:
  pNDC.x = radius * cos(theta);
  pNDC.y = radius * sin(theta);
  p.xy = pNDC.xy * p.w;
}

// http://marcodiiga.github.io/radial-lens-undistortion-filtering
// Works in both vertex and fragment.
void DistortImageRatio(inout vec4 p)
{
  p.xy = p.xy / p.w;
  // Convert to polar coords:
  // Calculate r*r.
  float rr = p.x*p.x + p.y*p.y;
  float alphax = 0.15;
  float alphay = 0.15;

  // Calculate the deflated or inflated new coordinate (reverse transform)
  p.x = p.x / (1.0 + alphax * rr);
  p.y = p.y / (1.0 + alphay * rr);
  p.xy = p.xy*p.w;
}


out gl_PerVertex
{
  vec4 gl_Position;
};

void main() 
{
  outNormal = inNormal;
  outColor = inColor;
  outUV = inUV;
  gl_Position = ubo.projection * ubo.model * vec4(inPos.xyz, 1.0);
  //DistortImageRatio(gl_Position);
  
  vec4 pos = ubo.model * vec4(inPos, 1.0);
  outNormal = mat3(ubo.model) * inNormal;
  vec3 lPos = mat3(ubo.model) * ubo.lightPos.xyz;
  outLightVec = lPos - pos.xyz;
  outViewVec = -pos.xyz;  
}
