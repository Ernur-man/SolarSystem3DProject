#version 330 core
in vec4 color;
in float edge;
out vec4 FragColor;
void main() {
    float softEdge = 1.0 - smoothstep(0.45, 1.0, abs(edge));
    FragColor = vec4(color.rgb, color.a * softEdge);
}
