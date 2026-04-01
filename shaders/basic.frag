#version 330 core
out vec4 FragColor;

uniform vec4 color;
uniform float isPointObject; // 1.0 if body point, 0.0 if line
uniform sampler2D lensingMaskTexture;
uniform vec2 viewportSize;
uniform float useLensingMask;

float lensingVisibility() {
    if (useLensingMask < 0.5) {
        return 1.0;
    }

    vec2 safeViewport = max(viewportSize, vec2(1.0));
    vec2 uv = gl_FragCoord.xy / safeViewport;
    float maskAlpha = texture(lensingMaskTexture, uv).a;

    // Occlude only in high-alpha regions (disk/horizon), ignore faint star alpha.
    float occlusion = smoothstep(0.7, 0.98, maskAlpha);
    return 1.0 - occlusion;
}

void main() {
    float visibility = lensingVisibility();

    if (isPointObject > 0.5) {
        // Discard fragments outside a circle
        vec2 coord = gl_PointCoord - vec2(0.5);
        if (length(coord) > 0.5) {
            discard;
        }
        
        // simple 3D lighting pseudo-effect based on circle radius
        float dist = length(coord) * 2.0; // 0 to 1
        float nz = sqrt(1.0 - dist*dist); // pseudo-normal z
        vec3 normal = vec3(coord.x*2.0, coord.y*2.0, nz);
        vec3 lightDir = normalize(vec3(0.5, 0.5, 1.0));
        float diff = max(dot(normal, lightDir), 0.3); // ambient 0.3

        FragColor = vec4(color.rgb * diff * visibility, color.a * visibility);
    } else {
        FragColor = vec4(color.rgb * visibility, color.a * visibility);
    }
}
