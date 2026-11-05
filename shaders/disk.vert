#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 localPosition;
out vec3 worldPosition;

void main() {
    // Keep polar coordinates in the disk plane for the fragment shader's rotating noise field.
    localPosition = aPos;
    // model maps ring-local coordinates to world space; view and projection map them to clip space.
    vec4 world = model * vec4(aPos, 1.0);
    worldPosition = world.xyz;
    gl_Position = projection * view * world;
}
