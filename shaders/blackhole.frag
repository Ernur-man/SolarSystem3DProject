#version 330 core

out vec4 FragColor;

void main() {
    // The event horizon absorbs all incoming light, so the unlit surface is pure black.
    FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
