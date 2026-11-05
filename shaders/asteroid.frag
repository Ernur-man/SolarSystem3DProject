#version 330 core
in vec3 vNormal;
in vec3 vWorldPosition;
in vec3 vColor;
in float vHeat;
in float vOpacity;
uniform vec3 lightPosition;
uniform vec3 cameraPosition;
out vec4 FragColor;

void main() {
    if (vOpacity <= 0.001)
        discard;
    vec3 N = normalize(vNormal);
    vec3 L = normalize(lightPosition - vWorldPosition);
    float diffuse = 0.24 + 0.76 * max(dot(N, L), 0.0);
    vec3 viewDirection = normalize(cameraPosition - vWorldPosition);
    float rim = pow(1.0 - max(dot(N, viewDirection), 0.0), 2.0);
    // Close approaches emit orange-white HDR light above 1.0 for bloom to reveal.
    vec3 impactGlow = mix(vec3(1.4, 0.18, 0.015), vec3(3.2, 2.0, 0.72), vHeat) * vHeat;
    vec3 color = (vColor * diffuse + vec3(0.08, 0.06, 0.04) * rim) * vOpacity + impactGlow;
    FragColor = vec4(color, 1.0);
}
