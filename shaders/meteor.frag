#version 330 core
in vec2 vUv;
in float vTail;
in float vOpacity;
in float vHeat;
uniform vec3 color;
out vec4 FragColor;

void main() {
    if (vOpacity <= 0.001)
        discard;
    float alpha = 1.0;
    if (vTail > 0.5) {
        // Fade the tail at its rear and edges so additive blending makes a tapered streak.
        float rearFade = smoothstep(0.0, 0.26, vUv.y);
        float headFade = 1.0 - smoothstep(0.80, 1.0, vUv.y);
        float edgeFade = 1.0 - smoothstep(0.35, 0.5, abs(vUv.x - 0.5));
        alpha = rearFade * headFade * edgeFade * 0.72;
    }
    vec3 heatGlow = mix(vec3(1.4, 0.18, 0.015), vec3(3.2, 2.0, 0.72), vHeat) * vHeat;
    vec3 emission = color * (vTail > 0.5 ? 2.2 : 3.0) * vOpacity + heatGlow;
    FragColor = vec4(emission, alpha * vOpacity);
}
