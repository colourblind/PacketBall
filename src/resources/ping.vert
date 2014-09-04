#version 430

layout(location = 0) in vec3 vertex;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 texCoordOut;

uniform mat4 m;
uniform mat4 v;
uniform mat4 p;

void main()
{
    gl_Position = p * v * m * transpose(mat4(mat3(v))) * vec4(vertex, 1.0);
    texCoordOut = texCoord;
}
