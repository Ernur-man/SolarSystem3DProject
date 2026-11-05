#version 330 core

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;

out vec2 uv;

void main() {
    // The quad positions are already in clip space, and their paired UVs cover the textures once.
    uv = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
