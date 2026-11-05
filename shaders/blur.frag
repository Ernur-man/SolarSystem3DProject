#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D image;
uniform vec2 texelSize;
uniform bool horizontal;

void main() {
    vec2 axis = horizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);

    // These normalized five-tap Gaussian weights are exp(-x*x/2) for x=0,1,2.
    vec3 weights = vec3(0.40262, 0.24420, 0.05449);
    vec3 result = texture(image, uv).rgb * weights.x;
    result += texture(image, uv + axis).rgb * weights.y;
    result += texture(image, uv - axis).rgb * weights.y;
    result += texture(image, uv + axis * 2.0).rgb * weights.z;
    result += texture(image, uv - axis * 2.0).rgb * weights.z;
    FragColor = vec4(result, 1.0);
}
