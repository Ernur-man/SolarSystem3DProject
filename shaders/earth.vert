#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 worldPos;
out vec3 localPos;
out vec3 normal;
out vec2 uv;

void main() {
    vec4 world = model * vec4(aPos, 1.0);
    worldPos = world.xyz;
    localPos = aPos;
    normal = mat3(transpose(inverse(model))) * aNormal;
    // The sphere mesh starts v at its north pole; flip v for top-origin lat-long images.
    uv = vec2(aUV.x, 1.0 - aUV.y);
    gl_Position = projection * view * world;
}
