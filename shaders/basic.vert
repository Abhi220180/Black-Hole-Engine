#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 offset;
uniform float scale; // Acts as radius for bodies

void main() {
    // If drawing a particle, scale>0, and offset is its world position, aPos is usually just (0,0,0) as we use Points
    // If drawing a line/trail/grid, offset is (0,0,0) and scale is 0.
    
    vec4 worldPos;
    if (scale > 0.0) {
        worldPos = vec4(offset, 1.0);
    } else {
        worldPos = vec4(aPos, 1.0);
    }

    vec4 viewPos = view * worldPos;
    gl_Position = projection * viewPos;

    // Point size perspective division
    if (scale > 0.0) {
        // approximate point size based on distance
        float dist = length(viewPos.xyz);
        gl_PointSize = max((scale * 800.0) / dist, 2.0);
    } else {
        gl_PointSize = 1.0;
    }
}
