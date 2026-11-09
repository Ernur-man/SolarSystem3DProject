#version 330 core
layout(location=0) in vec3 position;
layout(location=1) in float opacity;
uniform mat4 view;
uniform mat4 projection;
out float pinOpacity;
void main() { gl_Position = projection * view * vec4(position, 1.0); gl_PointSize = 9.0; pinOpacity = opacity; }
