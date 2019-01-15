#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 1) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;
layout (location = 1) in float inLodBias;

layout (location = 0) out vec4 outFragColor;


void main() 
{
    vec4 color = texture(samplerColor, inUV, inLodBias);

    //outFragColor = vec4(diffuse * color.rgb + specular, 1.0);
    float scale = 1.0;
    float offset = 0.0; // texture coordinate doesnot care this offset at all. Can be 0.0, -5.0,5.0. The result are the same.
    //outFragColor = texture(samplerColor, vec2(inUV.s*scale+offset, scale*(1.0 - inUV.t)+offset));
    outFragColor = texture(samplerColor, inUV);
}
