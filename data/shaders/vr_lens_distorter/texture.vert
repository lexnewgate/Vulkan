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
//pr*(0.24*Math.pow(sf,4)+0.22*Math.pow(sf,2)+1);
vec4 Distort(vec4 p)
{
    vec2 v = p.xy / p.w;
    // Convert to polar coords:
    float radius = length(v);
    if (radius > 0)
    {
      float theta = atan(v.y,v.x);
      
      // Distort:
      //1), https://www.geeks3d.com/20140213/glsl-shader-library-fish-eye-and-dome-and-barrel-distortion-post-processing-filters/2/
      //radius = pow(radius, 0.8);
      //2),  http://jsfiddle.net/s175ozts/4/, https://stackoverflow.com/questions/28130618/what-ist-the-correct-oculus-rift-barrel-distortion-radius-function
      /*

var rMax = Math.sqrt(Math.pow(xmid,2)+Math.pow(ymid,2));
      var pr = Math.sqrt(Math.pow(xmid-x,2)+Math.pow(ymid-y,2)); //radius from pixel to pic mid
      var sf = pr / rMax; //Scaling factor
      var newR = pr*(0.24*Math.pow(sf,4)+0.22*Math.pow(sf,2)+1); //barrel distortion function
	  */
      radius = radius/(0.24*pow(radius,4)+0.22*pow(radius,2)+1);

	
      //var rMax = Math.sqrt(Math.pow(xmid,2)+Math.pow(ymid,2));
      // Convert back to Cartesian:
      v.x = radius * cos(theta);
      v.y = radius * sin(theta);
      p.xy = v.xy * p.w;
    }
    return p;
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
	gl_Position = Distort(gl_Position);
	
	vec4 pos = ubo.model * vec4(inPos, 1.0);
	outNormal = mat3(ubo.model) * inNormal;
	vec3 lPos = mat3(ubo.model) * ubo.lightPos.xyz;
	outLightVec = lPos - pos.xyz;
	outViewVec = -pos.xyz;		
}