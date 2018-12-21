#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D samplerColorMap;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;


#if 0
//https://www.geeks3d.com/20140213/glsl-shader-library-fish-eye-and-dome-and-barrel-distortion-post-processing-filters/2/
vec2 Distort(vec2 p)
{
      float theta  = atan(p.y, p.x);

      float x = p.x;//(2.0 * p.x - 1.0) / 1.0;
      float y = p.y;//(2.0 * p.y - 1.0) / 1.0;
      float radius = length(vec2(x,y)); //sqrt( x*x + y*y);
      radius = pow(radius,3);
      vec2 uv;
      uv.x = radius * cos(theta);
      uv.y = radius * sin(theta);
      return 0.5 * (uv + 1.0);
}

void main() 
{
	vec2 uv;
	vec2 xy = 2.0 * inUV.xy - 1.0;	
	float d = length(xy);
	if (d < 1.0)
	{
	  uv = Distort(xy);
	}
	else
	{
	  uv = inUV.xy;
	}
	//uv = Distort(inUV);

	vec4 color;
	if(uv.x >= 0.0 && uv.x  <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
	    color = texture(samplerColorMap, uv) * vec4(inColor, 1.0);
	else 
	    color = vec4(0.0, 0.0, 0.0, 1.0);
        //color = texture(samplerColorMap, inUV) * vec4(inColor, 1.0);
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), 0.0) * inColor;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4(color.rgb, 1.0);//vec4(diffuse * color.rgb*2.0 + specular*3.0+1.0, 1.0);		
}
#endif
#if 1
//http://marcodiiga.github.io/radial-lens-undistortion-filtering
vec2 Distort2(vec2 pass_TextureCoord) {
	// Normalize the u,v coordinates in the range [-1;+1]
	float x = (2.0 * pass_TextureCoord.x - 1.0) / 1.0;
	float y = (2.0 * pass_TextureCoord.y - 1.0) / 1.0;
	float alphax= 0.08;
	float alphay= 0.08;

	// Calculate l2 norm
	float r = x*x + y*y;

	// Calculate the deflated or inflated new coordinate (reverse transform)
	float x3 = x / (1.0 - alphax * r);
	float y3 = y / (1.0 - alphay * r); 
	float x2 = x / (1.0 - alphax * (x3 * x3 + y3 * y3));
	float y2 = y / (1.0 - alphay * (x3 * x3 + y3 * y3));  

	// Forward transform
	// float x2 = x * (1.0 - alphax * r);
	// float y2 = y * (1.0 - alphay * r);

	// De-normalize to the original range
	float i2 = (x2 + 1.0) * 1.0 / 2.0;
	float j2 = (y2 + 1.0) * 1.0 / 2.0;
	return vec2(i2,j2);
}

void main() 
{
	vec2 uv = Distort2(inUV);
	vec4 color;
	if(uv.x >= 0.0 && uv.x  <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
	color = texture(samplerColorMap, uv) * vec4(inColor, 1.0);
	else 
		color = vec4(0.0, 0.0, 0.0, 1.0);
	// Bypass distortion.
        //color = texture(samplerColorMap, inUV) * vec4(inColor, 1.0);
	vec3 N = normalize(inNormal);
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), 0.0) * inColor;
	vec3 specular = pow(max(dot(R, V), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4(color.rgb, 1.0);//vec4(diffuse * color.rgb*2.0 + specular*3.0+1.0, 1.0);		
}
#endif


