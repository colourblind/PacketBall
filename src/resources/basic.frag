#version 410

layout(location = 0) out vec4 fragColour;

uniform vec3 colour;

void main()
{
    const float LOG2 = 1.442695;
    const float DENSITY = 0.5;
    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = exp2(-1 * DENSITY * DENSITY * z * z * LOG2);
    fragColour = vec4(colour * fog, 1.0);
}
