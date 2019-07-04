#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(binding = 1) uniform sampler2D samplerColorMap;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inViewVec;
layout(location = 4) in vec3 inLightVec;

layout(location = 0) out vec4 outFragColor;

// Based on image difference.
vec2 DistortImageDiff(vec2 uv) {
  // Re-centered the u,v coordinates at (0, 0), in the range [-1.0, 1.0].
  float x = 2.0 * uv.x - 1.0;
  float y = 2.0 * uv.y - 1.0;
  // Convert to polar coords.
  float radius = length(vec2(x, y));
  if (radius == 0)
    return uv;
  float theta = atan(y, x);
  radius = radius + (0.24 * pow(radius, 4) + 0.22 * pow(radius, 2));
  // Convert back to Cartesian:
  x = radius * cos(theta);
  y = radius * sin(theta);

  // Re-centered at (0.5, 0.5), in the range [0.0, 1.0].
  // De-normalize to the original range
  float u = (x + 1.0) / 2.0;
  float v = (y + 1.0) / 2.0;
  return vec2(u, v);
}

// http://marcodiiga.github.io/radial-lens-undistortion-filtering
// Works in both vertex and fragment.
// Based on image ratio.
vec2 DistortImageRatio(vec2 uv) {
  // Normalize the u,v coordinates in the range [-1;+1]
  float x = 2.0 * uv.x - 1.0;
  float y = 2.0 * uv.y - 1.0;
  float alphax = 0.25;
  float alphay = 0.25;

  // Calculate r*r norm
  float rr = x * x + y * y;

  // Calculate the deflated or inflated new coordinate (reverse transform)
  float xAntiDistorted = x / (1.0 - alphax * rr);
  float yAntiDistorted = y / (1.0 - alphay * rr);

  // De-normalize to the original range
  float u = (xAntiDistorted + 1.0) / 2.0;
  float v = (yAntiDistorted + 1.0) / 2.0;
  return vec2(u, v);
}

void main() {
  vec2 uv = DistortImageRatio(inUV);
  vec4 color;
  if (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
    color = texture(samplerColorMap, uv) * vec4(inColor, 1.0);
  else
    color = vec4(0.0, 0.0, 0.0, 1.0);
  vec3 N = normalize(inNormal);
  vec3 L = normalize(inLightVec);
  vec3 V = normalize(inViewVec);
  vec3 R = reflect(-L, N);
  vec3 diffuse = max(dot(N, L), 0.0) * inColor;
  vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
  // outFragColor =  vec4(diffuse * color.rgb*2.0 + specular*3.0+1.0, 1.0);
  outFragColor = vec4(color.rgb, 1.0);
}
