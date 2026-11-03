#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 worldPos;
out vec3 normal;

void main() {
    // Inverse-transpose keeps normals correct under nonuniform model scaling.
    vec4 world = model * vec4(aPos, 1.0);
    worldPos = world.xyz;
    normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * world;
}
