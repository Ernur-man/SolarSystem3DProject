#version 330 core

in vec3 worldPos;
in vec3 normal;

out vec4 FragColor;

uniform vec3 color;
uniform vec3 lightPos;
uniform vec3 cameraPos;
uniform float emissive;
uniform float opacity;
uniform bool ringEnabled;
uniform vec3 ringCenter;
uniform vec3 ringNormal;
uniform float ringInnerRadius;
uniform float ringOuterRadius;

void main() {
    // Blinn-Phong diffuse plus a 32-power half-vector highlight and ambient fill.
    vec3 N = normalize(normal);
    vec3 L = normalize(lightPos - worldPos);
    vec3 V = normalize(cameraPos - worldPos);
    float d = max(dot(N, L), 0.0);
    float s = pow(max(dot(N, normalize(L + V)), 0.0), 32.0);
    float ringVisibility = 1.0;
    if (ringEnabled) {
        float denominator = dot(L, normalize(ringNormal));
        if (abs(denominator) > 0.0001) {
            // Intersect the Sun ray with Saturn's ring plane, then test its radial interval.
            float t = dot(ringCenter - worldPos, normalize(ringNormal)) / denominator;
            if (t > 0.0 && t < length(lightPos - worldPos)) {
                vec3 intersection = worldPos + L * t;
                float radius = length(intersection - ringCenter -
                                      normalize(ringNormal) *
                                          dot(intersection - ringCenter, normalize(ringNormal)));
                if (radius >= ringInnerRadius && radius <= ringOuterRadius)
                    ringVisibility = 0.22;
            }
        }
    }
    vec3 lit = color * (0.12 + 0.88 * d * ringVisibility) +
               vec3(0.22) * s * ringVisibility;
    // opacity fades the body's linear radiance without changing its opaque depth coverage.
    FragColor = vec4((lit + color * emissive) * opacity, 1.0);
}
