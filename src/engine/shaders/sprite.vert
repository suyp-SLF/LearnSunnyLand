#version 450

layout(set = 0, binding = 0) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} pc;

layout(location = 0) out vec2 outUV;

void main() {
    // 标准 [0, 1] 范围的顶点
    vec2 positions[6] = vec2[](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0),
        vec2(0.0, 1.0), vec2(1.0, 0.0), vec2(1.0, 1.0)
    );

    vec2 pos = positions[gl_VertexIndex];
    outUV = pos;
    // ⚡️ 核心：让 MVP 矩阵决定方块的大小和位置
    gl_Position = pc.mvp * vec4(pos, 0.0, 1.0);
}