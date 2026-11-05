#version 330 core

in vec3 worldPos;
in vec2 ringPosition;
out vec4 FragColor;

uniform sampler2D ringTexture;
uniform bool hasRingTexture;
uniform vec3 lightPos;
uniform vec3 saturnCenter;
uniform vec3 ringNormal;
uniform float saturnRadius;
uniform float innerRadius;
uniform float outerRadius;
uniform float opacity;

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float valueNoise2D(vec2 p) {
    vec2 cell = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash12(cell);
    float b = hash12(cell + vec2(1.0, 0.0));
    float c = hash12(cell + vec2(0.0, 1.0));
    float d = hash12(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    float radius = length(ringPosition);
    float radialUv = clamp((radius - innerRadius) / (outerRadius - innerRadius), 0.0, 1.0);
    vec3 ringColor;
    float alpha;
    if (hasRingTexture) {
        vec4 texel = texture(ringTexture, vec2(radialUv, 0.5));
        ringColor = texel.rgb;
        alpha = texel.a;
    } else {
        // Repeated radial bands are perturbed by 2D value noise and cut by translucent gaps.
        float n = valueNoise2D(ringPosition * 13.0);
        float bands = 0.5 + 0.5 * sin(radius * 118.0 + n * 8.0);
        float gaps = smoothstep(0.28, 0.48, bands + (n - 0.5) * 0.24);
        float innerFade = smoothstep(innerRadius, innerRadius + 0.07, radius);
        float outerFade = 1.0 - smoothstep(outerRadius - 0.06, outerRadius, radius);
        ringColor = mix(vec3(0.28, 0.19, 0.12), vec3(0.78, 0.66, 0.47),
                        smoothstep(0.25, 0.78, bands + n * 0.18));
        alpha = gaps * innerFade * outerFade * 0.78;
    }

    vec3 toSun = lightPos - worldPos;
    float sunDistance = length(toSun);
    vec3 rayDirection = toSun / max(sunDistance, 0.00001);
    vec3 offset = worldPos - saturnCenter;
    // Ray-sphere test: solve t^2 + 2*b*t + c = 0 and shadow if the near hit is before the Sun.
    float b = dot(offset, rayDirection);
    float c = dot(offset, offset) - saturnRadius * saturnRadius;
    float discriminant = b * b - c;
    float hitDistance = -b - sqrt(max(discriminant, 0.0));
    float planetShadow = discriminant >= 0.0 && hitDistance > 0.0 && hitDistance < sunDistance
                             ? 0.22
                             : 1.0;
    float radialLight = 0.70 + 0.30 * abs(dot(normalize(ringNormal), rayDirection));
    FragColor = vec4(ringColor * radialLight * planetShadow * opacity, alpha * opacity);
}
