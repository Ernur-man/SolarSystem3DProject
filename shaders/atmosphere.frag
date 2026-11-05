#version 330 core

in vec3 worldPos;
in vec3 worldNormal;
out vec4 FragColor;

uniform vec3 cameraPos;
uniform float opacity;

void main() {
    vec3 N = normalize(worldNormal);
    vec3 V = normalize(cameraPos - worldPos);
    // Fresnel rim intensity grows as the surface normal turns away from the view direction.
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);
    vec3 blueAtmosphere = vec3(0.12, 0.42, 1.0);
    FragColor = vec4(blueAtmosphere * rim * 1.8 * opacity, 1.0);
}
