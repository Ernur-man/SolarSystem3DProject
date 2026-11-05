#version 330 core

in vec3 localPosition;
in vec3 worldPosition;
out vec4 FragColor;

uniform mat4 model;
uniform vec3 cameraPos;
uniform float time;
uniform float opacity;
uniform float innerRadius;
uniform float outerRadius;

float hash31(vec3 p) {
    // A sine-free polynomial hash returns a repeatable pseudo-random value for each lattice cell.
    p = fract(p * 0.1031);
    p += dot(p, p.yxz + vec3(33.33));
    return fract((p.x + p.y) * p.z);
}

float valueNoise3(vec3 p) {
    vec3 cell = floor(p);
    vec3 f = fract(p);
    // Cubic interpolation smooths the hash values between neighboring lattice points.
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(cell + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(cell + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(cell + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(cell + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(cell + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(cell + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(cell + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(cell + vec3(1.0, 1.0, 1.0));

    float z0 = mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
                   mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
    return z0;
}

float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int octave = 0; octave < 4; ++octave) {
        value += amplitude * valueNoise3(p);
        p = p * 2.03 + vec3(7.1, 13.7, 5.3);
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    float radius = length(localPosition.xy);
    // Map the physical mesh radius to a unit interval for speed, color, and edge fades.
    float radial = clamp((radius - innerRadius) / (outerRadius - innerRadius), 0.0, 1.0);
    float angle = atan(localPosition.y, localPosition.x);

    // Angular velocity rises inward; rotating polar coordinates make the gas spiral faster near Rs.
    float angularSpeed = mix(0.8, 4.0, 1.0 - radial);
    float spiralAngle = angle + time * angularSpeed;
    vec3 polarNoisePosition = vec3(cos(spiralAngle) * radius * 2.5,
                                   sin(spiralAngle) * radius * 2.5,
                                   radial * 3.0 + time * 0.12);
    float turbulence = fbm(polarNoisePosition);
    float filaments = mix(0.55, 1.45, turbulence);

    // HDR white-hot inner gas blends to orange and then dark red toward the outer edge.
    vec3 color = mix(vec3(2.8, 2.15, 1.25), vec3(1.65, 0.30, 0.03),
                     smoothstep(0.0, 0.48, radial));
    color = mix(color, vec3(0.25, 0.012, 0.004), smoothstep(0.45, 1.0, radial));

    // Circular motion is tangent to the ring; the camera-facing velocity side gets brighter.
    vec3 diskVelocityDir = normalize(mat3(model) * vec3(-sin(angle), cos(angle), 0.0));
    vec3 viewDir = normalize(cameraPos - worldPosition);
    float dopplerBrightness = 1.0 + 0.85 * dot(diskVelocityDir, viewDir);
    color *= filaments * dopplerBrightness;

    // Independent inner and outer smoothsteps fade the two radial edges to transparent.
    float innerFade = smoothstep(0.0, 0.10, radial);
    float outerFade = 1.0 - smoothstep(0.82, 1.0, radial);
    float alpha = opacity * innerFade * outerFade;
    FragColor = vec4(color, alpha);
}
