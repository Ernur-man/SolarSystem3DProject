#version 330 core

in vec2 uv;
out vec4 FragColor;

uniform sampler2D sceneTexture;
uniform float threshold;
uniform float softKnee;

void main() {
    vec3 hdrColor = texture(sceneTexture, uv).rgb;
    // Rec. 709 luminance estimates brightness in linear RGB.
    float brightness = dot(hdrColor, vec3(0.2126, 0.7152, 0.0722));

    // The quadratic soft knee blends smoothly into the hard threshold over a 2*knee interval.
    float soft = clamp(brightness - threshold + softKnee, 0.0, 2.0 * softKnee);
    soft = soft * soft / (4.0 * softKnee + 0.00001);
    float contribution = max(brightness - threshold, soft) / max(brightness, 0.00001);
    FragColor = vec4(hdrColor * contribution, 1.0);
}
