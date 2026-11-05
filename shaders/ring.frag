#version 330 core

in vec3 worldPos;
in vec2 ringPosition;
out vec4 FragColor;

uniform sampler2D ringTexture;
uniform bool hasRingTexture;
uniform vec3 lightPos;
uniform vec3 sunLightPos;
uniform vec3 saturnCenter;
uniform vec3 ringNormal;
uniform float saturnRadius;
uniform float innerRadius;
uniform float outerRadius;
uniform float opacity;
uniform float ringAngle;

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

float planetShadowFrom(vec3 lightPosition) {
    vec3 toLight = lightPosition - worldPos;
    float distanceToLight = length(toLight);
    vec3 direction = toLight / max(distanceToLight, 0.00001);
    vec3 offset = worldPos - saturnCenter;
    float b = dot(offset, direction);
    float c = dot(offset, offset) - saturnRadius * saturnRadius;
    float discriminant = b * b - c;
    float hitDistance = -b - sqrt(max(discriminant, 0.0));
    return discriminant >= 0.0 && hitDistance > 0.0 && hitDistance < distanceToLight
               ? 0.22
               : 1.0;
}

void main() {
    float radius = length(ringPosition);
    float radialUv = clamp((radius - innerRadius) / (outerRadius - innerRadius), 0.0, 1.0);
    // The mesh has local XY coordinates and +Z normal. Compensate for global mesh spin
    // so the visible angular streak moves at the desired radius-dependent Kepler speed.
    float speedFactor = clamp(pow(innerRadius / max(radius, innerRadius), 1.5), 0.32, 1.0);
    float animatedAngle = atan(ringPosition.y, ringPosition.x) +
                          ringAngle * (1.0 - speedFactor);
    vec2 animatedPolar = vec2(cos(animatedAngle), sin(animatedAngle));
    float angularNoise = valueNoise2D(animatedPolar * (18.0 + radius * 4.0) +
                                      vec2(radius * 3.0, -radius * 1.7));
    float streakWave = 0.5 + 0.5 * sin(animatedAngle * 23.0 + radius * 12.0 +
                                       angularNoise * 7.0);
    float angularStreak = smoothstep(0.66, 0.90, angularNoise + streakWave * 0.14);
    vec3 ringColor;
    float alpha;
    if (hasRingTexture) {
        vec4 texel = texture(ringTexture, vec2(radialUv, 0.5));
        ringColor = texel.rgb;
        alpha = texel.a;
        // Angular dust/streak contrast remains visible over a purely radial supplied map.
        ringColor *= mix(0.58, 1.16, angularStreak);
    } else {
        // Radial bands get time-varying angular noise and clumps, unlike a static radial-only ring.
        float n = valueNoise2D(ringPosition * 13.0) * 0.40 + angularNoise * 0.60;
        float bands = 0.5 + 0.5 * sin(radius * 118.0 + n * 8.0);
        float gaps = smoothstep(0.28, 0.48, bands + (n - 0.5) * 0.24);
        float innerFade = smoothstep(innerRadius, innerRadius + 0.07, radius);
        float outerFade = 1.0 - smoothstep(outerRadius - 0.06, outerRadius, radius);
        ringColor = mix(vec3(0.28, 0.19, 0.12), vec3(0.78, 0.66, 0.47),
                        smoothstep(0.25, 0.78, bands + n * 0.18));
        ringColor *= mix(0.54, 1.22, angularStreak);
        alpha = gaps * innerFade * outerFade * 0.78;
    }

    vec3 sunDirection = normalize(sunLightPos - worldPos);
    vec3 movableDirection = normalize(lightPos - worldPos);
    float sunRadial = 0.70 + 0.30 * abs(dot(normalize(ringNormal), sunDirection));
    float movableRadial = 0.70 + 0.30 * abs(dot(normalize(ringNormal), movableDirection));
    float sunLight = sunRadial * planetShadowFrom(sunLightPos);
    float movableLight = movableRadial * planetShadowFrom(lightPos);
    FragColor = vec4(ringColor * (0.10 + 0.48 * sunLight + 0.42 * movableLight) * opacity,
                     alpha * opacity);
}
