#version 430

layout(location = 0) out vec4 fragColour;

uniform vec3 colour;
layout(location = 0) uniform float age;

void main()
{
    const float LOG2 = 1.442695;
    const float DENSITY = 0.5;
    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = min(1.0, exp2(-1 * DENSITY * DENSITY * z * z * LOG2) + 0.25);
    fragColour = vec4(colour * fog, age);
}
