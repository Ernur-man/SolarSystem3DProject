#version 330 core
in float pinOpacity;
out vec4 FragColor;
void main() {
    vec2 p = gl_PointCoord * 2.0 - 1.0;
    float r = dot(p, p);
    if (r > 1.0) discard;
    float glow = exp(-r * 5.0);
    vec3 orangeWhite = mix(vec3(1.0, 0.38, 0.04), vec3(1.5, 1.25, 0.72), 1.0 - smoothstep(0.0, 0.25, r));
    FragColor = vec4(orangeWhite * glow, pinOpacity * glow);
}
