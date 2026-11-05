#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 holeCenter;
uniform float horizonRadius;
uniform float effectStrength;
uniform vec2 resolution;

void main() {
    vec3 originalColor = texture(sceneTexture, uv).rgb;
    vec4 holeClip = projection * view * vec4(holeCenter, 1.0);
    if (effectStrength <= 0.0 || holeClip.w <= 0.0) {
        FragColor = vec4(originalColor, 1.0);
        return;
    }

    // Perspective divide maps clip coordinates to NDC; the half-scale and bias map NDC to UV.
    vec3 holeNdc = holeClip.xyz / holeClip.w;
    vec2 holeUV = holeNdc.xy * 0.5 + 0.5;
    float aspect = resolution.x / max(resolution.y, 1.0);

    // Aspect-correct horizontal UV distance so r is measured in screen-height units.
    vec2 delta = uv - holeUV;
    delta.x *= aspect;
    float r = length(delta);
    float horizonScreenRadius = abs(projection[1][1] * horizonRadius / holeClip.w) * 0.5;

    // Inverse-radius deflection is bounded to prevent extreme UV jumps at the singular center.
    float deflection = clamp(0.06 * horizonScreenRadius / max(r, 0.00001), 0.0, 0.045);
    vec2 direction = delta / max(r, 0.00001);
    vec2 sampleOffset = vec2(direction.x / aspect, direction.y) * deflection;
    vec2 sampleUV = clamp(uv - sampleOffset, vec2(0.001), vec2(0.999));

    // Fade smoothly by radius, viewport proximity, clip depth, and distance outside the viewport.
    float distanceFade = 1.0 - smoothstep(max(horizonScreenRadius * 1.2, 0.001),
                                         max(horizonScreenRadius * 18.0, 0.22), r);
    float outside = max(max(abs(holeNdc.x) - 1.0, abs(holeNdc.y) - 1.0), 0.0);
    float offscreenFade = 1.0 - smoothstep(0.0, 0.35, outside);
    float borderDistance = min(min(holeUV.x, 1.0 - holeUV.x),
                               min(holeUV.y, 1.0 - holeUV.y));
    float borderFade = smoothstep(0.0, max(horizonScreenRadius * 2.0, 0.035), borderDistance);
    float depthFade = 1.0 - smoothstep(1.0, 1.15, abs(holeNdc.z));
    float effectFade = effectStrength * distanceFade * offscreenFade * borderFade * depthFade;

    vec3 bentColor = texture(sceneTexture, sampleUV).rgb;
    vec3 result = mix(originalColor, bentColor, effectFade);
    // The event horizon covers its projected disk in black, with a soft edge during transitions.
    float horizonMask = 1.0 - smoothstep(horizonScreenRadius * 0.98,
                                         horizonScreenRadius * 1.02, r);
    result = mix(result, vec3(0.0), horizonMask * effectFade);
    FragColor = vec4(result, 1.0);
}
