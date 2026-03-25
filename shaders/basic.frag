#version 330 core
out vec4 FragColor;

uniform vec4 color;
uniform float isPointObject; // 1.0 if body point, 0.0 if line

void main() {
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
        
        FragColor = vec4(color.rgb * diff, color.a);
    } else {
        FragColor = color;
    }
}
