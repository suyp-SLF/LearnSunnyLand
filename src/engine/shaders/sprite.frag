#version 450
layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform sampler2D texSampler;

layout(push_constant) uniform Constants {
    mat4 mvp;
    vec4 color;
    vec4 uv_rect;
} pc;

void main() {
    vec4 texColor = texture(texSampler, inUV);
    if (texColor.g == 0.0 && texColor.b == 0.0 && texColor.a == 1.0) {
        outColor = vec4(pc.color.rgb, texColor.r * pc.color.a);
    } else {
        outColor = texColor;
    }
}