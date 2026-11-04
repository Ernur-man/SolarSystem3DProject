#version 330 core

in vec3 worldPos;

out vec4 FragColor;

uniform float time;

float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y),
               f.z);
}

void main() {
    // Four-octave FBM adds animated convection to a hot solar surface.
    vec3 p = normalize(worldPos) * 5.0 + vec3(0, time * 0.08, 0);
    float n = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        n += a * noise(p);
        p *= 2.03;
        a *= 0.5;
    }
    vec3 c = mix(vec3(1.0, 0.18, 0.015), vec3(1.0, 0.83, 0.25), smoothstep(0.24, 0.78, n));
    FragColor = vec4(c * 2.4, 1.0);
}
