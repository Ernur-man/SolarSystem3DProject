#version 330 core

in vec3 worldPos;
in vec3 normal;

out vec4 FragColor;

uniform vec3 color;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform float emissive;
uniform float opacity;

void main() {
    // Blinn-Phong diffuse plus a 32-power half-vector highlight and ambient fill.
    vec3 N = normalize(normal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(cameraPos - worldPos);
    float d = max(dot(N, L), 0.0);
    float s = pow(max(dot(N, normalize(L + V)), 0.0), 32.0);
    vec3 lit = color * (0.12 + 0.88 * d) + vec3(0.22) * s;
    // opacity fades the body's linear radiance without changing its opaque depth coverage.
    FragColor = vec4((lit + color * emissive) * opacity, 1.0);
}
