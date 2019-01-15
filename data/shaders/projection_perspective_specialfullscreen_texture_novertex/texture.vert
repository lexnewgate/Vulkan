#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout (binding = 0) uniform UBO 
{
    mat4 projection;
    mat4 model;
    vec4 viewPos;
    float lodBias;
} ubo;

layout (location = 0) out vec2 outUV;
layout (location = 1) out float outLodBias;

out gl_PerVertex 
{
    vec4 gl_Position;   
};


void main() 
{
    //outUV = inUV;
    outLodBias = ubo.lodBias;

    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
    gl_Position.x = gl_Position.x/3.0f;
    gl_Position.y = gl_Position.y/3.0f;
}
