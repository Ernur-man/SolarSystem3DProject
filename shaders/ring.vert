#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 worldPos;
out vec2 ringPosition;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    worldPos = world.xyz;
    ringPosition = aUV;
    gl_Position = projection * view * world;
}
