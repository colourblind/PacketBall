#version 430

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 fragColour;

layout(location = 0) uniform sampler2D pingTexture;
layout(location = 1) uniform float alpha;

uniform vec3 colour;

void main()
{
    const float LOG2 = 1.442695;
    const float DENSITY = 0.5;
    float z = gl_FragCoord.z / gl_FragCoord.w;
    float fog = min(1.0, exp2(-1 * DENSITY * DENSITY * z * z * LOG2) + 0.25);
    fragColour = texture(pingTexture, texCoord) * vec4(colour * fog, alpha);
}
