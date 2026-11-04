#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

out vec3 direction;

void main() {
    direction = aPos;
    vec4 clipPosition = projection * view * model * vec4(aPos, 1.0);
    gl_Position = clipPosition.xyww;
}
