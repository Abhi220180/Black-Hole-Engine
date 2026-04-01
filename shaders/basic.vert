#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 offset;
uniform float scale; // Acts as radius for bodies
uniform float pointSizeScale;
uniform float pointMinSize;
uniform float lensingEnabled;
uniform vec2 lensCenterNDC;
uniform float lensStrength;
uniform float lensEpsilon;

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
    vec4 clipPos = projection * viewPos;

    // Simple screen-space warp so raster particles/trails participate in BH distortion.
    if (lensingEnabled > 0.5 && abs(clipPos.w) > 1e-5) {
        vec2 ndc = clipPos.xy / clipPos.w;
        vec2 delta = ndc - lensCenterNDC;
        float r = length(delta);

        if (r > 1e-5) {
            float warp = lensStrength / (r + lensEpsilon);
            warp = min(warp, 0.25);
            ndc += normalize(delta) * warp;
            clipPos.xy = ndc * clipPos.w;
        }
    }

    gl_Position = clipPos;

    // Point size perspective division
    if (scale > 0.0) {
        // approximate point size based on distance
        float dist = max(length(viewPos.xyz), 0.001);
        gl_PointSize = max((scale * pointSizeScale) / dist, pointMinSize);
    } else {
        gl_PointSize = 1.0;
    }
}
