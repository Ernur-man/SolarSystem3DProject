#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D sceneTex;
uniform sampler2D bloomTex;
uniform bool bloomOn;
uniform bool lensingOn;
uniform vec2 holeUV;

void main() {
    // Stylized gravitational lensing radially offsets UVs with inverse-distance deflection.
    vec2 delta = uv - holeUV;
    float r = length(delta);
    vec2 bent = uv;
    if (lensingOn && r > 0.015) {
        bent -= normalize(delta) * min(0.018 / (r + 0.035), 0.12);
    }
    vec3 c = texture(sceneTex, bent).rgb;
    if (bloomOn) {
        c += texture(bloomTex, bent).rgb;
    }
    // Reinhard tone map followed by display gamma conversion.
    c = c / (c + vec3(1.0));
    c = pow(c, vec3(1.0 / 2.2));
    FragColor = vec4(c, 1.0);
}
