#version 330 core

out vec4 FragColor;
uniform vec3 color;
uniform float lineAlpha;

void main() {
    // Alpha lets the depth-tested guide lines remain subtle over the HDR scene.
    FragColor = vec4(color, lineAlpha);
}
